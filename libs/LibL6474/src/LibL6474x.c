/*
 * LibL6474x.c
 *
 *  Created on: Dec 1, 2024
 *      Author: Thorsten
 */

 /*! \file */

// --------------------------------------------------------------------------------------------------------------------
#include "LibL6474.h"
#include "LibL6474Config.h"

// --------------------------------------------------------------------------------------------------------------------
#define IN_MILLISEC(x) (x)


// --------------------------------------------------------------------------------------------------------------------
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


// --------------------------------------------------------------------------------------------------------------------
#define STEP_REG_RANGE_MASK   0x1F

// --------------------------------------------------------------------------------------------------------------------
#define HIGH_POS_BIT (1 << 21)
#define HIGH_POS_MASK ((HIGH_POS_BIT*2)-1)

// --------------------------------------------------------------------------------------------------------------------
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

#define STEP_REG_TVAL         L6474_PROP_TORQUE
#define STEP_LEN_TVAL         0x01
#define STEP_MASK_TVAL        0x7F
#define STEP_OFFSET_TVAL      0x0

#define STEP_REG_T_FAST       L6474_PROP_TFAST
#define STEP_LEN_T_FAST       0x01
#define STEP_MASK_T_FAST      0xFF
#define STEP_OFFSET_T_FAST    0x0

#define STEP_REG_TON_MIN      L6474_PROP_TON
#define STEP_LEN_TON_MIN      0x01
#define STEP_MASK_TON_MIN     0x7F
#define STEP_OFFSET_TON_MIN   0x0

#define STEP_REG_TOFF_MIN     L6474_PROP_TOFF
#define STEP_LEN_TOFF_MIN     0x01
#define STEP_MASK_TOFF_MIN    0x7F
#define STEP_OFFSET_TOFF_MIN  0x0

#define STEP_REG_ADC_OUT      L6474_PROP_ADC_OUT
#define STEP_LEN_ADC_OUT      0x01
#define STEP_MASK_ADC_OUT     0x1F
#define STEP_OFFSET_ADC_OUT   0x0

#define STEP_REG_OCD_TH       L6474_PROP_OCDTH
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

#define STATUS_HIGHZ_MASK       ( 1 <<  0 )
#define STATUS_DIRECTION_MASK   ( 1 <<  4 )
#define STATUS_NOTPERF_CMD_MASK ( 1 <<  7 )
#define STATUS_WRONG_CMD_MASK   ( 1 <<  8 )
#define STATUS_UNDERVOLT_MASK   ( 1 <<  9 )
#define STATUS_THR_WARN_MASK    ( 1 << 10 )
#define STATUS_THR_SHORTD_MASK  ( 1 << 11 )
#define STATUS_OCD_MASK         ( 1 << 12 )


// --------------------------------------------------------------------------------------------------------------------
typedef enum L6474x_AccessFlags
// --------------------------------------------------------------------------------------------------------------------
{
	afNONE        = 0x00,
	afREAD        = 0x01,
	afWRITE       = 0x02,
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
	char*                name;
	L6474x_AccessFlags_t flags;
} L6474x_ParameterDescriptor_t;

// --------------------------------------------------------------------------------------------------------------------
struct L6474_Handle
// --------------------------------------------------------------------------------------------------------------------
{
	L6474x_State_t    state;
	int               pending;
	void*             pIO;
	void*             pGPO;
	void*             pPWM;
	L6474x_Platform_t platform;
};

// --------------------------------------------------------------------------------------------------------------------
static const L6474x_ParameterDescriptor_t L6474_Parameters[STEP_REG_RANGE_MASK]
// --------------------------------------------------------------------------------------------------------------------
= {
	[STEP_REG_ABS_POS]   = { .command = STEP_REG_ABS_POS,   .defined = 1, .length = STEP_LEN_ABS_POS,   .mask = STEP_MASK_ABS_POS,   .name = "ABS_POS",   .flags = afREAD | afWRITE       },
	[STEP_REG_EL_POS]    = { .command = STEP_REG_EL_POS,    .defined = 1, .length = STEP_LEN_EL_POS,    .mask = STEP_MASK_EL_POS,    .name = "EL_POS",    .flags = afREAD | afWRITE       },
	[STEP_REG_MARK]      = { .command = STEP_REG_MARK,      .defined = 1, .length = STEP_LEN_MARK,      .mask = STEP_MASK_MARK,      .name = "MARK",      .flags = afREAD | afWRITE       },
	[STEP_REG_TVAL]      = { .command = STEP_REG_TVAL,      .defined = 1, .length = STEP_LEN_TVAL,      .mask = STEP_MASK_TVAL,      .name = "TVAL",      .flags = afREAD | afWRITE       },
	[STEP_REG_T_FAST]    = { .command = STEP_REG_T_FAST,    .defined = 1, .length = STEP_LEN_T_FAST,    .mask = STEP_MASK_T_FAST,    .name = "T_FAST",    .flags = afREAD | afWRITE_HighZ },
	[STEP_REG_TON_MIN]   = { .command = STEP_REG_TON_MIN,   .defined = 1, .length = STEP_LEN_TON_MIN,   .mask = STEP_MASK_TON_MIN,   .name = "TON_MIN",   .flags = afREAD | afWRITE_HighZ },
	[STEP_REG_TOFF_MIN]  = { .command = STEP_REG_TOFF_MIN,  .defined = 1, .length = STEP_LEN_TOFF_MIN,  .mask = STEP_MASK_TOFF_MIN,  .name = "TOFF_MIN",  .flags = afREAD | afWRITE_HighZ },
	[STEP_REG_ADC_OUT]   = { .command = STEP_REG_ADC_OUT,   .defined = 1, .length = STEP_LEN_ADC_OUT,   .mask = STEP_MASK_ADC_OUT,   .name = "ADC_OUT",   .flags = afREAD                 },
	[STEP_REG_OCD_TH]    = { .command = STEP_REG_OCD_TH,    .defined = 1, .length = STEP_LEN_OCD_TH,    .mask = STEP_MASK_OCD_TH,    .name = "OCD_TH",    .flags = afREAD | afWRITE       },
	[STEP_REG_STEP_MODE] = { .command = STEP_REG_STEP_MODE, .defined = 1, .length = STEP_LEN_STEP_MODE, .mask = STEP_MASK_STEP_MODE, .name = "STEP_MODE", .flags = afREAD | afWRITE_HighZ },
	[STEP_REG_ALARM_EN]  = { .command = STEP_REG_ALARM_EN,  .defined = 1, .length = STEP_LEN_ALARM_EN,  .mask = STEP_MASK_ALARM_EN,  .name = "ALARM_EN",  .flags = afREAD | afWRITE       },
	[STEP_REG_CONFIG]    = { .command = STEP_REG_CONFIG,    .defined = 1, .length = STEP_LEN_CONFIG,    .mask = STEP_MASK_CONFIG,    .name = "CONFIG",    .flags = afREAD | afWRITE_HighZ },
	[STEP_REG_STATUS]    = { .command = STEP_REG_STATUS,    .defined = 1, .length = STEP_LEN_STATUS,    .mask = STEP_MASK_STATUS,    .name = "STATUS",    .flags = afREAD                 }
};


// --------------------------------------------------------------------------------------------------------------------
static inline int L6474_HelperLock(L6474_Handle_t h)
// --------------------------------------------------------------------------------------------------------------------
{
#if defined(LIBL6474_HAS_LOCKING) && LIBL6474_HAS_LOCKING == 1
	return h->platform->lock();
#else
	(void)h;
	return 0;
#endif
}

// --------------------------------------------------------------------------------------------------------------------
static inline void L6474_HelperUnlock(L6474_Handle_t h)
// --------------------------------------------------------------------------------------------------------------------
{
#if defined(LIBL6474_HAS_LOCKING) && LIBL6474_HAS_LOCKING == 1
	h->platform->unlock();
#else
	(void)h;
	return;
#endif
}


// --------------------------------------------------------------------------------------------------------------------
static void L6474_HelperReleaseStep(L6474_Handle_t h)
// --------------------------------------------------------------------------------------------------------------------
{
	L6474_HelperLock(h);
	h->pending = 0;
	L6474_HelperUnlock(h);
}

// --------------------------------------------------------------------------------------------------------------------
static int L6474_GetStatusCommand(L6474_Handle_t h)
// --------------------------------------------------------------------------------------------------------------------
{
	if ( h->state == stRESET )
		return errcINV_STATE;

	int length = STEP_CMD_STA_LENGTH;

	unsigned char rxBuff[STEP_CMD_STA_LENGTH] = { 0 };
	unsigned char txBuff[STEP_CMD_STA_LENGTH] = { 0 };

	txBuff[0] = STEP_CMD_STA_PREFIX | 0;
	int ret = h->platform.transfer(h->pIO, (char*)rxBuff, (const char*)txBuff, length);

	if ( ret != 0 )
		return errcINTERNAL;

	ret = (rxBuff[2] << 0 ) | (rxBuff[1] << 8 );
	h->state = ( ret & STATUS_HIGHZ_MASK ) ? stDISABLED : stENABLED;
	return ret;
}


// --------------------------------------------------------------------------------------------------------------------
static int L6474_NopCommand(L6474_Handle_t h)
// --------------------------------------------------------------------------------------------------------------------
{
	if ( h->state == stRESET )
		return errcINV_STATE;

	int length  = STEP_CMD_NOP_LENGTH;

	unsigned char rxBuff[STEP_CMD_NOP_LENGTH] = { 0 };
	unsigned char txBuff[STEP_CMD_NOP_LENGTH] = { 0 };

	txBuff[0] = STEP_CMD_NOP_PREFIX | 0;
	int ret = h->platform.transfer(h->pIO, (char*)rxBuff, (const char*)txBuff, length);

	if ( ret != 0 )
		return errcINTERNAL;

	return errcNONE;
}

// --------------------------------------------------------------------------------------------------------------------
static int L6474_GetParamCommand(L6474_Handle_t h, int addr)
// --------------------------------------------------------------------------------------------------------------------
{
	addr &= STEP_REG_RANGE_MASK;
	if( L6474_Parameters[addr].defined == 0 )
		return errcINV_ARG;

	if( ( L6474_Parameters[addr].flags & afREAD ) == 0 )
		return errcFORBIDDEN;

	if ( h->state == stRESET )
		return errcINV_STATE;

	int length  = L6474_Parameters[addr].length + STEP_CMD_GET_LENGTH;
	if ( length > STEP_CMD_GET_MAX_PAYLOAD )
		return errcINTERNAL;

	unsigned char rxBuff[STEP_CMD_GET_MAX_PAYLOAD] = { STEP_CMD_NOP_PREFIX };
	unsigned char txBuff[STEP_CMD_GET_MAX_PAYLOAD] = { STEP_CMD_NOP_PREFIX };

	txBuff[0] = STEP_CMD_GET_PREFIX | addr;
	int ret = h->platform.transfer(h->pIO, (char*)rxBuff, (const char*)txBuff, length);

	if ( ret != 0 )
		return errcINTERNAL;

	int res = errcNONE;
	unsigned int tmp = 0;
	switch (L6474_Parameters[addr].length)
	{
	    case 1:
	    	tmp = ( rxBuff[1] << 0 );
	    	res = tmp & L6474_Parameters[addr].mask;
	    	break;
	    case 2:
	    	tmp = ( rxBuff[1] << 8 | rxBuff[2] << 0 );
	    	res = tmp & L6474_Parameters[addr].mask;
	    	//res = ( ( tmp & 0xFF00 ) >> 8 ) | ( ( tmp & 0x00FF ) << 8 );
	    	break;
	    case 3:
	    	tmp = ( rxBuff[1] << 16 | rxBuff[2] << 8 | rxBuff[3] << 0 );
	    	res = tmp & L6474_Parameters[addr].mask;
	    	//res = ( ( tmp & 0xFF0000 ) >> 16 ) | ( ( tmp & 0x00FF00 ) << 0 ) | ( ( tmp & 0x0000FF ) << 16 );
	    	break;
	    default:
	    	return errcINTERNAL;
	}

	int opres = 0;
	if ( ( opres = L6474_GetStatusCommand(h) ) < 0 )
		return opres;

	if ( (opres & ( STATUS_NOTPERF_CMD_MASK | STATUS_WRONG_CMD_MASK ) ) != 0 )
		return errcDEVICE_STATE;

	return res;
}

// --------------------------------------------------------------------------------------------------------------------
static int L6474_SetParamCommand(L6474_Handle_t h, int addr, int value)
// --------------------------------------------------------------------------------------------------------------------
{
	addr &= STEP_REG_RANGE_MASK;
	if( L6474_Parameters[addr].defined == 0 )
		return errcINV_ARG;

	if( ( L6474_Parameters[addr].flags & ( afWRITE | afWRITE_HighZ ) ) == 0 )
		return errcFORBIDDEN;

	if ( ( h->state == stRESET ) || ( ( h->state == stENABLED ) && ( ( L6474_Parameters[addr].flags & afWRITE_HighZ ) != 0 ) ) )
		return errcINV_STATE;

	int length  = L6474_Parameters[addr].length + STEP_CMD_SET_LENGTH;
	if ( length > STEP_CMD_SET_MAX_PAYLOAD )
		return errcINTERNAL;

	unsigned char rxBuff[STEP_CMD_SET_MAX_PAYLOAD] = { 0 };
	unsigned char txBuff[STEP_CMD_SET_MAX_PAYLOAD] = { 0 };
	unsigned int  tmp = 0;

	txBuff[0] = STEP_CMD_SET_PREFIX | addr;

	switch (L6474_Parameters[addr].length)
	{
	    case 1:
	    	tmp = value & L6474_Parameters[addr].mask;
	    	txBuff[1] = tmp >> 0;
	    	break;
	    case 2:
	    	tmp = value & L6474_Parameters[addr].mask;
	    	txBuff[1] = tmp >> 8;
	    	txBuff[2] = tmp >> 0;
	    	break;
	    case 3:
	    	tmp = value & L6474_Parameters[addr].mask;
	    	txBuff[1] = tmp >> 16;
	    	txBuff[2] = tmp >> 8;
	    	txBuff[3] = tmp >> 0;
	    	break;
	    default:
	    	return errcINTERNAL;
	}

	int ret = h->platform.transfer(h->pIO, (char*)rxBuff, (const char*)txBuff, length);

	if ( ret != 0 )
		return errcINTERNAL;

	int res = 0;
	if ( ( res = L6474_GetStatusCommand(h) ) < 0 )
		return res;

	if ( ( res & ( STATUS_NOTPERF_CMD_MASK | STATUS_WRONG_CMD_MASK ) ) != 0 )
		return errcDEVICE_STATE;

	return errcNONE;
}

// --------------------------------------------------------------------------------------------------------------------
static int L6474_EnableCommand(L6474_Handle_t h)
// --------------------------------------------------------------------------------------------------------------------
{
	if ( h->state == stRESET )
		return errcINV_STATE;

	if ( h->state == stENABLED )
		return errcNONE;

	int length = STEP_CMD_ENA_LENGTH;

	unsigned char rxBuff[STEP_CMD_ENA_LENGTH] = { 0 };
	unsigned char txBuff[STEP_CMD_ENA_LENGTH] = { 0 };

	txBuff[0] = STEP_CMD_ENA_PREFIX | 0;
	int ret = h->platform.transfer(h->pIO, (char*)rxBuff, (const char*)txBuff, length);

	if ( ret != 0 )
		return errcINTERNAL;

	if ( ( ret = L6474_GetStatusCommand(h) ) < 0 )
		return ret;

	if ( ( ret & ( STATUS_NOTPERF_CMD_MASK | STATUS_WRONG_CMD_MASK ) ) != 0 )
		return errcDEVICE_STATE;

	h->state = stENABLED;
	return errcNONE;
}

// --------------------------------------------------------------------------------------------------------------------
static int L6474_DisableCommand(L6474_Handle_t h)
// --------------------------------------------------------------------------------------------------------------------
{
	if ( h->state == stRESET )
		return errcINV_STATE;

	int length = STEP_CMD_DIS_LENGTH;

	unsigned char rxBuff[STEP_CMD_DIS_LENGTH] = { 0 };
	unsigned char txBuff[STEP_CMD_DIS_LENGTH] = { 0 };

	txBuff[0] = STEP_CMD_DIS_PREFIX | 0;
	int ret = h->platform.transfer(h->pIO, (char*)rxBuff, (const char*)txBuff, length);

	if ( ret != 0 )
		return errcINTERNAL;

	if ( ( ret = L6474_GetStatusCommand(h) ) < 0 )
		return ret;

	if ( ( ret & ( STATUS_NOTPERF_CMD_MASK | STATUS_WRONG_CMD_MASK ) ) != 0 )
		return errcDEVICE_STATE;

	h->state   = stDISABLED;
#if defined(LIBL6474_STEP_ASYNC) && ( LIBL6474_STEP_ASYNC == 1 )
	h->pending = 0;
	h->platform.cancelStep(h->pPWM);
#endif
	return errcNONE;
}


// --------------------------------------------------------------------------------------------------------------------
L6474_Handle_t L6474_CreateInstance(L6474x_Platform_t* p, void* pIO, void* pGPO, void* pPWM)
// --------------------------------------------------------------------------------------------------------------------
{
	if ( p == 0 )
		return 0;

	if ( ( p->reset == 0 ) || ( p->malloc == 0 ) || (p->free == 0) || (p->sleep == 0) || ( p->transfer == 0 ) )
		return 0;

#if defined(LIBL6474_HAS_LOCKING) && LIBL6474_HAS_LOCKING == 1
	if ( ( p->lock == 0 ) || ( p->unlock == 0 ) )
		return 0;
#endif

#if defined(LIBL6474_HAS_FLAG) && ( LIBL6474_HAS_FLAG == 1 )
	if ( p->getFlag == 0 )
		return 0;
#endif

#if defined(LIBL6474_STEP_ASYNC) && ( LIBL6474_STEP_ASYNC == 1 )
	if ( ( p->cancelStep == 0 ) || ( p->stepAsync == 0 ) )
		return 0;
#else
	if ( p->step == 0 )
		return 0;
#endif

	L6474_Handle_t h = p->malloc(sizeof(struct L6474_Handle));
	if ( h == 0 )
		return 0;

	h->pGPO                = pGPO;
	h->pIO                 = pIO;
	h->pPWM                = pPWM;
#if defined(LIBL6474_STEP_ASYNC) && ( LIBL6474_STEP_ASYNC == 1 )
	h->platform.cancelStep = p->cancelStep;
	h->platform.stepAsync  = p->stepAsync;
#else
	h->platform.step       = p->step;
#endif
	h->platform.free       = p->free;
	h->platform.malloc     = p->malloc;
	h->platform.reset      = p->reset;
	h->platform.sleep      = p->sleep;
	h->platform.transfer   = p->transfer;
	h->pending             = 0;
	h->state               = stRESET;

	h->platform.reset(h->pGPO, 1);

	(void)L6474_NopCommand;
	return h;
}


// --------------------------------------------------------------------------------------------------------------------
int L6474_DestroyInstance(L6474_Handle_t h)
// --------------------------------------------------------------------------------------------------------------------
{
	if (h == 0)
		return errcNULL_ARG;

	if (L6474_HelperLock(h) != 0)
		return errcLOCKING;

	h->platform.reset(h->pGPO, 1);
#if defined(LIBL6474_HAS_LOCKING) && LIBL6474_HAS_LOCKING == 1
	void  (*pUnlock)(void) = h->platform->unlock;
#endif
	h->platform.free(h);

#if defined(LIBL6474_HAS_LOCKING) && LIBL6474_HAS_LOCKING == 1
	if ( pUnlock != 0 )
		pUnlock();
#endif

	return errcNONE;
}


// --------------------------------------------------------------------------------------------------------------------
int L6474_ResetStandBy(L6474_Handle_t h)
// --------------------------------------------------------------------------------------------------------------------
{
	if ( h == 0 )
		return errcNULL_ARG;

	if ( L6474_HelperLock(h) != 0 )
		return errcLOCKING;

	// forces the device state to update
	L6474_GetStatusCommand(h);

	if ( h->state == stENABLED )
	{
#if defined(LIBL6474_STEP_ASYNC) && ( LIBL6474_STEP_ASYNC == 1 )
		if ( h->pending != 0 )
		{
			h->platform.cancelStep(h->pPWM);
			h->platform.sleep(IN_MILLISEC(1));
			h->pending = 0;
		}
#endif

		int ret = 0;
		if ( ( ret = L6474_DisableCommand(h) ) != 0 )
		{
			L6474_HelperUnlock(h);
			return ret;
		}
	}

	h->platform.reset(h->pGPO, 1);
	h->state = stRESET;

	h->platform.sleep(IN_MILLISEC(1));
	L6474_HelperUnlock(h);

	return errcNONE;
}


// --------------------------------------------------------------------------------------------------------------------
int L6474_SetBaseParameter(L6474_BaseParameter_t* p)
// --------------------------------------------------------------------------------------------------------------------
{
	if ( p == 0 )
		return errcNULL_ARG;

	p->OcdTh      = ocdth1500mA;
	p->TorqueVal  = 0x26; // ~1,2A
	p->stepMode   = smMICRO16;
	p->TimeOnMin  = 0x29;
	p->TimeOffMin = 0x29;
	p->TFast      = 0x14; //0x19

	return errcNONE;
}


// --------------------------------------------------------------------------------------------------------------------
char L6474_EncodePhaseCurrent(float mA)
// --------------------------------------------------------------------------------------------------------------------
{
	if ( mA >= 4000.0f ) return 0x7F;
	else if ( mA <= 31.25f ) return 0x00;
	else return ( ( mA + 15.625f ) / 31.25f );
}


// --------------------------------------------------------------------------------------------------------------------
int L6474_EncodePhaseCurrentParameter(L6474_BaseParameter_t* p, float mA)
// --------------------------------------------------------------------------------------------------------------------
{
	if ( p == 0 ) return errcNULL_ARG;
	p->TorqueVal = L6474_EncodePhaseCurrent(mA);
	return errcNONE;
}


// --------------------------------------------------------------------------------------------------------------------
int L6474_Initialize(L6474_Handle_t h, L6474_BaseParameter_t* p)
// --------------------------------------------------------------------------------------------------------------------
{
	int val = 0;

	if ( h == 0 || p == 0 )
		return errcNULL_ARG;

	if ( L6474_HelperLock(h) != 0 )
		return errcLOCKING;

	// forces the device state to update
	L6474_GetStatusCommand(h);

	if ( h->state != stRESET )
	{
		if ( ( val = L6474_ResetStandBy(h) ) != 0 )
		{
			L6474_HelperUnlock(h);
			return val;
		}
	}

	h->platform.reset(h->pGPO, 0);
	h->state = stDISABLED;

	h->platform.sleep(IN_MILLISEC(10));

	//Now we have to write the configuration register
	unsigned int CONFIG = 0x2E88; // reset default value
	CONFIG &= ~0xF; // disables all clock outputs and selects internal oscillator

#if defined(LIBL6474_DISABLE_OCD) && ( LIBL6474_DISABLE_OCD == 1 )
	CONFIG &= ~(1 << 7); // disable the OCD
#endif

	if ( ( val = L6474_SetParamCommand(h, STEP_REG_CONFIG, CONFIG) ) != 0 )
	{
		h->platform.reset(h->pGPO, 1);
		h->state = stRESET;
		L6474_HelperUnlock(h);
		return val;
	}

	if ( ( val = L6474_SetParamCommand(h, STEP_REG_OCD_TH, p->OcdTh) ) != 0 )
	{
		h->platform.reset(h->pGPO, 1);
		h->state = stRESET;
		L6474_HelperUnlock(h);
		return val;
	}

	if ( ( val = L6474_SetParamCommand(h, STEP_REG_TVAL, p->TorqueVal) ) != 0 )
	{
		h->platform.reset(h->pGPO, 1);
		h->state = stRESET;
		L6474_HelperUnlock(h);
		return val;
	}

	if ( ( val = L6474_SetParamCommand(h, STEP_REG_TOFF_MIN, p->TimeOffMin) ) != 0 )
	{
		h->platform.reset(h->pGPO, 1);
		h->state = stRESET;
		L6474_HelperUnlock(h);
		return val;
	}

	if ( ( val = L6474_SetParamCommand(h, STEP_REG_TON_MIN, p->TimeOnMin) ) != 0 )
	{
		h->platform.reset(h->pGPO, 1);
		h->state = stRESET;
		L6474_HelperUnlock(h);
		return val;
	}

	if ( ( val = L6474_SetParamCommand(h, STEP_REG_T_FAST, p->TFast) ) != 0 )
	{
		h->platform.reset(h->pGPO, 1);
		h->state = stRESET;
		L6474_HelperUnlock(h);
		return val;
	}

	if ( ( val = L6474_SetStepMode(h, p->stepMode) ) != 0 )
	{
		h->platform.reset(h->pGPO, 1);
		h->state = stRESET;
		L6474_HelperUnlock(h);
		return val;
	}

	// enable all alarms
	if ( ( val = L6474_SetParamCommand(h, STEP_REG_ALARM_EN, STEP_MASK_ALARM_EN) ) != 0 )
	{
		h->platform.reset(h->pGPO, 1);
		h->state = stRESET;
		L6474_HelperUnlock(h);
		return val;
	}

	if ( ( val = L6474_DisableCommand(h) ) != 0 )
	{
		h->platform.reset(h->pGPO, 1);
		h->state = stRESET;
		L6474_HelperUnlock(h);
		return val;
	}

	// now it should not fail when reading status register!
	if ( ( val = L6474_GetStatusCommand(h) ) < 0 )
	{
		h->platform.reset(h->pGPO, 1);
		h->state = stRESET;
		L6474_HelperUnlock(h);
		return val;
	}

	L6474_GetParamCommand(h, STEP_REG_CONFIG);

	L6474_HelperUnlock(h);
	return errcNONE;
}


// --------------------------------------------------------------------------------------------------------------------
int L6474_IsMoving(L6474_Handle_t h, int* moving)
// --------------------------------------------------------------------------------------------------------------------
{
	if ( h == 0 || moving == 0)
		return errcNULL_ARG;

	if ( L6474_HelperLock(h) != 0 )
		return errcLOCKING;

	*moving = h->pending;

	L6474_HelperUnlock(h);
	return errcNONE;
}


// --------------------------------------------------------------------------------------------------------------------
int L6474_SetStepMode(L6474_Handle_t h, L6474x_StepMode_t mode)
// --------------------------------------------------------------------------------------------------------------------
{
	int val = 0;

	if ( h == 0 )
		return errcNULL_ARG;

	if ( mode > smMICRO16 )
		return errcINV_ARG;

	// set this bit. is described in the spec.
	mode |= ( 1 << 3 );

	if ( L6474_HelperLock(h) != 0 )
		return errcLOCKING;

	// forces the device state to update
	L6474_GetStatusCommand(h);

	if ( h->state == stRESET )
	{
		L6474_HelperUnlock(h);
		return errcINV_STATE;
	}

	if ( ( val = L6474_SetParamCommand(h, STEP_REG_STEP_MODE, ( ( mode & STEP_MASK_STEP_MODE ) << STEP_OFFSET_STEP_MODE ) ) ) != 0 )
	{
		L6474_HelperUnlock(h);
		return val;
	}

	L6474_HelperUnlock(h);
	return errcNONE;
}


// --------------------------------------------------------------------------------------------------------------------
int L6474_GetStepMode(L6474_Handle_t h, L6474x_StepMode_t* mode)
// --------------------------------------------------------------------------------------------------------------------
{
	int val = 0;

	if ( h == 0 )
		return errcNULL_ARG;

	if ( mode == 0 )
		return errcNULL_ARG;

	if ( L6474_HelperLock(h) != 0 )
		return errcLOCKING;

	// forces the device state to update
	L6474_GetStatusCommand(h);

	if ( h->state == stRESET )
	{
		L6474_HelperUnlock(h);
		return errcINV_STATE;
	}

	if ( ( val = L6474_GetParamCommand(h, STEP_REG_STEP_MODE ) ) < 0 )
	{
		L6474_HelperUnlock(h);
		return val;
	}

	L6474_HelperUnlock(h);
	*mode = ( ( val & 0x07 ) >> STEP_OFFSET_STEP_MODE );

	return errcNONE;
}


// --------------------------------------------------------------------------------------------------------------------
int L6474_SetPowerOutputs(L6474_Handle_t h, int ena)
// --------------------------------------------------------------------------------------------------------------------
{
	int val = 0;

	if ( h == 0 )
		return errcNULL_ARG;

	if ( L6474_HelperLock(h) != 0 )
		return errcLOCKING;

	// forces the device state to update
	L6474_GetStatusCommand(h);

	if ( h->state == stRESET )
	{
		L6474_HelperUnlock(h);
		return errcINV_STATE;
	}

	if ( ( val = ( ( ( !!ena ) == 0 ) ? L6474_DisableCommand(h) : L6474_EnableCommand(h) ) ) != 0 )
	{
		L6474_HelperUnlock(h);
		return val;
	}

	L6474_HelperUnlock(h);
	return errcNONE;
}


// --------------------------------------------------------------------------------------------------------------------
int L6474_GetAbsolutePosition(L6474_Handle_t h, int* position)
// --------------------------------------------------------------------------------------------------------------------
{
	int val = 0;

	if ( h == 0 || position == 0 )
		return errcNULL_ARG;

	if ( L6474_HelperLock(h) != 0 )
		return errcLOCKING;

	// forces the device state to update
	L6474_GetStatusCommand(h);

	if ( h->state == stRESET )
	{
		L6474_HelperUnlock(h);
		return errcINV_STATE;
	}

	if ( ( val = L6474_GetParamCommand(h, STEP_REG_ABS_POS) ) < 0 )
	{
		L6474_HelperUnlock(h);
		return val;
	}

	if (val & HIGH_POS_BIT)
		val = -(((~val) + 1) & HIGH_POS_MASK);
	*position = val;


	L6474_HelperUnlock(h);
	return errcNONE;
}


// --------------------------------------------------------------------------------------------------------------------
int L6474_SetAbsolutePosition(L6474_Handle_t h, int position)
// --------------------------------------------------------------------------------------------------------------------
{
	int val = 0;

	if ( h == 0 )
		return errcNULL_ARG;

	if ( L6474_HelperLock(h) != 0 )
		return errcLOCKING;

	// forces the device state to update
	L6474_GetStatusCommand(h);

	if ( h->state == stRESET )
	{
		L6474_HelperUnlock(h);
		return errcINV_STATE;
	}

	if ( ( val = L6474_SetParamCommand(h, STEP_REG_ABS_POS, position) ) != 0 )
	{
		L6474_HelperUnlock(h);
		return val;
	}

	L6474_HelperUnlock(h);
	return errcNONE;
}


// --------------------------------------------------------------------------------------------------------------------
int L6474_GetElectricalPosition(L6474_Handle_t h, int* position)
// --------------------------------------------------------------------------------------------------------------------
{
	int val = 0;

	if ( h == 0 || position == 0 )
		return errcNULL_ARG;

	if ( L6474_HelperLock(h) != 0 )
		return errcLOCKING;

	// forces the device state to update
	L6474_GetStatusCommand(h);

	if ( h->state == stRESET )
	{
		L6474_HelperUnlock(h);
		return errcINV_STATE;
	}

	if ( ( val = L6474_GetParamCommand(h, STEP_REG_EL_POS) ) < 0 )
	{
		L6474_HelperUnlock(h);
		return val;
	}

	*position = val;
	L6474_HelperUnlock(h);
	return errcNONE;
}


// --------------------------------------------------------------------------------------------------------------------
int L6474_SetElectricalPosition(L6474_Handle_t h, int position)
// --------------------------------------------------------------------------------------------------------------------
{
	int val = 0;

	if ( h == 0 )
		return errcNULL_ARG;

	if ( L6474_HelperLock(h) != 0 )
		return errcLOCKING;

	// forces the device state to update
	L6474_GetStatusCommand(h);

	if ( h->state == stRESET )
	{
		L6474_HelperUnlock(h);
		return errcINV_STATE;
	}

	if ( ( val = L6474_SetParamCommand(h, STEP_REG_EL_POS, position) ) != 0 )
	{
		L6474_HelperUnlock(h);
		return val;
	}

	L6474_HelperUnlock(h);
	return errcNONE;
}


// --------------------------------------------------------------------------------------------------------------------
int L6474_GetPositionMark(L6474_Handle_t h, int* position)
// --------------------------------------------------------------------------------------------------------------------
{
	int val = 0;

	if ( h == 0 || position == 0 )
		return errcNULL_ARG;

	if ( L6474_HelperLock(h) != 0 )
		return errcLOCKING;

	// forces the device state to update
	L6474_GetStatusCommand(h);

	if ( h->state == stRESET )
	{
		L6474_HelperUnlock(h);
		return errcINV_STATE;
	}

	if ( ( val = L6474_GetParamCommand(h, STEP_REG_MARK) ) < 0 )
	{
		L6474_HelperUnlock(h);
		return val;
	}

	if (val & HIGH_POS_BIT)
		val = -(((~val) + 1) & HIGH_POS_MASK);
	*position = val;

	L6474_HelperUnlock(h);
	return errcNONE;
}


// --------------------------------------------------------------------------------------------------------------------
int L6474_SetPositionMark(L6474_Handle_t h, int position)
// --------------------------------------------------------------------------------------------------------------------
{
	int val = 0;

	if ( h == 0 )
		return errcNULL_ARG;

	if ( L6474_HelperLock(h) != 0 )
		return errcLOCKING;

	// forces the device state to update
	L6474_GetStatusCommand(h);

	if ( h->state == stRESET )
	{
		L6474_HelperUnlock(h);
		return errcINV_STATE;
	}

	if ( ( val = L6474_SetParamCommand(h, STEP_REG_MARK, position) ) != 0 )
	{
		L6474_HelperUnlock(h);
		return val;
	}

	L6474_HelperUnlock(h);
	return errcNONE;
}


// --------------------------------------------------------------------------------------------------------------------
int L6474_GetAlarmEnables(L6474_Handle_t h, int* bits)
// --------------------------------------------------------------------------------------------------------------------
{
	int val = 0;

	if ( h == 0 || bits == 0 )
		return errcNULL_ARG;

	if ( L6474_HelperLock(h) != 0 )
		return errcLOCKING;

	// forces the device state to update
	L6474_GetStatusCommand(h);

	if ( h->state == stRESET )
	{
		L6474_HelperUnlock(h);
		return errcINV_STATE;
	}

	if ( ( val = L6474_GetParamCommand(h, STEP_REG_ALARM_EN) ) < 0 )
	{
		L6474_HelperUnlock(h);
		return val;
	}

	*bits = val;
	L6474_HelperUnlock(h);
	return errcNONE;
}


// --------------------------------------------------------------------------------------------------------------------
int L6474_SetProperty(L6474_Handle_t h, L6474_Property_t prop, int value)
// -------------------------------------------------------------------------------------------------------------------
{
	int val = 0;

	if ( h == 0 )
		return errcNULL_ARG;

	if ( L6474_HelperLock(h) != 0 )
		return errcLOCKING;

	// forces the device state to update
	L6474_GetStatusCommand(h);

	if ( h->state == stRESET )
	{
		L6474_HelperUnlock(h);
		return errcINV_STATE;
	}

	if ( ( val = L6474_SetParamCommand(h, prop, value) ) != 0 )
	{
		L6474_HelperUnlock(h);
		return val;
	}

	L6474_HelperUnlock(h);
	return errcNONE;
}


// --------------------------------------------------------------------------------------------------------------------
int L6474_GetProperty(L6474_Handle_t h, L6474_Property_t prop, int* value)
// --------------------------------------------------------------------------------------------------------------------
{
	int val = 0;

	if ( h == 0 )
		return errcNULL_ARG;

	if ( L6474_HelperLock(h) != 0 )
		return errcLOCKING;

	// forces the device state to update
	L6474_GetStatusCommand(h);

	if ( h->state == stRESET )
	{
		L6474_HelperUnlock(h);
		return errcINV_STATE;
	}

	if ( ( val = L6474_GetParamCommand(h, prop) ) < 0 )
	{
		L6474_HelperUnlock(h);
		return val;
	}

	*value = val;
	L6474_HelperUnlock(h);
	return errcNONE;
}


// --------------------------------------------------------------------------------------------------------------------
int L6474_SetAlarmEnables(L6474_Handle_t h, int bits)
// --------------------------------------------------------------------------------------------------------------------
{
	int val = 0;

	if ( h == 0 )
		return errcNULL_ARG;

	if ( L6474_HelperLock(h) != 0 )
		return errcLOCKING;

	// forces the device state to update
	L6474_GetStatusCommand(h);

	if ( h->state == stRESET )
	{
		L6474_HelperUnlock(h);
		return errcINV_STATE;
	}

	if ( ( val = L6474_SetParamCommand(h, STEP_REG_ALARM_EN, bits) ) != 0 )
	{
		L6474_HelperUnlock(h);
		return val;
	}

	L6474_HelperUnlock(h);
	return errcNONE;
}


// --------------------------------------------------------------------------------------------------------------------
int L6474_GetStatus(L6474_Handle_t h, L6474_Status_t* status)
// --------------------------------------------------------------------------------------------------------------------
{
	int val = 0;

	if ( h == 0 )
		return errcNULL_ARG;

	if (status == 0 )
		return errcNULL_ARG;

	if ( L6474_HelperLock(h) != 0 )
		return errcLOCKING;

	// forces the device state to update
	L6474_GetStatusCommand(h);

	if ( ( val = ( L6474_GetStatusCommand(h) ) ) < 0 )
	{
		L6474_HelperUnlock(h);
		return val;
	}

	if ( h->state == stRESET )
	{
		L6474_HelperUnlock(h);
		return errcINV_STATE;
	}

	status->HIGHZ       = (val & STATUS_HIGHZ_MASK)       ? 1 : 0;
	status->DIR         = (val & STATUS_DIRECTION_MASK)   ? 1 : 0;
	status->NOTPERF_CMD = (val & STATUS_NOTPERF_CMD_MASK) ? 1 : 0;
	status->WRONG_CMD   = (val & STATUS_WRONG_CMD_MASK)   ? 1 : 0;
	status->UVLO        = (val & STATUS_UNDERVOLT_MASK)   ? 0 : 1;
	status->TH_WARN     = (val & STATUS_THR_WARN_MASK)    ? 0 : 1;
	status->TH_SD       = (val & STATUS_THR_SHORTD_MASK)  ? 0 : 1;
	status->OCD         = (val & STATUS_OCD_MASK)         ? 0 : 1;
	status->ONGOING     = h->pending;

	L6474_HelperUnlock(h);
	return errcNONE;
}

// --------------------------------------------------------------------------------------------------------------------
int L6474_GetState(L6474_Handle_t h, L6474x_State_t* state)
// --------------------------------------------------------------------------------------------------------------------
{
	if (h == 0)
		return errcNULL_ARG;

	if (state == 0)
		return errcNULL_ARG;

	*state = h->state;

	return errcNONE;
}

// --------------------------------------------------------------------------------------------------------------------
int L6474_StopMovement(L6474_Handle_t h)
// --------------------------------------------------------------------------------------------------------------------
{
	if ( h == 0 )
		return errcNULL_ARG;

	if ( L6474_HelperLock(h) != 0 )
		return errcLOCKING;

	// forces the device state to update
	L6474_GetStatusCommand(h);

	if ( h->state != stENABLED )
	{
		L6474_HelperUnlock(h);
		return errcNONE;
	}

	if ( h->pending == 0 )
	{
		L6474_HelperUnlock(h);
		return errcNONE;
	}

#if defined(LIBL6474_STEP_ASYNC) && ( LIBL6474_STEP_ASYNC == 1 )
	if ( h->platform.cancelStep(h->pPWM) != 0 )
	{
		L6474_HelperUnlock(h);
		return errcINTERNAL;
	}
	h->pending = 0;
#else
	L6474_HelperUnlock(h);
	return errcINTERNAL;
#endif

	L6474_HelperUnlock(h);
	return errcNONE;
}

// --------------------------------------------------------------------------------------------------------------------
int L6474_StepIncremental(L6474_Handle_t h, int steps )
// --------------------------------------------------------------------------------------------------------------------
{
	if ( h == 0 )
		return errcNULL_ARG;

	if ( steps == 0 )
		return errcNULL_ARG;

	if ( L6474_HelperLock(h) != 0 )
		return errcLOCKING;

	// forces the device state to update
	L6474_GetStatusCommand(h);

	if ( h->state != stENABLED )
	{
		L6474_HelperUnlock(h);
		return errcINV_STATE;
	}

	if ( h->pending != 0 )
	{
		L6474_HelperUnlock(h);
		return errcPENDING;
	}

	int ret = 0;
#if defined(LIBL6474_STEP_ASYNC) && ( LIBL6474_STEP_ASYNC == 1 )
	h->pending = 1;
	if ( ( ret = h->platform.stepAsync(h->pPWM, steps >= 0, ( ( steps < 0 ) ? -steps : steps ), L6474_HelperReleaseStep, h) ) != 0 )
	{
		h->pending = 0;
	}
#else
	h->pending = 1;
	(void)L6474_HelperReleaseStep;
	ret = h->platform.step(h->pPWM,  steps >= 0, ( ( steps < 0 ) ? -steps : steps ) );
	h->pending = 0;
#endif

	if ( ret != 0 )
	{
		L6474_HelperUnlock(h);
		return errcINTERNAL;
	}

	L6474_HelperUnlock(h);
	return errcNONE;
}

