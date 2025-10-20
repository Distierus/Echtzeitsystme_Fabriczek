
// standard includes for the unit test framework
#include <stdio.h>
#include <windows.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <stdint.h>

// includes for the library
#include "LibL6474.h"


// ====================================================================================================================
// area of state helpers and mockup functions
// ====================================================================================================================

#define STATUS_HIGHZ_MASK       ( 1 <<  0 )
#define STATUS_DIRECTION_MASK   ( 1 <<  4 )
#define STATUS_NOTPERF_CMD_MASK ( 1 <<  7 )
#define STATUS_WRONG_CMD_MASK   ( 1 <<  8 )
#define STATUS_UNDERVOLT_MASK   ( 1 <<  9 )
#define STATUS_THR_WARN_MASK    ( 1 << 10 )
#define STATUS_THR_SHORTD_MASK  ( 1 << 11 )
#define STATUS_OCD_MASK         ( 1 << 12 )

#define STEP_REG_RANGE_MASK   0x1F
#define STEP_REG_CMD_MASK     0xE0

#define STEP_CMD_NOP_PREFIX      ((char)0x00) //Nothing
#define STEP_CMD_NOP_LENGTH      0x01
#define STEP_CMD_SET_PREFIX      ((char)0x00) //Writes VALUE in PARAM register
#define STEP_CMD_SET_LENGTH      0x01
#define STEP_CMD_SET_MAX_PAYLOAD 0x04
#define STEP_CMD_GET_PREFIX      ((char)0x20) //Reads VALUE from PARAM register
#define STEP_CMD_GET_LENGTH      0x01
#define STEP_CMD_GET_MAX_PAYLOAD 0x04
#define STEP_CMD_ENA_PREFIX      ((char)0xB8) //Enable the power stage
#define STEP_CMD_ENA_LENGTH      0x01
#define STEP_CMD_DIS_PREFIX      ((char)0xA8) //Puts the bridges in High Impedance status immediately
#define STEP_CMD_DIS_LENGTH      0x01
#define STEP_CMD_STA_PREFIX      ((char)0xD0) //Returns the status register value
#define STEP_CMD_STA_LENGTH      0x03
#define STEP_REG_ABS_POS      0x01
#define STEP_LEN_ABS_POS      0x03
#define STEP_MASK_ABS_POS     0x3FFFFF
#define STEP_OFFSET_ABS_POS   0x0

#define STEP_REG_EL_POS       0x02
#define STEP_LEN_EL_POS       0x02
#define STEP_MASK_EL_POS      0x1FF
#define STEP_OFFSET_EL_POS    0x0

#define STEP_REG_MARK         0x03
#define STEP_LEN_MARK         0x03
#define STEP_MASK_MARK        0x3FFFFF
#define STEP_OFFSET_MARK      0x0

#define STEP_REG_TVAL         0x09
#define STEP_LEN_TVAL         0x01
#define STEP_MASK_TVAL        0x7F
#define STEP_OFFSET_TVAL      0x0

#define STEP_REG_T_FAST       0x0E
#define STEP_LEN_T_FAST       0x01
#define STEP_MASK_T_FAST      0xFF
#define STEP_OFFSET_T_FAST    0x0

#define STEP_REG_TON_MIN      0x0F
#define STEP_LEN_TON_MIN      0x01
#define STEP_MASK_TON_MIN     0x7F
#define STEP_OFFSET_TON_MIN   0x0

#define STEP_REG_TOFF_MIN     0x10
#define STEP_LEN_TOFF_MIN     0x01
#define STEP_MASK_TOFF_MIN    0x7F
#define STEP_OFFSET_TOFF_MIN  0x0

#define STEP_REG_ADC_OUT      0x12
#define STEP_LEN_ADC_OUT      0x01
#define STEP_MASK_ADC_OUT     0x1F
#define STEP_OFFSET_ADC_OUT   0x0

#define STEP_REG_OCD_TH       0x13
#define STEP_LEN_OCD_TH       0x01
#define STEP_MASK_OCD_TH      0x0F
#define STEP_OFFSET_OCD_TH    0x0

#define STEP_REG_STEP_MODE    0x16
#define STEP_LEN_STEP_MODE    0x01
#define STEP_MASK_STEP_MODE   0xFF
#define STEP_OFFSET_STEP_MODE 0x0

#define STEP_REG_ALARM_EN     0x17
#define STEP_LEN_ALARM_EN     0x01
#define STEP_MASK_ALARM_EN    0xFF
#define STEP_OFFSET_ALARM_EN  0x0

#define STEP_REG_CONFIG       0x18
#define STEP_LEN_CONFIG       0x02
#define STEP_MASK_CONFIG      0xFFFF
#define STEP_OFFSET_CONFIG    0x0

#define STEP_REG_STATUS       0x19
#define STEP_LEN_STATUS       0x02
#define STEP_MASK_STATUS      0xFFFF
#define STEP_OFFSET_STATUS    0x0


// global context pointer of IO
static int myIOContext = 0xBADEAFFE;

// global context pointer of GPO
static int myGPOContext = 0xDEADC0DE;

// global context pointer of PWM
static int myPWMContext = 0xBADCAB1E;

// --------------------------------------------------------------------------------------------------------------------
typedef enum L6474x_AccessFlags
// --------------------------------------------------------------------------------------------------------------------
{
    afNONE = 0x00,
    afREAD = 0x01,
    afWRITE = 0x02,
    afWRITE_HighZ = 0x04
} L6474x_AccessFlags_t;


// --------------------------------------------------------------------------------------------------------------------
typedef struct L6474x_ParameterDescriptor
// --------------------------------------------------------------------------------------------------------------------
{
    unsigned char        command;
    unsigned char        defined;
    unsigned char        length;
    unsigned int         mask;
    char* address;
    L6474x_AccessFlags_t flags;
} L6474x_ParameterDescriptor_t;


// --------------------------------------------------------------------------------------------------------------------
static struct myState
// --------------------------------------------------------------------------------------------------------------------
{
    L6474x_Platform_t p;
    L6474_Handle_t h;
    L6474_BaseParameter_t b;
    void* pIoCtx;
    void* pGpoCtx;
    void* pPwmCtx;
    struct
    {
        int isResetted;
        int direction;
        int32_t position;
        int highZ;
        struct
        {
            uint16_t status;
            uint32_t abs_pos;
            uint32_t el_pos;
            uint32_t mark;
            uint8_t  ton;
            uint8_t  toff;
            uint8_t  tfast;
            uint8_t  tval;
            uint8_t  adc_out;
            uint8_t  ocd_th;
            uint8_t  step_mode;
            uint8_t  alarm;
            uint16_t config;
        } registers;
        struct
        {
            int                custom;
            L6474x_ErrorCode_t defaultResult;
            int (*func)(void*);
        } cancel;
        struct
        {
            int                custom;
            L6474x_ErrorCode_t defaultResult;
            int (*func)(void*);
        } getflag;
        struct
        {
            int                custom;
            L6474x_ErrorCode_t defaultResult;
            int (*func)(void);
        } lock;
        struct
        {
            int                custom;
            void (*func)(void);
        } unlock;
        struct
        {
            int                custom;
            void (*func)(void* pGPO, const int ena);
        } reset;
        struct
        {
            int                custom;
            void (*func)(unsigned int);
        } sleep;
        struct
        {
            int                custom;
            L6474x_ErrorCode_t defaultResult;
            int (*func)(void* pPWM, int dir, unsigned int numPulses, void (*doneClb)(L6474_Handle_t), L6474_Handle_t h);
        } stepAsync;
        struct
        {
            int                custom;
            L6474x_ErrorCode_t defaultResult;
            int (*func)(void* pIO, char* pRX, const char* pTX, unsigned int length);
        } transfer;
    } mock;
} myState;

static const struct myState state_template =
{
    .pIoCtx = &myIOContext,
    .pGpoCtx = &myGPOContext,
    .pPwmCtx = &myPWMContext,

    .mock = {
        .isResetted = 0,
        .direction = 0,
        .highZ = 1,
        .position = 0,
        .registers =
        {
            .status    = 0,
            .abs_pos   = 0,
            .el_pos    = 0,
            .mark      = 0,
            .ton       = 0x29,
            .toff      = 0x29,
            .tfast     = 0x19,
            .tval      = 0x29,
            .adc_out   = 0,
            .ocd_th    = 0x8,
            .step_mode = 0x7,
            .alarm     = 0xFF,
            .config    = 0x2E88
        },
        .cancel = {
            .custom = 0,
            .defaultResult = errcNONE,
            .func = NULL
        },
        .getflag = {
            .custom = 0,
            .defaultResult = errcNONE,
            .func = NULL
        },
        .lock = {
            .custom = 0,
            .defaultResult = errcNONE,
            .func = NULL
        },
        .unlock = {
            .custom = 0,
            .func = NULL
        },
        .reset = {
            .custom = 0,
            .func = NULL
        },
        .sleep = {
            .custom = 0,
            .func = NULL
        },
        .stepAsync = {
            .custom = 0,
            .defaultResult = errcNONE,
            .func = NULL
        },
        .transfer = {
            .custom = 0,
            .defaultResult = errcNONE,
            .func = NULL
        }
    },

    .b = {
        .OcdTh = ocdth1500mA,
        .stepMode = smMICRO16,
        .TFast = 0x14,
        .TimeOffMin = 0x29,
        .TimeOnMin = 0x29,
        .TorqueVal = 0x26
    }
};


// --------------------------------------------------------------------------------------------------------------------
static const L6474x_ParameterDescriptor_t L6474_Parameters[STEP_REG_RANGE_MASK]
// --------------------------------------------------------------------------------------------------------------------
= {
    [STEP_REG_ABS_POS] = {.command = STEP_REG_ABS_POS,   .defined = 1, .length = STEP_LEN_ABS_POS,   .mask = STEP_MASK_ABS_POS,   .address = &myState.mock.registers.abs_pos,   .flags = afREAD | afWRITE       },
    [STEP_REG_EL_POS] = {.command = STEP_REG_EL_POS,    .defined = 1, .length = STEP_LEN_EL_POS,    .mask = STEP_MASK_EL_POS,    .address = &myState.mock.registers.el_pos,    .flags = afREAD | afWRITE       },
    [STEP_REG_MARK] = {.command = STEP_REG_MARK,      .defined = 1, .length = STEP_LEN_MARK,      .mask = STEP_MASK_MARK,      .address = &myState.mock.registers.mark,      .flags = afREAD | afWRITE       },
    [STEP_REG_TVAL] = {.command = STEP_REG_TVAL,      .defined = 1, .length = STEP_LEN_TVAL,      .mask = STEP_MASK_TVAL,      .address = &myState.mock.registers.tval,      .flags = afREAD | afWRITE       },
    [STEP_REG_T_FAST] = {.command = STEP_REG_T_FAST,    .defined = 1, .length = STEP_LEN_T_FAST,    .mask = STEP_MASK_T_FAST,    .address = &myState.mock.registers.tfast,    .flags = afREAD | afWRITE_HighZ },
    [STEP_REG_TON_MIN] = {.command = STEP_REG_TON_MIN,   .defined = 1, .length = STEP_LEN_TON_MIN,   .mask = STEP_MASK_TON_MIN,   .address = &myState.mock.registers.ton,   .flags = afREAD | afWRITE_HighZ },
    [STEP_REG_TOFF_MIN] = {.command = STEP_REG_TOFF_MIN,  .defined = 1, .length = STEP_LEN_TOFF_MIN,  .mask = STEP_MASK_TOFF_MIN,  .address = &myState.mock.registers.toff,  .flags = afREAD | afWRITE_HighZ },
    [STEP_REG_ADC_OUT] = {.command = STEP_REG_ADC_OUT,   .defined = 1, .length = STEP_LEN_ADC_OUT,   .mask = STEP_MASK_ADC_OUT,   .address = &myState.mock.registers.adc_out,   .flags = afREAD                 },
    [STEP_REG_OCD_TH] = {.command = STEP_REG_OCD_TH,    .defined = 1, .length = STEP_LEN_OCD_TH,    .mask = STEP_MASK_OCD_TH,    .address = &myState.mock.registers.ocd_th,    .flags = afREAD | afWRITE       },
    [STEP_REG_STEP_MODE] = {.command = STEP_REG_STEP_MODE, .defined = 1, .length = STEP_LEN_STEP_MODE, .mask = STEP_MASK_STEP_MODE, .address = &myState.mock.registers.step_mode, .flags = afREAD | afWRITE_HighZ },
    [STEP_REG_ALARM_EN] = {.command = STEP_REG_ALARM_EN,  .defined = 1, .length = STEP_LEN_ALARM_EN,  .mask = STEP_MASK_ALARM_EN,  .address = &myState.mock.registers.alarm,  .flags = afREAD | afWRITE       },
    [STEP_REG_CONFIG] = {.command = STEP_REG_CONFIG,    .defined = 1, .length = STEP_LEN_CONFIG,    .mask = STEP_MASK_CONFIG,    .address = &myState.mock.registers.config,    .flags = afREAD | afWRITE_HighZ },
    [STEP_REG_STATUS] = {.command = STEP_REG_STATUS,    .defined = 1, .length = STEP_LEN_STATUS,    .mask = STEP_MASK_STATUS,    .address = &myState.mock.registers.status,    .flags = afREAD                 }
};

// --------------------------------------------------------------------------------------------------------------------
static int myCancelStep(void* pPWM)
// --------------------------------------------------------------------------------------------------------------------
{
    // make sure user has configured the default mocking properly
    assert_in_range(myState.mock.cancel.custom, 0, 1);
    if (!myState.mock.cancel.custom)
    {
        // make sure user has configured the default result properly
        assert_true(myState.mock.cancel.custom <= errcNONE && myState.mock.cancel.custom >= errcFORBIDDEN);
        if (myState.mock.cancel.defaultResult == errcNONE)
        {
            // here we can do some steps which are required for the case of success
        }
        return myState.mock.cancel.defaultResult;
    }
    else
    {
        // make sure user has specified a custom mock function
        assert_non_null(myState.mock.cancel.func);
        return myState.mock.cancel.func(pPWM);
    }
}

// --------------------------------------------------------------------------------------------------------------------
static int myGetFlag(void* pIO)
// --------------------------------------------------------------------------------------------------------------------
{
    // make sure user has configured the default mocking properly
    assert_in_range(myState.mock.getflag.custom, 0, 1);
    if (!myState.mock.getflag.custom)
    {
        // make sure user has configured the default result properly
        assert_true(myState.mock.getflag.custom <= errcNONE && myState.mock.getflag.custom >= errcFORBIDDEN);
        if (myState.mock.getflag.defaultResult == errcNONE)
        {
            // here we can do some steps which are required for the case of success
        }
        return myState.mock.getflag.defaultResult;
    }
    else
    {
        // make sure user has specified a custom mock function
        assert_non_null(myState.mock.getflag.func);
        return myState.mock.getflag.func(pIO);
    }
}

// --------------------------------------------------------------------------------------------------------------------
static int myLock(void)
// --------------------------------------------------------------------------------------------------------------------
{    
    // make sure user has configured the default mocking properly
    assert_in_range(myState.mock.lock.custom, 0, 1);
    if (!myState.mock.lock.custom)
    {
        // make sure user has configured the default result properly
        assert_true(myState.mock.lock.custom <= errcNONE && myState.mock.lock.custom >= errcFORBIDDEN);
        if (myState.mock.lock.defaultResult == errcNONE)
        {
            // here we can do some steps which are required for the case of success
        }
        return myState.mock.lock.defaultResult;
    }
    else
    {
        // make sure user has specified a custom mock function
        assert_non_null(myState.mock.lock.func);
        return myState.mock.lock.func();
    }
}

// --------------------------------------------------------------------------------------------------------------------
static void myUnlock(void)
// --------------------------------------------------------------------------------------------------------------------
{
    // make sure user has configured the default mocking properly
    assert_in_range(myState.mock.unlock.custom, 0, 1);
    if (myState.mock.unlock.custom)
    {
        // make sure user has specified a custom mock function
        assert_non_null(myState.mock.unlock.func);
        myState.mock.unlock.func();
    }
}

// --------------------------------------------------------------------------------------------------------------------
static void myReset(void* pGPO, const int ena)
// --------------------------------------------------------------------------------------------------------------------
{
    // make sure user has configured the default mocking properly
    assert_in_range(myState.mock.reset.custom, 0, 1);
    if (myState.mock.reset.custom)
    {
        // make sure user has specified a custom mock function
        assert_non_null(myState.mock.reset.func);
        myState.mock.reset.func(pGPO, ena);
    }
    else
    {
        myState.mock.isResetted = ena;
        if (myState.mock.isResetted)
        {
            memcpy(&myState.mock.registers, &state_template.mock.registers, sizeof(state_template.mock.registers));
            myState.mock.position = 0;
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------
static void mySleep(unsigned int ms)
// --------------------------------------------------------------------------------------------------------------------
{
    // make sure user has configured the default mocking properly
    assert_in_range(myState.mock.sleep.custom, 0, 1);
    assert_non_null(ms);

    if (myState.mock.sleep.custom)
    {
        // make sure user has specified a custom mock function
        assert_non_null(myState.mock.sleep.func);
        myState.mock.sleep.func(ms);
    }
    else
    {
        Sleep(ms);
    }
}

// --------------------------------------------------------------------------------------------------------------------
static DWORD WINAPI clbDelayThreadFunc(LPVOID lpThreadParameter)
// --------------------------------------------------------------------------------------------------------------------
{
    Sleep(100);
    void (*doneClb)(L6474_Handle_t) = ((void**)lpThreadParameter)[0];
    L6474_Handle_t h = ((void**)lpThreadParameter)[1];
    doneClb(h);
    void** args = lpThreadParameter;
    free(args);
    return 0;
}

// --------------------------------------------------------------------------------------------------------------------
static int myStepAsync(void* pPWM, int dir, unsigned int numPulses, void (*doneClb)(L6474_Handle_t), L6474_Handle_t h)
// --------------------------------------------------------------------------------------------------------------------
{
    // make sure user has configured the default mocking properly
    assert_in_range(myState.mock.stepAsync.custom, 0, 1);
    assert_non_null(doneClb);
    assert_non_null(h);
    assert_non_null(numPulses);

    if (!myState.mock.lock.custom)
    {
        // make sure user has configured the default result properly
        assert_true(myState.mock.stepAsync.custom <= errcNONE && myState.mock.stepAsync.custom >= errcFORBIDDEN);
        if (myState.mock.stepAsync.defaultResult == errcNONE)
        {
            // here we can do some steps which are required for the case of success
            myState.mock.direction = dir;
            if (!myState.mock.highZ)
            {
                int multiplicator = (1 << (myState.mock.registers.step_mode & 0x7));
                if (dir == 0) myState.mock.position -= (numPulses * multiplicator);
                else myState.mock.position += (numPulses * multiplicator);
            }
            // fire and forget the thread, it should terminate itself...
            void** args = calloc(sizeof(void*), 2);
            args[0] = doneClb;
            args[1] = h;
            CreateThread(0, 0, clbDelayThreadFunc, args, 0, NULL);
        }
        return myState.mock.stepAsync.defaultResult;
    }
    else
    {
        // make sure user has specified a custom mock function
        assert_non_null(myState.mock.stepAsync.func);
        return myState.mock.stepAsync.func(pPWM, dir, numPulses, doneClb, h);
    }
}

// --------------------------------------------------------------------------------------------------------------------
static void myMemCpy(void* dst, void* src, unsigned int len, char msb)
// --------------------------------------------------------------------------------------------------------------------
{
    if (msb == 0) memcpy(dst, src, len);
    else {
        for (int i = len - 1; i >= 0; i--)
        {
            *((char*)dst) = ((char*)src)[i];
            dst = (((char*)dst) += 1);
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------
static int myTransfer(void* pIO, char* pRX, const char* pTX, unsigned int length)
// --------------------------------------------------------------------------------------------------------------------
{
    // make sure user has configured the default mocking properly
    assert_in_range(myState.mock.transfer.custom, 0, 1);
    assert_non_null(pRX);
    assert_non_null(pTX);
    assert_non_null(length);

    if (!myState.mock.transfer.custom)
    {
        // make sure user has configured the default result properly
        assert_true(myState.mock.transfer.custom <= errcNONE && myState.mock.transfer.custom >= errcFORBIDDEN);
        if (myState.mock.transfer.defaultResult == errcNONE)
        {
            // here we can do some steps which are required for the case of success
            myState.mock.registers.status &= ~STATUS_HIGHZ_MASK;
            myState.mock.registers.status &= ~STATUS_DIRECTION_MASK;
            myState.mock.registers.status |= myState.mock.highZ;
            myState.mock.registers.status |= myState.mock.direction << 4;
            myState.mock.registers.status |= ( STATUS_UNDERVOLT_MASK | STATUS_THR_WARN_MASK | STATUS_THR_SHORTD_MASK | STATUS_OCD_MASK );
            if (myState.mock.isResetted)
            {
                memcpy(&myState.mock.registers, &state_template.mock.registers, sizeof(state_template.mock.registers));
                myState.mock.position = 0;
                for (int i = 0; i < length; i++) { pRX[i] = STEP_CMD_NOP_PREFIX; }
            }
            else
            {
                if (pTX[0] == STEP_CMD_DIS_PREFIX)
                {
                    myState.mock.highZ = 1;
                }
                else if (pTX[0] == STEP_CMD_ENA_PREFIX)
                {
                    myState.mock.highZ = 0;
                }
                else if ((pTX[0] & STEP_REG_CMD_MASK) == STEP_CMD_GET_PREFIX)
                {
                    if (!L6474_Parameters[pTX[0] & STEP_REG_RANGE_MASK].defined)
                    {
                        myState.mock.registers.status |= (STATUS_NOTPERF_CMD_MASK);
                        for (int i = 0; i < length; i++) { pRX[i] = STEP_CMD_NOP_PREFIX; }
                    }
                    else
                    {
                        char* reg = L6474_Parameters[pTX[0] & STEP_REG_RANGE_MASK].address;
                        int len = L6474_Parameters[pTX[0] & STEP_REG_RANGE_MASK].length;
                        if (reg == &myState.mock.registers.abs_pos)
                            memcpy(reg, &myState.mock.position, sizeof(myState.mock.position));
                        if (length - 1 < len) len = length - 1;
                        myMemCpy(&pRX[1], reg, len, 1);
                    }
                }
                else if ((pTX[0] & STEP_REG_CMD_MASK) == STEP_CMD_SET_PREFIX)
                {
                    if (pTX[0] == 0) for (int i = 0; i < length; i++) { pRX[i] = STEP_CMD_NOP_PREFIX; }
                    else
                    {
                        if (!L6474_Parameters[pTX[0] & STEP_REG_RANGE_MASK].defined)
                        {
                            myState.mock.registers.status |= (STATUS_NOTPERF_CMD_MASK);
                            for (int i = 0; i < length; i++) { pRX[i] = STEP_CMD_NOP_PREFIX; }
                        }
                        if ((L6474_Parameters[pTX[0] & STEP_REG_RANGE_MASK].flags & afWRITE_HighZ) && (myState.mock.highZ == 0))
                        {
                            myState.mock.registers.status |= (STATUS_NOTPERF_CMD_MASK);
                            for (int i = 0; i < length; i++) { pRX[i] = STEP_CMD_NOP_PREFIX; }
                        }
                        else
                        {
                            char* reg = L6474_Parameters[pTX[0] & STEP_REG_RANGE_MASK].address;
                            int len = L6474_Parameters[pTX[0] & STEP_REG_RANGE_MASK].length;
                            if (length - 1 < len) len = length - 1;
                            myMemCpy(reg, &pTX[1], len, 1);
                            if (reg == &myState.mock.registers.abs_pos)
                                memcpy(&myState.mock.position, reg, sizeof(myState.mock.position));
                        }
                    }
                }
                else if (pTX[0] == STEP_CMD_STA_PREFIX)
                {
                    char* reg = &myState.mock.registers.status;
                    int len = sizeof(myState.mock.registers.status);
                    myMemCpy(&pRX[1], reg, len, 1);
                }
                else
                {
                    myState.mock.registers.status |= (STATUS_WRONG_CMD_MASK);
                }
            }
        }
        return myState.mock.transfer.defaultResult;
    }
    else
    {
        // make sure user has specified a custom mock function
        assert_non_null(myState.mock.transfer.func);
        return myState.mock.transfer.func(pIO, pRX, pTX, length);
    }
}


// ====================================================================================================================
// area of test cases
// ====================================================================================================================

// test case
// --------------------------------------------------------------------------------------------------------------------
static void null_test_instance_creation_cancel_step(void** state)
// --------------------------------------------------------------------------------------------------------------------
{
    struct myState* s = ((struct myState*)*state);
    s->p.cancelStep = NULL;

    assert_null((s->h = L6474_CreateInstance(&s->p, NULL, NULL, NULL)));
    assert_null((s->h = L6474_CreateInstance(&s->p, s->pIoCtx, NULL, NULL)));
    assert_null((s->h = L6474_CreateInstance(&s->p, s->pIoCtx, s->pGpoCtx, NULL)));
    assert_null((s->h = L6474_CreateInstance(&s->p, s->pIoCtx, s->pGpoCtx, s->pPwmCtx)));
}

// test case
// --------------------------------------------------------------------------------------------------------------------
static void null_test_instance_creation_free(void** state)
// --------------------------------------------------------------------------------------------------------------------
{
    struct myState* s = ((struct myState*)*state);
    s->p.free = NULL;

    assert_null((s->h = L6474_CreateInstance(&s->p, NULL, NULL, NULL)));
    assert_null((s->h = L6474_CreateInstance(&s->p, s->pIoCtx, NULL, NULL)));
    assert_null((s->h = L6474_CreateInstance(&s->p, s->pIoCtx, s->pGpoCtx, NULL)));
    assert_null((s->h = L6474_CreateInstance(&s->p, s->pIoCtx, s->pGpoCtx, s->pPwmCtx)));
}

// test case
// --------------------------------------------------------------------------------------------------------------------
static void null_test_instance_creation_get_flag(void** state)
// --------------------------------------------------------------------------------------------------------------------
{
    struct myState* s = ((struct myState*)*state);
    s->p.getFlag = NULL;

    assert_null((s->h = L6474_CreateInstance(&s->p, NULL, NULL, NULL)));
    assert_null((s->h = L6474_CreateInstance(&s->p, s->pIoCtx, NULL, NULL)));
    assert_null((s->h = L6474_CreateInstance(&s->p, s->pIoCtx, s->pGpoCtx, NULL)));
    assert_null((s->h = L6474_CreateInstance(&s->p, s->pIoCtx, s->pGpoCtx, s->pPwmCtx)));
}

// test case
// --------------------------------------------------------------------------------------------------------------------
static void null_test_instance_creation_lock(void** state)
// --------------------------------------------------------------------------------------------------------------------
{
    struct myState* s = ((struct myState*)*state);
    s->p.lock = NULL;

    assert_null((s->h = L6474_CreateInstance(&s->p, NULL, NULL, NULL)));
    assert_null((s->h = L6474_CreateInstance(&s->p, s->pIoCtx, NULL, NULL)));
    assert_null((s->h = L6474_CreateInstance(&s->p, s->pIoCtx, s->pGpoCtx, NULL)));
    assert_null((s->h = L6474_CreateInstance(&s->p, s->pIoCtx, s->pGpoCtx, s->pPwmCtx)));
}

// test case
// --------------------------------------------------------------------------------------------------------------------
static void null_test_instance_creation_malloc(void** state)
// --------------------------------------------------------------------------------------------------------------------
{
    struct myState* s = ((struct myState*)*state);
    s->p.malloc = NULL;

    assert_null((s->h = L6474_CreateInstance(&s->p, NULL, NULL, NULL)));
    assert_null((s->h = L6474_CreateInstance(&s->p, s->pIoCtx, NULL, NULL)));
    assert_null((s->h = L6474_CreateInstance(&s->p, s->pIoCtx, s->pGpoCtx, NULL)));
    assert_null((s->h = L6474_CreateInstance(&s->p, s->pIoCtx, s->pGpoCtx, s->pPwmCtx)));
}

// test case
// --------------------------------------------------------------------------------------------------------------------
static void null_test_instance_creation_reset(void** state)
// --------------------------------------------------------------------------------------------------------------------
{
    struct myState* s = ((struct myState*)*state);
    s->p.reset = NULL;

    assert_null((s->h = L6474_CreateInstance(&s->p, NULL, NULL, NULL)));
    assert_null((s->h = L6474_CreateInstance(&s->p, s->pIoCtx, NULL, NULL)));
    assert_null((s->h = L6474_CreateInstance(&s->p, s->pIoCtx, s->pGpoCtx, NULL)));
    assert_null((s->h = L6474_CreateInstance(&s->p, s->pIoCtx, s->pGpoCtx, s->pPwmCtx)));
}

// test case
// --------------------------------------------------------------------------------------------------------------------
static void null_test_instance_creation_sleep(void** state)
// --------------------------------------------------------------------------------------------------------------------
{
    struct myState* s = ((struct myState*)*state);
    s->p.sleep = NULL;

    assert_null((s->h = L6474_CreateInstance(&s->p, NULL, NULL, NULL)));
    assert_null((s->h = L6474_CreateInstance(&s->p, s->pIoCtx, NULL, NULL)));
    assert_null((s->h = L6474_CreateInstance(&s->p, s->pIoCtx, s->pGpoCtx, NULL)));
    assert_null((s->h = L6474_CreateInstance(&s->p, s->pIoCtx, s->pGpoCtx, s->pPwmCtx)));
}

// test case
// --------------------------------------------------------------------------------------------------------------------
static void null_test_instance_creation_step_async(void** state)
// --------------------------------------------------------------------------------------------------------------------
{
    struct myState* s = ((struct myState*)*state);
    s->p.stepAsync = NULL;

    assert_null((s->h = L6474_CreateInstance(&s->p, NULL, NULL, NULL)));
    assert_null((s->h = L6474_CreateInstance(&s->p, s->pIoCtx, NULL, NULL)));
    assert_null((s->h = L6474_CreateInstance(&s->p, s->pIoCtx, s->pGpoCtx, NULL)));
    assert_null((s->h = L6474_CreateInstance(&s->p, s->pIoCtx, s->pGpoCtx, s->pPwmCtx)));
}

// test case
// --------------------------------------------------------------------------------------------------------------------
static void null_test_instance_creation_transfer(void** state)
// --------------------------------------------------------------------------------------------------------------------
{
    struct myState* s = ((struct myState*)*state);
    s->p.transfer = NULL;

    assert_null((s->h = L6474_CreateInstance(&s->p, NULL, NULL, NULL)));
    assert_null((s->h = L6474_CreateInstance(&s->p, s->pIoCtx, NULL, NULL)));
    assert_null((s->h = L6474_CreateInstance(&s->p, s->pIoCtx, s->pGpoCtx, NULL)));
    assert_null((s->h = L6474_CreateInstance(&s->p, s->pIoCtx, s->pGpoCtx, s->pPwmCtx)));
}

// test case
// --------------------------------------------------------------------------------------------------------------------
static void null_test_instance_creation_unlock(void** state)
// --------------------------------------------------------------------------------------------------------------------
{
    struct myState* s = ((struct myState*)*state);
    s->p.unlock = NULL;

    assert_null((s->h = L6474_CreateInstance(&s->p, NULL, NULL, NULL)));
    assert_null((s->h = L6474_CreateInstance(&s->p, s->pIoCtx, NULL, NULL)));
    assert_null((s->h = L6474_CreateInstance(&s->p, s->pIoCtx, s->pGpoCtx, NULL)));
    assert_null((s->h = L6474_CreateInstance(&s->p, s->pIoCtx, s->pGpoCtx, s->pPwmCtx)));
}

// test case
// --------------------------------------------------------------------------------------------------------------------
static void non_null_test_instance_creation_successful_1(void** state)
// --------------------------------------------------------------------------------------------------------------------
{
    struct myState* s = ((struct myState*)*state);
    assert_non_null((s->h = L6474_CreateInstance(&s->p, NULL, NULL, NULL)));
}

// test case
// --------------------------------------------------------------------------------------------------------------------
static void non_null_test_instance_creation_successful_2(void** state)
// --------------------------------------------------------------------------------------------------------------------
{
    struct myState* s = ((struct myState*)*state);
    assert_non_null((s->h = L6474_CreateInstance(&s->p, s->pIoCtx, NULL, NULL)));
}

// test case
// --------------------------------------------------------------------------------------------------------------------
static void non_null_test_instance_creation_successful_3(void** state)
// --------------------------------------------------------------------------------------------------------------------
{
    struct myState* s = ((struct myState*)*state);
    assert_non_null((s->h = L6474_CreateInstance(&s->p, s->pIoCtx, s->pGpoCtx, NULL)));
}

// test case
// --------------------------------------------------------------------------------------------------------------------
static void non_null_test_instance_creation_successful_4(void** state)
// --------------------------------------------------------------------------------------------------------------------
{
    struct myState* s = ((struct myState*)*state);
    assert_non_null((s->h = L6474_CreateInstance(&s->p, s->pIoCtx, s->pGpoCtx, s->pPwmCtx)));
}

// test case
// --------------------------------------------------------------------------------------------------------------------
static void instance_creation_and_destruction(void** state)
// --------------------------------------------------------------------------------------------------------------------
{
    struct myState* s = ((struct myState*)*state);
    assert_non_null((s->h = L6474_CreateInstance(&s->p, s->pIoCtx, s->pGpoCtx, s->pPwmCtx)));
    assert_int_equal(L6474_DestroyInstance(s->h), errcNONE);
    s->h = NULL;
}

// test case
// --------------------------------------------------------------------------------------------------------------------
static void instance_status_state_test(void** t_state)
// --------------------------------------------------------------------------------------------------------------------
{
    L6474_Handle_t         h      = ((struct myState*)*t_state)->h;
    L6474_BaseParameter_t* b      = &((struct myState*)*t_state)->b;
    L6474_Status_t         status = { 0 };
    L6474x_State_t         state  = stRESET;

    // we start with a library kept in reset state
    assert_int_equal(L6474_GetStatus(h, &status), errcINV_STATE);
    assert_int_equal(L6474_GetState(h, &state), errcNONE);
    assert_int_equal(state, stRESET);

    // now it does not fail anymore
    assert_int_equal(L6474_Initialize(h, b), errcNONE);
    assert_int_equal(L6474_GetStatus(h, &status), errcNONE);

    // the deufault states no errors but HighZ
    assert_int_equal(status.HIGHZ,       1);
    assert_int_equal(status.NOTPERF_CMD, 0);
    assert_int_equal(status.OCD,         0);
    assert_int_equal(status.ONGOING,     0);
    assert_int_equal(status.TH_SD,       0);
    assert_int_equal(status.TH_WARN,     0);
    assert_int_equal(status.UVLO,        0);
    assert_int_equal(status.WRONG_CMD,   0);

    assert_int_equal(L6474_GetState(h, &state), errcNONE);
    assert_int_equal(state, stDISABLED);

    // disabling power outputs while they are disabled should not fail
    assert_int_equal(L6474_SetPowerOutputs(h, 0), errcNONE);
    // enabling power outputs while they are disabled should not fail
    assert_int_equal(L6474_SetPowerOutputs(h, 1), errcNONE);
    assert_int_equal(L6474_GetState(h, &state), errcNONE);
    assert_int_equal(state, stENABLED);

    // enabling power outputs while they are enabled should not fail
    assert_int_equal(L6474_SetPowerOutputs(h, 1), errcNONE);
    assert_int_equal(L6474_GetState(h, &state), errcNONE);
    assert_int_equal(state, stENABLED);

    // check the state again
    assert_int_equal(L6474_GetStatus(h, &status), errcNONE);
    assert_int_equal(status.HIGHZ,       0);
    assert_int_equal(status.NOTPERF_CMD, 0);
    assert_int_equal(status.OCD,         0);
    assert_int_equal(status.ONGOING,     0);
    assert_int_equal(status.TH_SD,       0);
    assert_int_equal(status.TH_WARN,     0);
    assert_int_equal(status.UVLO,        0);
    assert_int_equal(status.WRONG_CMD,   0);

    // disabling power outputs while they are enabled should not fail
    assert_int_equal(L6474_SetPowerOutputs(h, 0), errcNONE);
    assert_int_equal(L6474_GetState(h, &state), errcNONE);
    assert_int_equal(state, stDISABLED);

    // resetting the library should lead to the initial state
    L6474_ResetStandBy(h);
    assert_int_equal(L6474_GetStatus(h, &status), errcINV_STATE);
    assert_int_equal(L6474_GetState(h, &state), errcNONE);
    assert_int_equal(state, stRESET);
}

// test case
// --------------------------------------------------------------------------------------------------------------------
static void instance_check_properties_test(void** t_state)
// --------------------------------------------------------------------------------------------------------------------
{
    L6474_Handle_t         h = ((struct myState*)*t_state)->h;
    L6474_BaseParameter_t* b = &((struct myState*)*t_state)->b;
    L6474_Status_t         status = { 0 };
    L6474x_State_t         state = stRESET;
    int                    value = 0;

    // initialize default values
    assert_int_equal(L6474_Initialize(h, b), errcNONE);

    // check intial properties after intialization
    assert_int_equal(L6474_GetProperty(h, L6474_PROP_TORQUE, &value), errcNONE);
    assert_int_equal(value, state_template.b.TorqueVal);
    assert_int_equal(L6474_GetProperty(h, L6474_PROP_ADC_OUT, &value), errcNONE);
    assert_int_equal(value, 0);
    assert_int_equal(L6474_GetProperty(h, L6474_PROP_OCDTH, &value), errcNONE);
    assert_int_equal(value, state_template.b.OcdTh);
    assert_int_equal(L6474_GetProperty(h, L6474_PROP_TFAST, &value), errcNONE);
    assert_int_equal(value, state_template.b.TFast);
    assert_int_equal(L6474_GetProperty(h, L6474_PROP_TOFF, &value), errcNONE);
    assert_int_equal(value, state_template.b.TimeOffMin);
    assert_int_equal(L6474_GetProperty(h, L6474_PROP_TON, &value), errcNONE);
    assert_int_equal(value, state_template.b.TimeOnMin);
    assert_int_equal(L6474_GetStepMode(h, &value), errcNONE);
    assert_int_equal(value, state_template.b.stepMode);

    // check intial properties after enabling the power outputs
    assert_int_equal(L6474_SetPowerOutputs(h, 1), errcNONE);
    assert_int_equal(L6474_GetProperty(h, L6474_PROP_TORQUE, &value), errcNONE);
    assert_int_equal(value, state_template.b.TorqueVal);
    assert_int_equal(L6474_GetProperty(h, L6474_PROP_ADC_OUT, &value), errcNONE);
    assert_int_equal(value, 0);
    assert_int_equal(L6474_GetProperty(h, L6474_PROP_OCDTH, &value), errcNONE);
    assert_int_equal(value, state_template.b.OcdTh);
    assert_int_equal(L6474_GetProperty(h, L6474_PROP_TFAST, &value), errcNONE);
    assert_int_equal(value, state_template.b.TFast);
    assert_int_equal(L6474_GetProperty(h, L6474_PROP_TOFF, &value), errcNONE);
    assert_int_equal(value, state_template.b.TimeOffMin);
    assert_int_equal(L6474_GetProperty(h, L6474_PROP_TON, &value), errcNONE);
    assert_int_equal(value, state_template.b.TimeOnMin);
    assert_int_equal(L6474_GetStepMode(h, &value), errcNONE);
    assert_int_equal(value, state_template.b.stepMode);

    // write some properties without power outputs enabled
    assert_int_equal(L6474_SetPowerOutputs(h, 0), errcNONE);
    value = 0x10;
    assert_int_equal(L6474_SetProperty(h, L6474_PROP_TORQUE, value), errcNONE);
    assert_int_equal(L6474_GetProperty(h, L6474_PROP_TORQUE, &value), errcNONE);
    assert_int_equal(value, 0x10);
    value = 0x20;
    assert_int_equal(L6474_SetProperty(h, L6474_PROP_ADC_OUT, value), errcFORBIDDEN); // read only
    assert_int_equal(L6474_GetProperty(h, L6474_PROP_ADC_OUT, &value), errcNONE);
    assert_int_equal(value, 0);
    value = 0x03;
    assert_int_equal(L6474_SetProperty(h, L6474_PROP_OCDTH, value), errcNONE);
    assert_int_equal(L6474_GetProperty(h, L6474_PROP_OCDTH, &value), errcNONE);
    assert_int_equal(value, 0x03);
    value = 0xf;
    assert_int_equal(L6474_SetProperty(h, L6474_PROP_TFAST, value), errcNONE);
    assert_int_equal(L6474_GetProperty(h, L6474_PROP_TFAST, &value), errcNONE);
    assert_int_equal(value, 0xf);
    value = 0x10;
    assert_int_equal(L6474_SetProperty(h, L6474_PROP_TOFF, value), errcNONE);
    assert_int_equal(L6474_GetProperty(h, L6474_PROP_TOFF, &value), errcNONE);
    assert_int_equal(value, 0x10);
    value = 0x00;
    assert_int_equal(L6474_SetProperty(h, L6474_PROP_TON, value), errcNONE);
    assert_int_equal(L6474_GetProperty(h, L6474_PROP_TON, &value), errcNONE);
    assert_int_equal(value, 0x00);
    value = smHALF;
    assert_int_equal(L6474_SetStepMode(h, value), errcNONE);
    assert_int_equal(L6474_GetStepMode(h, &value), errcNONE);
    assert_int_equal(value, smHALF);
    value = 0x08;
    assert_int_equal(L6474_SetAlarmEnables(h, value), errcNONE);
    assert_int_equal(L6474_GetAlarmEnables(h, &value), errcNONE);
    assert_int_equal(value, 0x08);

    // now reset all and check that everything is at default after reinitialization
    assert_int_equal(L6474_ResetStandBy(h), errcNONE);
    assert_int_equal(L6474_Initialize(h, b), errcNONE);

    // check intial properties after intialization
    assert_int_equal(L6474_GetProperty(h, L6474_PROP_TORQUE, &value), errcNONE);
    assert_int_equal(value, state_template.b.TorqueVal);
    assert_int_equal(L6474_GetProperty(h, L6474_PROP_ADC_OUT, &value), errcNONE);
    assert_int_equal(value, 0);
    assert_int_equal(L6474_GetProperty(h, L6474_PROP_OCDTH, &value), errcNONE);
    assert_int_equal(value, state_template.b.OcdTh);
    assert_int_equal(L6474_GetProperty(h, L6474_PROP_TFAST, &value), errcNONE);
    assert_int_equal(value, state_template.b.TFast);
    assert_int_equal(L6474_GetProperty(h, L6474_PROP_TOFF, &value), errcNONE);
    assert_int_equal(value, state_template.b.TimeOffMin);
    assert_int_equal(L6474_GetProperty(h, L6474_PROP_TON, &value), errcNONE);
    assert_int_equal(value, state_template.b.TimeOnMin);
    assert_int_equal(L6474_GetStepMode(h, &value), errcNONE);
    assert_int_equal(value, state_template.b.stepMode);
    assert_int_equal(L6474_GetAlarmEnables(h, &value), errcNONE);
    assert_int_equal(value, 0xFF);

    // write some properties without power outputs enabled
    assert_int_equal(L6474_SetPowerOutputs(h, 1), errcNONE);
    value = 0x10;
    assert_int_equal(L6474_SetProperty(h, L6474_PROP_TORQUE, value), errcNONE);
    assert_int_equal(L6474_GetProperty(h, L6474_PROP_TORQUE, &value), errcNONE);
    assert_int_equal(value, 0x10);
    value = 0x20;
    assert_int_equal(L6474_SetProperty(h, L6474_PROP_ADC_OUT, value), errcFORBIDDEN); // read only
    assert_int_equal(L6474_GetProperty(h, L6474_PROP_ADC_OUT, &value), errcNONE);
    assert_int_equal(value, 0x0);
    value = 0x03;
    assert_int_equal(L6474_SetProperty(h, L6474_PROP_OCDTH, value), errcNONE);
    assert_int_equal(L6474_GetProperty(h, L6474_PROP_OCDTH, &value), errcNONE);
    assert_int_equal(value, 0x03);
    value = 0x0f;
    assert_int_equal(L6474_SetProperty(h, L6474_PROP_TFAST, value), errcINV_STATE);
    assert_int_equal(L6474_GetProperty(h, L6474_PROP_TFAST, &value), errcNONE);
    assert_int_equal(value, state_template.b.TFast);
    value = 0x10;
    assert_int_equal(L6474_SetProperty(h, L6474_PROP_TOFF, value), errcINV_STATE);
    assert_int_equal(L6474_GetProperty(h, L6474_PROP_TOFF, &value), errcNONE);
    assert_int_equal(value, state_template.b.TimeOffMin);
    value = 0x00;
    assert_int_equal(L6474_SetProperty(h, L6474_PROP_TON, value), errcINV_STATE);
    assert_int_equal(L6474_GetProperty(h, L6474_PROP_TON, &value), errcNONE);
    assert_int_equal(value, state_template.b.TimeOnMin);
    value = smHALF;
    assert_int_equal(L6474_SetStepMode(h, value), errcINV_STATE);
    assert_int_equal(L6474_GetStepMode(h, &value), errcNONE);
    assert_int_equal(value, state_template.b.stepMode);
    value = 0x08;
    assert_int_equal(L6474_SetAlarmEnables(h, value), errcNONE);
    assert_int_equal(L6474_GetAlarmEnables(h, &value), errcNONE);
    assert_int_equal(value, 0x08);

    // now reset all and check that everything is at default after reinitialization
    assert_int_equal(L6474_ResetStandBy(h), errcNONE);
    assert_int_equal(L6474_Initialize(h, b), errcNONE);

    // check intial properties after intialization
    assert_int_equal(L6474_GetProperty(h, L6474_PROP_TORQUE, &value), errcNONE);
    assert_int_equal(value, state_template.b.TorqueVal);
    assert_int_equal(L6474_GetProperty(h, L6474_PROP_ADC_OUT, &value), errcNONE);
    assert_int_equal(value, 0);
    assert_int_equal(L6474_GetProperty(h, L6474_PROP_OCDTH, &value), errcNONE);
    assert_int_equal(value, state_template.b.OcdTh);
    assert_int_equal(L6474_GetProperty(h, L6474_PROP_TFAST, &value), errcNONE);
    assert_int_equal(value, state_template.b.TFast);
    assert_int_equal(L6474_GetProperty(h, L6474_PROP_TOFF, &value), errcNONE);
    assert_int_equal(value, state_template.b.TimeOffMin);
    assert_int_equal(L6474_GetProperty(h, L6474_PROP_TON, &value), errcNONE);
    assert_int_equal(value, state_template.b.TimeOnMin);
    assert_int_equal(L6474_GetStepMode(h, &value), errcNONE);
    assert_int_equal(value, state_template.b.stepMode);
    assert_int_equal(L6474_GetAlarmEnables(h, &value), errcNONE);
    assert_int_equal(value, 0xFF);
}

// test case
// --------------------------------------------------------------------------------------------------------------------
static void instance_check_reference_and_position_test(void** t_state)
// --------------------------------------------------------------------------------------------------------------------
{
    L6474_Handle_t         h = ((struct myState*)*t_state)->h;
    L6474_BaseParameter_t* b = &((struct myState*)*t_state)->b;
    L6474_Status_t         status = { 0 };
    L6474x_State_t         state = stRESET;
    int                    value = 0;

    // initialize default values
    assert_int_equal(L6474_Initialize(h, b), errcNONE);

    // default is all zero
    assert_int_equal(L6474_GetAbsolutePosition(h, &value), errcNONE);
    assert_int_equal(value, 0);
    value = 100;
    assert_int_equal(L6474_SetAbsolutePosition(h, value), errcNONE);
    assert_int_equal(L6474_GetAbsolutePosition(h, &value), errcNONE);
    assert_int_equal(value, 100);

    assert_int_equal(L6474_GetElectricalPosition(h, &value), errcNONE);
    assert_int_equal(value, 0);
    value = 127;
    assert_int_equal(L6474_SetElectricalPosition(h, value), errcNONE);
    assert_int_equal(L6474_GetElectricalPosition(h, &value), errcNONE);
    assert_int_equal(value, 127);

    assert_int_equal(L6474_GetPositionMark(h, &value), errcNONE);
    assert_int_equal(value, 0);
    value = -400;
    assert_int_equal(L6474_SetPositionMark(h, value), errcNONE);
    assert_int_equal(L6474_GetPositionMark(h, &value), errcNONE);
    assert_int_equal(value, -400);

    // now reset and reinitialize everything
    assert_int_equal(L6474_ResetStandBy(h), errcNONE);
    assert_int_equal(L6474_Initialize(h, b), errcNONE);
    assert_int_equal(L6474_SetPowerOutputs(h, 1), errcNONE);

    // defaults are all again zero
    assert_int_equal(L6474_GetAbsolutePosition(h, &value), errcNONE);
    assert_int_equal(value, 0);
    value = 100;
    assert_int_equal(L6474_SetAbsolutePosition(h, value), errcNONE);
    assert_int_equal(L6474_GetAbsolutePosition(h, &value), errcNONE);
    assert_int_equal(value, 100);

    assert_int_equal(L6474_GetElectricalPosition(h, &value), errcNONE);
    assert_int_equal(value, 0);
    value = 42;
    assert_int_equal(L6474_SetElectricalPosition(h, value), errcNONE);
    assert_int_equal(L6474_GetElectricalPosition(h, &value), errcNONE);
    assert_int_equal(value, 42);

    assert_int_equal(L6474_GetPositionMark(h, &value), errcNONE);
    assert_int_equal(value, 0);
    value = -400;
    assert_int_equal(L6474_SetPositionMark(h, value), errcNONE);
    assert_int_equal(L6474_GetPositionMark(h, &value), errcNONE);
    assert_int_equal(value, -400);

    assert_int_equal(L6474_ResetStandBy(h), errcNONE);
}

// test case
// --------------------------------------------------------------------------------------------------------------------
static void instance_check_movement_test(void** t_state)
// --------------------------------------------------------------------------------------------------------------------
{
    L6474_Handle_t         h = ((struct myState*)*t_state)->h;
    L6474_BaseParameter_t* b = &((struct myState*)*t_state)->b;
    L6474_Status_t         status = { 0 };
    L6474x_State_t         state = stRESET;
    int                    value = 0;

    // initialize default values
    assert_int_equal(L6474_Initialize(h, b), errcNONE);
    assert_int_equal(L6474_SetPowerOutputs(h, 1), errcNONE);

    assert_int_equal(L6474_StepIncremental(h, 1000), errcNONE);
    assert_int_equal(L6474_IsMoving(h, &value), errcNONE);
    assert_int_equal(value, 1);
    while (value == 1)
    {
        assert_int_equal(L6474_IsMoving(h, &value), errcNONE);
        Sleep(10);
    }
    assert_int_equal(value, 0);
    assert_int_equal(L6474_GetAbsolutePosition(h, &value), errcNONE);
    assert_int_equal(value, 1000 * (1 << b->stepMode));

    assert_int_equal(L6474_StepIncremental(h, -1000), errcNONE);
    assert_int_equal(L6474_StepIncremental(h, 1000), errcPENDING);
    assert_int_equal(L6474_IsMoving(h, &value), errcNONE);
    assert_int_equal(value, 1);
    while (value == 1)
    {
        assert_int_equal(L6474_IsMoving(h, &value), errcNONE);
        Sleep(10);
    }
    assert_int_equal(value, 0);
    assert_int_equal(L6474_GetAbsolutePosition(h, &value), errcNONE);
    assert_int_equal(value, 0);

    assert_int_equal(L6474_SetPowerOutputs(h, 0), errcNONE);
    assert_int_equal(L6474_StepIncremental(h, 1000), errcINV_STATE);
}

// test case
// --------------------------------------------------------------------------------------------------------------------
static void instance_check_movement_cancel_test(void** t_state)
// --------------------------------------------------------------------------------------------------------------------
{
    L6474_Handle_t         h = ((struct myState*)*t_state)->h;
    L6474_BaseParameter_t* b = &((struct myState*)*t_state)->b;
    L6474_Status_t         status = { 0 };
    L6474x_State_t         state = stRESET;
    int                    value = 0;

    // initialize default values
    assert_int_equal(L6474_Initialize(h, b), errcNONE);
    assert_int_equal(L6474_SetPowerOutputs(h, 1), errcNONE);

    assert_int_equal(L6474_StepIncremental(h, 1000), errcNONE);
    assert_int_equal(L6474_IsMoving(h, &value), errcNONE);
    assert_int_equal(value, 1);
    assert_int_equal(L6474_StopMovement(h), errcNONE);
    assert_int_equal(L6474_IsMoving(h, &value), errcNONE);
    assert_int_equal(value, 0);
    // its ok when requesting stop while it is already stopped
    assert_int_equal(L6474_StopMovement(h), errcNONE);
    
    assert_int_equal(L6474_SetPowerOutputs(h, 0), errcNONE);
    // its ok when requesting stop while it is already disabled
    assert_int_equal(L6474_StopMovement(h), errcNONE);
}

// ====================================================================================================================
// area of test fixture functions and the corresponding variables
// ====================================================================================================================

// --------------------------------------------------------------------------------------------------------------------
static int myStartFixtureFunction1(void** state)
// --------------------------------------------------------------------------------------------------------------------
{
    // make sure that on beginning always all configuration members are correct
    L6474x_Platform_t template =
    {
        .cancelStep = myCancelStep,
        .free       = free,
        .getFlag    = myGetFlag,
        .lock       = myLock,
        .malloc     = malloc,
        .reset      = myReset,
        .sleep      = mySleep,
        .stepAsync  = myStepAsync,
        .transfer   = myTransfer,
        .unlock     = myUnlock
    };
    memcpy(&myState, &state_template, sizeof(state_template));
    memcpy(&myState.p, &template, sizeof(template));

    // assign the state for the tests
    *state = &myState;
    return 0;
}

// --------------------------------------------------------------------------------------------------------------------
static int myStartFixtureFunction2(void** state)
// --------------------------------------------------------------------------------------------------------------------
{
    // make sure that on beginning always all configuration members are correct
    L6474x_Platform_t template =
    {
        .cancelStep = myCancelStep,
        .free = free,
        .getFlag = myGetFlag,
        .lock = myLock,
        .malloc = malloc,
        .reset = myReset,
        .sleep = mySleep,
        .stepAsync = myStepAsync,
        .transfer = myTransfer,
        .unlock = myUnlock
    };
    memcpy(&myState, &state_template, sizeof(state_template));
    memcpy(&myState.p, &template, sizeof(template));

    // assign the state for the tests
    *state = &myState;
    myState.h = L6474_CreateInstance(&myState.p, myState.pIoCtx, myState.pGpoCtx, myState.pPwmCtx);
    return -(myState.h == NULL);
}


// --------------------------------------------------------------------------------------------------------------------
static int myStopFixtureFunction1(void** state)
// --------------------------------------------------------------------------------------------------------------------
{
    if (((struct myState*)*state)->h != NULL)
    {
        if (((struct myState*)*state)->p.free == NULL)
            ((struct myState*)*state)->p.free = free;

        if (((struct myState*)*state)->p.lock == NULL)
            ((struct myState*)*state)->p.lock = myLock;

        if (((struct myState*)*state)->p.unlock == NULL)
            ((struct myState*)*state)->p.unlock = myUnlock;

        L6474_DestroyInstance(((struct myState*)*state)->h);
        ((struct myState*)*state)->h = NULL;
    }
    return 0;
}

// --------------------------------------------------------------------------------------------------------------------
static int myStopFixtureFunction2(void** state)
// --------------------------------------------------------------------------------------------------------------------
{
    if (((struct myState*)*state)->h != NULL)
    {
        L6474_DestroyInstance(((struct myState*)*state)->h);
        ((struct myState*)*state)->h = NULL;
    }
    return 0;
}


// ====================================================================================================================
// area of main entry point and test execution as well as its corresponding variables
// ====================================================================================================================

// basic instance creation and destruction test group
// --------------------------------------------------------------------------------------------------------------------
const struct CMUnitTest creation_an_destruction_tests[] = {
    cmocka_unit_test_setup_teardown(null_test_instance_creation_cancel_step,      myStartFixtureFunction1, myStopFixtureFunction1),
    cmocka_unit_test_setup_teardown(null_test_instance_creation_free,             myStartFixtureFunction1, myStopFixtureFunction1),
    cmocka_unit_test_setup_teardown(null_test_instance_creation_get_flag,         myStartFixtureFunction1, myStopFixtureFunction1),
    cmocka_unit_test_setup_teardown(null_test_instance_creation_lock,             myStartFixtureFunction1, myStopFixtureFunction1),
    cmocka_unit_test_setup_teardown(null_test_instance_creation_malloc,           myStartFixtureFunction1, myStopFixtureFunction1),
    cmocka_unit_test_setup_teardown(null_test_instance_creation_reset,            myStartFixtureFunction1, myStopFixtureFunction1),
    cmocka_unit_test_setup_teardown(null_test_instance_creation_sleep,            myStartFixtureFunction1, myStopFixtureFunction1),
    cmocka_unit_test_setup_teardown(null_test_instance_creation_step_async,       myStartFixtureFunction1, myStopFixtureFunction1),
    cmocka_unit_test_setup_teardown(null_test_instance_creation_transfer,         myStartFixtureFunction1, myStopFixtureFunction1),
    cmocka_unit_test_setup_teardown(null_test_instance_creation_unlock,           myStartFixtureFunction1, myStopFixtureFunction1),
    cmocka_unit_test_setup_teardown(non_null_test_instance_creation_successful_1, myStartFixtureFunction1, myStopFixtureFunction1),
    cmocka_unit_test_setup_teardown(non_null_test_instance_creation_successful_2, myStartFixtureFunction1, myStopFixtureFunction1),
    cmocka_unit_test_setup_teardown(non_null_test_instance_creation_successful_3, myStartFixtureFunction1, myStopFixtureFunction1),
    cmocka_unit_test_setup_teardown(non_null_test_instance_creation_successful_4, myStartFixtureFunction1, myStopFixtureFunction1),
    cmocka_unit_test_setup_teardown(instance_creation_and_destruction,            myStartFixtureFunction1, myStopFixtureFunction1),
};

// library tests with predefined instance
// --------------------------------------------------------------------------------------------------------------------
const struct CMUnitTest instance_usage_tests[] = {
    cmocka_unit_test_setup_teardown(instance_status_state_test,                 myStartFixtureFunction2, myStopFixtureFunction2),
    cmocka_unit_test_setup_teardown(instance_check_properties_test,             myStartFixtureFunction2, myStopFixtureFunction2),
    cmocka_unit_test_setup_teardown(instance_check_reference_and_position_test, myStartFixtureFunction2, myStopFixtureFunction2),
    cmocka_unit_test_setup_teardown(instance_check_movement_test,               myStartFixtureFunction2, myStopFixtureFunction2),
    cmocka_unit_test_setup_teardown(instance_check_movement_cancel_test,        myStartFixtureFunction2, myStopFixtureFunction2),
};

// --------------------------------------------------------------------------------------------------------------------
int main()
// --------------------------------------------------------------------------------------------------------------------
{
    int result = 0;
    cmocka_set_message_output(CM_OUTPUT_STDOUT);
    result |= cmocka_run_group_tests(creation_an_destruction_tests, NULL, NULL);
    result |= cmocka_run_group_tests(instance_usage_tests,          NULL, NULL);
    return result;
}