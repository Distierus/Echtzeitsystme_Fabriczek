#include "main.h"
#include "Console_implementation/my_console.h"
#include "LibL6474.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "LibL6474Config.h"
#include "stm32f7xx_hal_spi.h"
#include "stm32f7xx_hal_tim.h"
#include "FreeRTOS.h"
#include "task.h"

// --- Motor-Parameter ---
// Das sind die Grunddaten von unserem Motor und der Mechanik.
#define STEPS_PER_TURN 200  // Standard 1.8 Grad Motor
#define RESOLUTION 16       // Fahren im 1/16 Mikroschritt-Modus
#define MM_PER_TURN 4       // Spindelsteigung, 4mm pro Umdrehung

// --- StepperContext ---
// Das ist quasi unser globales "Gehirn" für den Motor.
// Hier speichern wir den ganzen Zustand, die Handles und die Konfiguration.
typedef struct {
    L6474_Handle_t h;       // Handle für die L6474-Bibliothek
    int is_powered;         // Ist der Motor bestromt? (High-Z oder nicht)
    int is_referenced;      // Haben wir die Home-Position gefunden?
    int is_running;         // Bewegt sich der Motor gerade?

    void (*done_callback)(L6474_Handle_t); // Callback, wenn Bewegung fertig ist
    int remaining_pulses;                  // Für Bewegungen > 65535 Schritte
    TIM_HandleTypeDef* htim1_handle; // Timer für die Puls-ZÄHLUNG (One-Pulse-Mode)
    TIM_HandleTypeDef* htim4_handle; // Timer für die Puls-GENERIERUNG (PWM-Takt)

    // Mechanik-Config
    int steps_per_turn;
    int resolution;
    float mm_per_turn;

    // Software-Endlagen (in Mikroschritten)
    int position_min_steps;
    int position_max_steps;
    int position_ref_steps; // Wo ist die "Nullposition" nach dem Homing?
} StepperContext;

static int StepTimerCancelAsync(void* pPWM);
void set_speed(StepperContext* stepper_ctx, int steps_per_second);

// --- Library Hooks ---
// Die L6474-Lib braucht ein paar Funktionen von uns, um auf die Hardware
// zuzugreifen. Das hier sind die "Brücken".

static void* StepLibraryMalloc( unsigned int size )
{
    // Einfach an FreeRTOS (oder stdlib) weiterleiten.
    return malloc(size);
}

static void StepLibraryFree( const void* const ptr )
{
    // ...und das Gegenstück.
    free((void*)ptr);
}

static int StepDriverSpiTransfer( void* pIO, char* pRX, const char* pTX, unsigned int length )
{
    // byte based access, so keep in mind that only single byte transfers are performed!
    // Die Lib macht nur Byte-Transfers.

    HAL_StatusTypeDef status = 0;

    for ( unsigned int i = 0; i < length; i++ )
    {
        // FIXME: Das ist super lahm. Wir toggeln CS für JEDES Byte.
        .
        HAL_GPIO_WritePin(STEP_SPI_CS_GPIO_Port, STEP_SPI_CS_Pin, 0); // CS Low

        // Senden und empfangen
        status |= HAL_SPI_TransmitReceive(pIO, (uint8_t*)pTX + i, (uint8_t*)pRX + i, 1, 10000);

        HAL_GPIO_WritePin(STEP_SPI_CS_GPIO_Port, STEP_SPI_CS_Pin, 1); // CS High

        // Sicher ist sicher, 'n kurzes Delay.
        HAL_Delay(1);
    }

    if (status != HAL_OK) {
        return -1; // Irgendwas ist schiefgelaufen
    }

    return 0; // Alles gut
}

static void StepDriverReset(void* pGPO, int ena)
{
    // Einfacher Pin-Wackler für den Reset-Pin des L6474.
    (void) pGPO; // pGPO brauchen wir nicht, Pin ist fix
    HAL_GPIO_WritePin(STEP_RSTN_GPIO_Port, STEP_RSTN_Pin, !ena); // Aktiv-Low
}

static void StepLibraryDelay()
{
    // NOP. Wird anscheinend von der Lib für Delays gebraucht,
    // aber wir machen das blockierend in der SPI-Funktion.
    return;
}


/*typedef struct {
    void *pPWM;
    int dir;
    unsigned int numPulses;
    void (*doneClb)(L6474_Handle_t);
    L6474_Handle_t h;
} StepTaskParams;*/


static int reset(StepperContext* stepper_ctx){
    // Setzt den L6474 auf Werkseinstellungen und unsere Defaults.
    L6474_BaseParameter_t param;
    param.stepMode = smMICRO16;   // 1/16 Mikroschritte
    param.OcdTh = ocdth6000mA;    // Überstromschutz
    param.TimeOnMin = 0x29;       // Magische PWM-Werte
    param.TimeOffMin = 0x29;
    param.TorqueVal = 0x26;       // Haltemoment
    param.TFast = 0x19;

    int result = 0;

    result |= L6474_ResetStandBy(stepper_ctx->h); // Erstmal resetten
    result |= L6474_Initialize(stepper_ctx->h, &param); // Dann mit unseren Werten initialisieren
    result |= L6474_SetPowerOutputs(stepper_ctx->h, 0); // Und Motoren aus (High-Z)

    // Internen Zustand auch zurücksetzen
    stepper_ctx->is_powered = 0;
    stepper_ctx->is_referenced = 0;
    stepper_ctx->is_running = 0;

    return result;
}

static int powerena(StepperContext* stepper_ctx, int argc, char** argv) {
    // Konsolenbefehl: `config powerena` (Status) oder `config powerena -v 1` (Setzen)
    if (argc == 2) {
        // Nur Status abfragen
        printf("Current Powerstate: %d\r\n", stepper_ctx->is_powered);
        return 0;
    }
    else if (argc == 4 && strcmp(argv[2], "-v") == 0) {
        // Wert setzen
        int ena = atoi(argv[3]);
        if (ena != 0 && ena != 1) {
            printf("Invalid argument for powerena\r\n");
            return -1;
        }
        stepper_ctx->is_powered = ena;
        return L6474_SetPowerOutputs(stepper_ctx->h, ena); // An den Treiber schicken
    }
    else {
        printf("Invalid number of arguments\r\n");
        return -1;
    }
}

static int reference(StepperContext* stepper_ctx, int argc, char** argv) {
    // Homing-Routine. Fährt auf einen Referenzschalter.
    int result = 0;
    int poweroutput = 0; // Sollen wir danach an oder aus sein? Default: aus.
    int is_skip = 0;     // Homing überspringen?
    uint32_t timeout_ms = 0; // Timeout, falls Schalter klemmt

    // Argumente parsen
    for (int i = 1; i < argc; ) {
        // skip
        if (strcmp(argv[i], "-s") == 0) {
            is_skip = 1;
            i++;
        }
        // power (enable after)
        else if (strcmp(argv[i], "-e") == 0) {
            poweroutput = 1;
            i++;
        }
        // timeout
        else if (strcmp(argv[i], "-t") == 0) {
            if (i == argc - 1) {
                printf("Invalid number of arguments\r\n");
                return -1;
            }
            timeout_ms = atoi(argv[2]) * 1000;
            if (timeout_ms <= 0) {
                printf("Invalid timeout value\r\n");
                return -1;
            }
            i += 2;
        }
        else {
            printf("Invalid Flag\r\n");
            return -1;
        }
    }


    if (!is_skip) {
        // Homing wirklich durchführen
        const uint32_t start_time = HAL_GetTick();
        result |= L6474_SetPowerOutputs(stepper_ctx->h, 1); // Strom an
        set_speed(stepper_ctx, 3000); // Feste Homing-Geschwindigkeit

        // Fall 1: Wir stehen schon auf dem Schalter.
        if(HAL_GPIO_ReadPin(REFERENCE_MARK_GPIO_Port, REFERENCE_MARK_Pin) == GPIO_PIN_RESET) {
            // Erstmal vom Schalter runterfahren (ins Positive)
            L6474_StepIncremental(stepper_ctx->h, 100000000); // "unendlich" weit fahren
            while(HAL_GPIO_ReadPin(REFERENCE_MARK_GPIO_Port, REFERENCE_MARK_Pin) == GPIO_PIN_RESET){
                // Timeout-Check
                if (timeout_ms > 0 && HAL_GetTick() - start_time > timeout_ms) {
                    StepTimerCancelAsync(NULL); // Bewegung stoppen
                    printf("Timeout while waiting for reference switch\r\n");
                    return -1;
                }
            }
            StepTimerCancelAsync(NULL); // Bewegung stoppen
        }

        // Fall 2: Wir sind nicht auf dem Schalter.
        // Langsam draufzufahren (ins Negative)
        L6474_StepIncremental(stepper_ctx->h, -1000000000); // "unendlich" weit fahren
        while(HAL_GPIO_ReadPin(REFERENCE_MARK_GPIO_Port, REFERENCE_MARK_Pin) != GPIO_PIN_RESET && result == 0) {
            // Timeout-Check
            if (timeout_ms > 0 && HAL_GetTick() - start_time > timeout_ms) {
                StepTimerCancelAsync(NULL);
                printf("Timeout while waiting for reference switch\r\n");
                result = -1;
            }
        }
        StepTimerCancelAsync(NULL); // Bewegung stoppen
    }

    if (result == 0) {
        // Homing war erfolgreich (oder wurde geskippt)
        stepper_ctx->is_referenced = 1;
        // Absolute Position auf unseren Referenzwert setzen
        L6474_SetAbsolutePosition(stepper_ctx->h, stepper_ctx->position_ref_steps);
    }

    // Strom wieder so schalten, wie per Flag gewünscht
    result |= L6474_SetPowerOutputs(stepper_ctx->h, poweroutput);
    stepper_ctx->is_powered = poweroutput;
    return result;
}


static int config(StepperContext* stepper_ctx, int argc, char** argv) {
    // Riesen-Parser für alle `config ...` Befehle.
    if (argc < 2) {
        printf("Invalid number of arguments\r\n");
        return -1;
    }
    if (strcmp(argv[1], "powerena") == 0) {
        return powerena(stepper_ctx, argc, argv);
    }
    else if(strcmp(argv[1], "torque") == 0){
        // `config torque` (get) oder `config torque -v 38` (set)
        if (argc == 2) {
            int value = 0;
            L6474_GetProperty(stepper_ctx->h, L6474_PROP_TORQUE, &value);
            printf("%d\r\n", value);
            return 0;
        }
        else if (argc == 4 && strcmp(argv[2], "-v") == 0) {
            return L6474_SetProperty(stepper_ctx->h, L6474_PROP_TORQUE, atoi(argv[3]));
        }
        else {
            printf("Invalid number of arguments\r\n");
            return -1;
        }
    }
    else if(strcmp(argv[1], "timeon") == 0){
        // WICHTIG: Diese Werte nur ändern, wenn Motor aus ist!
        if(stepper_ctx->is_powered == 1){
            printf("Power must be off to change this setting\r\n"); // Eigene Fehlermeldung
            return -1;
        }
        if (argc == 2) {
            int value = 0;
            L6474_GetProperty(stepper_ctx->h, L6474_PROP_TON, &value);
            printf("%d\r\n", value);
            return 0;
        }
        else if (argc == 4 && strcmp(argv[2], "-v") == 0) {
            return L6474_SetProperty(stepper_ctx->h, L6474_PROP_TON, atoi(argv[3]));
        }
        else {
            printf("Invalid number of arguments\r\n");
            return -1;
        }
    }
    else if(strcmp(argv[1], "timeoff") == 0){
        // WICHTIG: Diese Werte nur ändern, wenn Motor aus ist!
        if(stepper_ctx->is_powered == 1){
            printf("Power must be off to change this setting\r\n");
            return -1;
        }
        if (argc == 2) {
            int value = 0;
            L6474_GetProperty(stepper_ctx->h, L6474_PROP_TOFF, &value);
            printf("%d\r\n", value);
            return 0;
        }
        else if (argc == 4 && strcmp(argv[2], "-v") == 0) {
            return L6474_SetProperty(stepper_ctx->h, L6474_PROP_TOFF, atoi(argv[3]));
        }
        else {
            printf("Invalid number of arguments\r\n");
            return -1;
        }
    }
    else if(strcmp(argv[1], "timefast") == 0){
        // WICHTIG: Diese Werte nur ändern, wenn Motor aus ist!
        if(stepper_ctx->is_powered == 1){
            printf("Power must be off to change this setting\r\n");
            return -1;
        }
        if (argc == 2) {
            int value = 0;
            L6474_GetProperty(stepper_ctx->h, L6474_PROP_TFAST, &value);
            printf("%d\r\n", value);
            return 0;
        }
        else if (argc == 4 && strcmp(argv[2], "-v") == 0) {
            return L6474_SetProperty(stepper_ctx->h, L6474_PROP_TFAST, atoi(argv[3]));
        }
        else {
            printf("Invalid number of arguments\r\n");
            return -1;
        }
    }
    else if(strcmp(argv[1], "throvercurr") == 0){
        // Overcurrent Threshold
        if (argc == 2) {
            int value = 0;
            L6474_GetProperty(stepper_ctx->h, L6474_PROP_OCDTH, &value);
            printf("%d\r\n", value);
            return 0;
        }
        else if (argc == 4 && strcmp(argv[2], "-v") == 0) {
            return L6474_SetProperty(stepper_ctx->h, L6474_PROP_OCDTH, atoi(argv[3]));
        }
        else {
            printf("Invalid number of arguments\r\n");
            return -1;
        }
    }
    else if(strcmp(argv[1], "stepmode") == 0){
        // WICHTIG: Diese Werte nur ändern, wenn Motor aus ist!
        if(stepper_ctx->is_powered == 1){
            printf("Power must be off to change this setting\r\n");
            return -1;
        }
        if (argc == 2) {
            // Wir geben unsere interne 'resolution' zurück, nicht den Registerwert
            printf("%d\r\n", stepper_ctx->resolution);
            return 0;
        }
        else if (argc == 4 && strcmp(argv[2], "-v") == 0) {
            int resolution = atoi(argv[3]);
            L6474x_StepMode_t step_mode;

            // Mapping von Zahl auf Enum-Wert
            switch (resolution) {
                case 1: step_mode = smFULL; break;
                case 2: step_mode = smHALF; break;
                case 4: step_mode = smMICRO4; break;
                case 8: step_mode = smMICRO8; break;
                case 16: step_mode = smMICRO16; break;
                default:
                    printf("Invalid step mode\r\n");
                    return -1;
            }
            stepper_ctx->resolution = resolution; // Internen Wert aktualisieren
            return L6474_SetStepMode(stepper_ctx->h, step_mode); // An Treiber schicken
        }
        else {
            printf("Invalid number of arguments\r\n");
            return -1;
        }
    }
    // Die folgenden Werte sind rein software-basiert (in unserem Context).
    // Der L6474 weiß nichts von mm oder Umdrehungen.
    else if(strcmp(argv[1], "stepsperturn") == 0){
        if (argc == 2) {
            printf("%d\r\n", stepper_ctx->steps_per_turn);
            return 0;
        }
        else if (argc == 4 && strcmp(argv[2], "-v") == 0) {
            stepper_ctx->steps_per_turn = atoi(argv[3]);
            return 0;
        }
        else {
            printf("Invalid number of arguments\r\n");
            return -1;
        }
    }
    else if(strcmp(argv[1], "mmperturn") == 0){
        if (argc == 2) {
            printf("%f\r\n", stepper_ctx->mm_per_turn);
            return 0;
        }
        else if (argc == 4 && strcmp(argv[2], "-v") == 0) {
            stepper_ctx->mm_per_turn = atoff(argv[3]);
            return 0;
        }
        else {
            printf("Invalid number of arguments\r\n");
            return -1;
        }
    }
    else if(strcmp(argv[1], "posmin") == 0){
        // Software-Endlage MIN (in mm)
        if (argc == 2) {
            // Umrechnen von Steps -> mm
            printf("%f\r\n", (float)(stepper_ctx->position_min_steps * stepper_ctx->mm_per_turn) / (float)(stepper_ctx->steps_per_turn  * stepper_ctx->resolution));
            return 0;
        }
        else if (argc == 4 && strcmp(argv[2], "-v") == 0) {
            float value_float = atoff(argv[3]);
            // Umrechnen von mm -> Steps
            stepper_ctx->position_min_steps = (value_float * stepper_ctx->steps_per_turn  * stepper_ctx->resolution) / stepper_ctx->mm_per_turn;;
            return 0;
        }
        else {
            printf("Invalid number of arguments\r\n");
            return -1;
        }
    }
    else if(strcmp(argv[1], "posmax") == 0){
        // Software-Endlage MAX (in mm)
        if (argc == 2) {
            printf("%f\r\n", (float)(stepper_ctx->position_max_steps * stepper_ctx->mm_per_turn) / (float)(stepper_ctx->steps_per_turn  * stepper_ctx->resolution));
            return 0;
        }
        else if (argc == 4 && strcmp(argv[2], "-v") == 0) {
            float value_float = atoff(argv[3]);
            stepper_ctx->position_max_steps = (value_float * stepper_ctx->steps_per_turn  * stepper_ctx->resolution) / stepper_ctx->mm_per_turn;;
            return 0;
        }
        else {
            printf("Invalid number of arguments\r\n");
            return -1;
        }
    }
    else if(strcmp(argv[1], "posref") == 0){
        // Referenz-Position (in mm)
        if (argc == 2) {
            printf("%f\r\n", (float)(stepper_ctx->position_ref_steps * stepper_ctx->mm_per_turn) / (float)(stepper_ctx->steps_per_turn  * stepper_ctx->resolution));
            return 0;
        }
        else if (argc == 4 && strcmp(argv[2], "-v") == 0) {
            float value_float = atoff(argv[3]);
            stepper_ctx->position_ref_steps = (value_float * stepper_ctx->steps_per_turn  * stepper_ctx->resolution) / stepper_ctx->mm_per_turn;;
            return 0;
        }
        else {
            printf("Invalid number of arguments\r\n");
            return -1;
        }
    }
    else {
        printf("Invalid command\r\n");
        return -1;
    }
}

void set_speed(StepperContext* stepper_ctx, int steps_per_second) {
    // Diese Funktion stellt den TIM4 (PWM-Generator) so ein,
    // dass er die Taktfrequenz 'steps_per_second' auf dem STCK-Pin ausgibt.
    int clk = HAL_RCC_GetHCLKFreq(); // Systemtakt holen

    // Wir brauchen 'steps_per_second' Pulse. Der Timer zählt hoch (ARR) und
    // toggelt bei Compare (CCR). Um eine Frequenz X zu kriegen, brauchen
    // wir eine Periode von clk / X.
    // Die "magische 2" kommt wahrscheinlich vom PWM-Mode (Toggle?).
    int quotient = clk / (steps_per_second * 2);

    // Der Prescaler (PSC) und der Auto-Reload (ARR) sind nur 16-bit.
    // Wenn der 'quotient' zu groß ist, müssen wir einen Prescaler finden.
    int i = 0; // Das wird der Prescaler-Wert (also i+1)
    while ((quotient / (i + 1)) > 65535) i++; // Finde kleinstes 'i', so dass ARR < 65535

    __HAL_TIM_SET_PRESCALER(stepper_ctx->htim4_handle, i);
    __HAL_TIM_SET_AUTORELOAD(stepper_ctx->htim4_handle, (quotient / (i + 1)) - 1);
    // Duty-Cycle auf 50% setzen
    stepper_ctx->htim4_handle->Instance->CCR4 = stepper_ctx->htim4_handle->Instance->ARR / 2;
}

static int move(StepperContext* stepper_ctx, int argc, char** argv) {
    // Konsolenbefehl: `move <pos> [-a] [-r] [-s <speed>]`

    // Sicherheitsabfragen
    if (stepper_ctx->is_powered != 1) {
        printf("Stepper not powered\r\n");
        return -1;
    }
    if (stepper_ctx->is_referenced != 1) {
        printf("Stepper not referenced\r\n");
        return -1;
    }
    if (argc < 2) {
        printf("Invalid number of arguments\r\n");
        return -1;
    }

    int position = atoi(argv[1]); // Zielposition in mm
    int speed = 1000; // Default-Speed in mm/min

    // Flags parsen
    int is_async = 0; // Warten bis fertig?
    int is_relative = 0; // Relativ oder absolut?

    for (int i = 2; i < argc; ) {
        // async
        if (strcmp(argv[i], "-a") == 0) {
            is_async = 1;
            i++;
        }
        // relative
        else if (strcmp(argv[i], "-r") == 0) {
            is_relative = 1;
            i++;
        }
        // speed
        else if (strcmp(argv[i], "-s") == 0) {
            if (i == argc - 1) {
                printf("Invalid number of arguments\r\n");
                return -1;
            }
            speed = atoi(argv[i + 1]);
            i += 2;
        }
        else {
            printf("Invalid Flag\r\n");
            return -1;
        }
    }

    // --- Umrechnungen ---
    // 1. Geschwindigkeit: von mm/min -> steps/sec
    // (speed [mm/min] / 60 [s/min]) * (steps_per_turn * resolution [steps/turn]) / (mm_per_turn [mm/turn])
    // -> (speed * steps_per_turn * resolution) / (60 * mm_per_turn) [steps/sec]
    int steps_per_second = (speed * stepper_ctx->steps_per_turn * stepper_ctx->resolution) / (60 * stepper_ctx->mm_per_turn);

    if (steps_per_second < 1) {
        printf("Speed too small\r\n");
        return -1;
    }

    set_speed(stepper_ctx, steps_per_second); // TIM4 (PWM-Takt) einstellen

    // 2. Position: von mm -> steps
    // (position [mm] * (steps_per_turn * resolution [steps/turn])) / (mm_per_turn [mm/turn])
    int steps = (position * stepper_ctx->steps_per_turn  * stepper_ctx->resolution) / stepper_ctx->mm_per_turn;

    if (!is_relative) {
        // Absolute Bewegung
        int absolute_position;
        L6474_GetAbsolutePosition(stepper_ctx->h, &absolute_position);
        steps -= absolute_position; // Differenz zur aktuellen Position bilden
    }

    // Keine Bewegung nötig
    if (steps == 0 || steps == -1 || steps == 1) {
        printf("No movement\r\n");
        return -1;
    }

    // --- Soft-Limit Check ---
    int resulting_steps;
    L6474_GetAbsolutePosition(stepper_ctx->h, &resulting_steps);
    resulting_steps += steps; // Wo landen wir?

    if (resulting_steps < stepper_ctx->position_min_steps || resulting_steps > stepper_ctx->position_max_steps) {
        printf("Position out of bounds\r\n");
        return -1;
    }

    // --- Bewegung ausführen ---
    if (is_async) {
        // Befehl absetzen und sofort zurückkehren
        return L6474_StepIncremental(stepper_ctx->h, steps);
    }
    else {
        // Befehl absetzen und warten, bis `is_running` false gesetzt wird
        // (passiert im TIM1-Interrupt-Callback)
        int result = L6474_StepIncremental(stepper_ctx->h, steps);
        while (stepper_ctx->is_running); // Blockierendes Warten
        return result;
    }
}

static int initialize(StepperContext* stepper_ctx) {
    // 'init' Befehl. Setzt alles zurück, schaltet Strom an
    // und (VORSICHT!) tut so, als wären wir referenziert.
    // Nützlich für schnelle Tests ohne Homing.
    reset(stepper_ctx);
    stepper_ctx->is_powered = 1;
    stepper_ctx->is_referenced = 1;

    return L6474_SetPowerOutputs(stepper_ctx->h, 1);
}

static int stepperConsoleFunction(int argc, char** argv, void* ctx) {
    // Der Haupt-Einstiegspunkt für alle "stepper ..." Konsolenbefehle.
    // Das ist nur ein großer Verteiler, der den ersten Befehl (argv[0])
    // prüft und an die jeweilige Sub-Funktion (move, reset, config, ...) weiterleitet.

    StepperContext* stepper_ctx = (StepperContext*)ctx;
    int result = 0;

    if (argc == 0) {
        printf("Invalid number of arguments\r\n");
        return -1;
    }
    if (strcmp(argv[0], "move") == 0 )
    {
        result = move(stepper_ctx, argc, argv);
    }
    else if (strcmp(argv[0], "reset") == 0) {
        result = reset(stepper_ctx);
    }
    else if (strcmp(argv[0], "config") == 0) {
        result = config(stepper_ctx, argc, argv);
    }
    else if (strcmp(argv[0], "reference") == 0) {
        result = reference(stepper_ctx, argc, argv);
    }
    else if (strcmp(argv[0], "cancel") == 0) {
        // Harter Abbruch der aktuellen Bewegung
        result = StepTimerCancelAsync(NULL);
    }
    else if (strcmp(argv[0], "init") == 0){
        result = initialize(stepper_ctx);
    }
    else if (strcmp(argv[0], "position") == 0){
        // Aktuelle Position in mm ausgeben
        int position;
        L6474_GetAbsolutePosition(stepper_ctx->h, &position);
        printf("%.4f\r\n", (float)(position * stepper_ctx->mm_per_turn) / (float)(stepper_ctx->steps_per_turn  * stepper_ctx->resolution));
    }
    else if (strcmp(argv[0], "status") == 0){
        // Internen Status und L6474-Statusregister ausgeben
        int status;
        // Unser interner Status
        if (stepper_ctx->is_powered == 0 && stepper_ctx->is_referenced == 0) {
            status = 0x0; // Not Powered, Not Referenced
        }
        else if(stepper_ctx->is_powered == 1 && stepper_ctx->is_referenced == 0) {
            status = 0x1; // Powered, Not Referenced
        }
        else if(stepper_ctx->is_powered == 0 && stepper_ctx->is_referenced == 1) {
            status = 0x2; // Not Powered, Referenced
        }
        else {
            status = 0x4; // Powered and Referenced (Ready)
        }

        // L6474-Fehlerstatus prüfen
        L6474_Status_t status_struct;
        L6474_GetStatus(stepper_ctx->h, &status_struct);
        if (status_struct.NOTPERF_CMD != 0 || status_struct.OCD != 0 || status_struct.TH_SD != 0|| status_struct.TH_WARN != 0 || status_struct.UVLO != 0 || status_struct.WRONG_CMD != 0) {
            status = 0x8; // Error-State
        }

        // Rohes Statusregister zusammenbauen
        int out_status = 0;
        out_status |= (status_struct.DIR << 0);
        out_status |= (status_struct.HIGHZ << 1);
        out_status |= (status_struct.NOTPERF_CMD << 2);
        out_status |= (status_struct.OCD << 3);
        out_status |= (status_struct.ONGOING << 4);
        out_status |= (status_struct.TH_SD << 5);
        out_status |= (status_struct.TH_WARN << 6);
        out_status |= (status_struct.UVLO << 7);
        out_status |= (status_struct.WRONG_CMD << 8);

        // Ausgabe: Unser Status, L6474-Status, is_running Flag
        printf("0x%x\r\n0x%x\r\n%d\r\n", status, out_status, stepper_ctx->is_running);
    }
    else {
        printf("Invalid command\r\n");
        return -1;
    }

    // Standard-Antwort für die Konsole
    if (result == 0) {
        printf("OK\r\n");
    }
    else {
        printf("FAIL\r\n");
    }
    return result;
}

L6474x_Platform_t p; // Platform-Struct für die Lib-Hooks
StepperContext stepper_ctx; // Unser "Gehirn"

// --- Timer-Logik für Schritt-ZÄHLUNG ---

void start_tim1(int pulses) {
    // Startet den TIM1 im One-Pulse-Mode, um GENAU 'pulses' Schritte zu zählen.
    // Der Takt kommt von TIM4 (externer Trigger).

    // Workaround: Der ARR-Register (Counter) ist nur 16-bit (max 65535).
    // Wenn wir mehr Pulse brauchen, müssen wir stückeln.
    int current_pulses = (pulses >= 65535) ? 65535 : pulses;
    stepper_ctx.remaining_pulses = pulses - current_pulses;

    if (current_pulses != 1) { // 1-Puls-Fahrten machen manchmal Probleme
        HAL_TIM_OnePulse_Stop_IT(stepper_ctx.htim1_handle, TIM_CHANNEL_1);
        __HAL_TIM_SET_AUTORELOAD(stepper_ctx.htim1_handle, current_pulses); // Wie viele Pulse zählen?
        HAL_TIM_GenerateEvent(stepper_ctx.htim1_handle, TIM_EVENTSOURCE_UPDATE); // Register laden
        HAL_TIM_OnePulse_Start_IT(stepper_ctx.htim1_handle, TIM_CHANNEL_1); // Scharfschalten
        __HAL_TIM_ENABLE(stepper_ctx.htim1_handle); // Los!
    }
    else {
        // Bewegung überspringen, direkt "fertig" melden.
        stepper_ctx.done_callback(stepper_ctx.h);
    }
}


void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef* htim) {
    // --- !! WICHTIGER INTERRUPT !! ---
    // Das ist der Callback von TIM1. Er wird ausgelöst, wenn
    // der One-Pulse-Mode seine 'current_pulses' gezählt hat.

    // Sicherstellen, dass der Callback gesetzt ist und es der richtige IRQ ist
    if ((stepper_ctx.done_callback != 0) && ((htim->Instance->SR & (1 << 2)) == 0)) {
        if (stepper_ctx.remaining_pulses > 0) {
            // Wir sind noch nicht fertig, nächsten Chunk starten
            start_tim1(stepper_ctx.remaining_pulses);
        }
        else {
            // Wir sind fertig.
            stepper_ctx.done_callback(stepper_ctx.h); // Der Lib Bescheid sagen
            stepper_ctx.is_running = 0; // Unser Flag für blockierendes Warten
        }
    }
}

static int StepAsyncTimer(void* pPWM, int dir, unsigned int numPulses, void (*doneClb)(L6474_Handle_t), L6474_Handle_t h) {
    // Das ist der 'stepAsync'-Hook, den die L6474-Lib aufruft,
    // wenn `L6474_StepIncremental` genutzt wird.
    (void)pPWM;
    (void)h;

    stepper_ctx.is_running = 1; // Wir legen los
    stepper_ctx.done_callback = doneClb; // Callback für "Fertig" merken

    // Richtungspin setzen
    HAL_GPIO_WritePin(STEP_DIR_GPIO_Port, STEP_DIR_Pin, !!dir);

    // Und den Puls-Zähler (TIM1) starten
    start_tim1(numPulses);

    return 0;
}



static int StepTimerCancelAsync(void* pPWM)
{
    // Das ist der 'cancelStep'-Hook.
    // Wird von der Lib ODER von uns ("cancel" Befehl) aufgerufen.
    (void)pPWM;

    if (stepper_ctx.is_running) {
        HAL_TIM_OnePulse_Stop_IT(stepper_ctx.htim1_handle, TIM_CHANNEL_1); // Timer sofort stoppen
        stepper_ctx.done_callback(stepper_ctx.h); // Der Lib "fertig" melden
        stepper_ctx.is_running = 0; // Flag zurücksetzen
    }

    return 0;
}

void Initialize_Stepper(ConsoleHandle_t console_handle, SPI_HandleTypeDef* hspi1, TIM_HandleTypeDef* tim1_handle, TIM_HandleTypeDef* tim4_handle){
    // Die große Initialisierungsfunktion. Wird einmal beim Start aufgerufen.

    HAL_GPIO_WritePin(STEP_SPI_CS_GPIO_Port, STEP_SPI_CS_Pin, 1); // CS Pin High (default)
    HAL_TIM_PWM_Start(tim4_handle, TIM_CHANNEL_4); // PWM-Generator (TIM4) starten

    // --- Hooks zuweisen ---
    // Der Lib sagen, welche unserer Funktionen sie nutzen soll.
    p.malloc     = StepLibraryMalloc;
    p.free       = StepLibraryFree;
    p.transfer   = StepDriverSpiTransfer; // Unsere SPI-Funktion
    p.reset      = StepDriverReset;
    p.sleep      = StepLibraryDelay;
    p.stepAsync  = StepAsyncTimer;     // Unser TIM1-Starter
    p.cancelStep = StepTimerCancelAsync; // Unser TIM1-Stopper

    // --- Instanz erstellen ---
    // Die Lib-Instanz mit unseren Hooks und Hardware-Handles (SPI, Timer) erstellen
    stepper_ctx.h = L6474_CreateInstance(&p, hspi1, NULL, tim1_handle);
    stepper_ctx.htim1_handle = tim1_handle; // TIM1 (Zähler) merken
    stepper_ctx.htim4_handle = tim4_handle; // TIM4 (Takt) merken

    // --- Defaults setzen ---
    // Die mechanischen Parameter in unseren Context laden
    stepper_ctx.steps_per_turn = STEPS_PER_TURN;
    stepper_ctx.resolution = RESOLUTION;
    stepper_ctx.mm_per_turn = MM_PER_TURN;

    // Software-Endlagen (Defaults)
    stepper_ctx.position_min_steps = 0;
    stepper_ctx.position_max_steps = 100000;
    stepper_ctx.position_ref_steps = 0;

    // --- Konsole registrieren ---
    // Den "stepper" Befehl bei der Konsole anmelden und unser 'stepper_ctx'
    // als Kontext-Pointer mitgeben.
    CONSOLE_RegisterCommand(console_handle, "stepper", "Stepper main Command", stepperConsoleFunction, &stepper_ctx);
}
