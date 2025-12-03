/*
 * DCX.c
 *
 *  Created on: 3.12.2025
 *      Author: Parham
 */

#include "cy_syslib.h"
#include "cy_tcpwm.h"
#include "cy_trigmux.h"
#include "cybsp.h"
#include "cy_tcpwm_pwm.h"
#include "cy_sysint.h"
#include <math.h>
#include "DCX.h"

#define DCX_TRIGGER_START_INDEX   6
#define DCX_TRIGGER_RELOAD_INDEX  7

#define DCX_SWTRIG_START    TRIG_OUT_MUX_10_TCPWM0_TR_IN6
#define DCX_SWTRIG_RELOAD   TRIG_OUT_MUX_10_TCPWM0_TR_IN7

#define DCX_MaxDeadtime     (DCX_phase[0].period / 2u + 2u)
#define DCX_DEGREE_FULL     (360.0f)
#define HRPWM_SCALE         (64u)
#define DCX_HR_BITSHIFT     (6u)
#define DCX_HR_STEP         (1u << DCX_HR_BITSHIFT)

DCX_phase_t DCX_phase[DCX_NUMBER_OF_PHASES];

static inline void DCX_disconntectOutputs(DCX_phase_t* phase);
static inline void DCX_connectOutputs(DCX_phase_t* phase);
static inline void DCX_PWM_init(DCX_phase_t* phase);
static inline void DCX_SetupTriggerMux(DCX_phase_t* phase);

static uint16_t softStartCounter = 0;

static inline void DCX_PWM_init(DCX_phase_t* phase)
{
    Cy_TCPWM_PWM_Init  (phase->PWM_PRI.hw, phase->PWM_PRI.num, phase->PWM_PRI.config);
    Cy_TCPWM_PWM_Enable(phase->PWM_PRI.hw, phase->PWM_PRI.num);

    Cy_TCPWM_PWM_Init  (phase->PWM_SEC.hw, phase->PWM_SEC.num, phase->PWM_SEC.config);
    Cy_TCPWM_PWM_Enable(phase->PWM_SEC.hw, phase->PWM_SEC.num);
}

static inline void DCX_SetupTriggerMux(DCX_phase_t* phase)
{
    Cy_TCPWM_InputTriggerSetup(phase->PWM_PRI.hw,
                               phase->PWM_PRI.num,
                               CY_TCPWM_INPUT_TR_START,
                               CY_TCPWM_INPUT_RISINGEDGE,
                               CY_TCPWM_INPUT_TRIG(DCX_TRIGGER_START_INDEX));

    Cy_TCPWM_InputTriggerSetup(phase->PWM_PRI.hw,
                               phase->PWM_PRI.num,
                               CY_TCPWM_INPUT_TR_RELOAD_OR_INDEX,
                               CY_TCPWM_INPUT_RISINGEDGE,
                               CY_TCPWM_INPUT_TRIG(DCX_TRIGGER_RELOAD_INDEX));

    Cy_TCPWM_InputTriggerSetup(phase->PWM_SEC.hw,
                               phase->PWM_SEC.num,
                               CY_TCPWM_INPUT_TR_START,
                               CY_TCPWM_INPUT_RISINGEDGE,
                               CY_TCPWM_INPUT_TRIG(DCX_TRIGGER_START_INDEX));

    Cy_TCPWM_InputTriggerSetup(phase->PWM_SEC.hw,
                               phase->PWM_SEC.num,
                               CY_TCPWM_INPUT_TR_RELOAD_OR_INDEX,
                               CY_TCPWM_INPUT_RISINGEDGE,
                               CY_TCPWM_INPUT_TRIG(DCX_TRIGGER_RELOAD_INDEX));
}

static inline void DCX_disconntectOutputs(DCX_phase_t* phase)
{
    Cy_TCPWM_PWM_Configure_LineSelect(phase->PWM_PRI.hw, phase->PWM_PRI.num,
                                      CY_TCPWM_OUTPUT_CONSTANT_0, CY_TCPWM_OUTPUT_CONSTANT_0);
    Cy_TCPWM_PWM_Configure_LineSelect(phase->PWM_SEC.hw, phase->PWM_SEC.num,
                                      CY_TCPWM_OUTPUT_CONSTANT_0, CY_TCPWM_OUTPUT_CONSTANT_0);
}

static inline void DCX_connectOutputs(DCX_phase_t* phase)
{
    Cy_TCPWM_PWM_Configure_LineSelect(phase->PWM_PRI.hw, phase->PWM_PRI.num,
                                      CY_TCPWM_OUTPUT_PWM_SIGNAL, CY_TCPWM_OUTPUT_INVERTED_PWM_SIGNAL);
    Cy_TCPWM_PWM_Configure_LineSelect(phase->PWM_SEC.hw, phase->PWM_SEC.num,
                                      CY_TCPWM_OUTPUT_PWM_SIGNAL, CY_TCPWM_OUTPUT_INVERTED_PWM_SIGNAL);
}

void DCX_setDeadtime(DCX_pwm_channel_t* channel, uint32_t deadtime_ticks)
{
    uint32_t dt_hw;
    channel->deadtime = deadtime_ticks;

    if (channel->config->hrpwm_enable)
    {
        dt_hw = deadtime_ticks << DCX_HR_BITSHIFT;
    }
    else
    {
        dt_hw = deadtime_ticks;
    }

    Cy_TCPWM_PWM_PWMDeadTime (channel->hw, channel->num, dt_hw);
    Cy_TCPWM_PWM_PWMDeadTimeN(channel->hw, channel->num, dt_hw);
}

void DCX_setDeadtimePrimary(uint32_t deadtime_ticks)
{
    for (uint32_t i = 0; i < DCX_NUMBER_OF_PHASES; i++)
    {
        DCX_setDeadtime(&DCX_phase[i].PWM_PRI, deadtime_ticks);
    }
}

void DCX_setDeadtimeSecondary(uint32_t deadtime_ticks)
{
    for (uint32_t i = 0; i < DCX_NUMBER_OF_PHASES; i++)
    {
        DCX_setDeadtime(&DCX_phase[i].PWM_SEC, deadtime_ticks);
    }
}

void ISR_SoftStartPrim(void)
{
    softStartCounter++;

    if (softStartCounter == 10000u)
    {
        if (DCX_phase[0].PWM_PRI.deadtime > 3u)
        {
            uint32_t new_dt = DCX_phase[0].PWM_PRI.deadtime - 1u;
            DCX_phase[0].PWM_PRI.deadtime = new_dt;
            DCX_setDeadtimePrimary(new_dt);
        }
        else
        {
            NVIC_DisableIRQ(TIMER_SOFT_START_IRQ);
        }
        softStartCounter = 0;
    }

    uint32_t interrupts = Cy_TCPWM_GetInterruptStatusMasked(TIMER_SOFT_START_HW,
                                                            TIMER_SOFT_START_NUM);
    Cy_TCPWM_ClearInterrupt(TIMER_SOFT_START_HW, TIMER_SOFT_START_NUM, interrupts);
    NVIC_ClearPendingIRQ(TIMER_SOFT_START_IRQ);
}

void ISR_SoftStopPrim(void)
{
    if (DCX_phase[0].PWM_PRI.deadtime < DCX_MaxDeadtime)
    {
        uint32_t new_dt = DCX_phase[0].PWM_PRI.deadtime + 1u;
        DCX_phase[0].PWM_PRI.deadtime = new_dt;
        DCX_setDeadtimePrimary(new_dt);
    }
    else
    {
        NVIC_DisableIRQ(TIMER_SOFT_START_IRQ);
    }

    uint32_t interrupts = Cy_TCPWM_GetInterruptStatusMasked(TIMER_SOFT_START_HW,
                                                            TIMER_SOFT_START_NUM);
    Cy_TCPWM_ClearInterrupt(TIMER_SOFT_START_HW, TIMER_SOFT_START_NUM, interrupts);
    NVIC_ClearPendingIRQ(TIMER_SOFT_START_IRQ);
}

void ISR_SoftStartSec(void)
{
    if (DCX_phase[0].PWM_SEC.deadtime > 12u)
    {
        uint32_t new_dt = DCX_phase[0].PWM_SEC.deadtime - 1u;
        DCX_phase[0].PWM_SEC.deadtime = new_dt;
        DCX_setDeadtimeSecondary(new_dt);
    }
    else
    {
        NVIC_DisableIRQ(TIMER_SOFT_START_IRQ);
    }

    uint32_t interrupts = Cy_TCPWM_GetInterruptStatusMasked(TIMER_SOFT_START_HW,
                                                            TIMER_SOFT_START_NUM);
    Cy_TCPWM_ClearInterrupt(TIMER_SOFT_START_HW, TIMER_SOFT_START_NUM, interrupts);
    NVIC_ClearPendingIRQ(TIMER_SOFT_START_IRQ);
}

void DCX_init(void)
{
    DCX_phase[0].PWM_PRI.hw     = PWM_PRI_PHASE1_HW;
    DCX_phase[0].PWM_PRI.num    = PWM_PRI_PHASE1_NUM;
    DCX_phase[0].PWM_PRI.config = &PWM_PRI_PHASE1_config;

    DCX_phase[0].PWM_SEC.hw     = PWM_SEC_PHASE1_HW;
    DCX_phase[0].PWM_SEC.num    = PWM_SEC_PHASE1_NUM;
    DCX_phase[0].PWM_SEC.config = &PWM_SEC_PHASE1_config;

    DCX_phase[0].phaseShift_PhaseToPhase       = 0.0f;
    DCX_phase[0].phaseShift_PrimaryToSecondary = 4.0f;

    DCX_phase[1].PWM_PRI.hw     = PWM_PRI_PHASE2_HW;
    DCX_phase[1].PWM_PRI.num    = PWM_PRI_PHASE2_NUM;
    DCX_phase[1].PWM_PRI.config = &PWM_PRI_PHASE2_config;

    DCX_phase[1].PWM_SEC.hw     = PWM_SEC_PHASE2_HW;
    DCX_phase[1].PWM_SEC.num    = PWM_SEC_PHASE2_NUM;
    DCX_phase[1].PWM_SEC.config = &PWM_SEC_PHASE2_config;

    DCX_phase[1].phaseShift_PhaseToPhase       = -90.0f;
    DCX_phase[1].phaseShift_PrimaryToSecondary = 4.0f;

    DCX_phase[2].PWM_PRI.hw     = PWM_PRI_PHASE3_HW;
    DCX_phase[2].PWM_PRI.num    = PWM_PRI_PHASE3_NUM;
    DCX_phase[2].PWM_PRI.config = &PWM_PRI_PHASE3_config;

    DCX_phase[2].PWM_SEC.hw     = PWM_SEC_PHASE3_HW;
    DCX_phase[2].PWM_SEC.num    = PWM_SEC_PHASE3_NUM;
    DCX_phase[2].PWM_SEC.config = &PWM_SEC_PHASE3_config;

    DCX_phase[2].phaseShift_PhaseToPhase       = -180.0f;
    DCX_phase[2].phaseShift_PrimaryToSecondary = 4.0f;

    DCX_phase[3].PWM_PRI.hw     = PWM_PRI_PHASE4_HW;
    DCX_phase[3].PWM_PRI.num    = PWM_PRI_PHASE4_NUM;
    DCX_phase[3].PWM_PRI.config = &PWM_PRI_PHASE4_config;

    DCX_phase[3].PWM_SEC.hw     = PWM_SEC_PHASE4_HW;
    DCX_phase[3].PWM_SEC.num    = PWM_SEC_PHASE4_NUM;
    DCX_phase[3].PWM_SEC.config = &PWM_SEC_PHASE4_config;

    DCX_phase[3].phaseShift_PhaseToPhase       = -270.0f;
    DCX_phase[3].phaseShift_PrimaryToSecondary = 4.0f;

    const uint32_t defaultFsw = 800000u;

    for (uint32_t i = 0; i < DCX_NUMBER_OF_PHASES; i++)
    {
        DCX_PWM_init(&DCX_phase[i]);
        DCX_disconntectOutputs(&DCX_phase[i]);

        DCX_setFsw(&DCX_phase[i], defaultFsw);
        DCX_UpdatePhaseShift(&DCX_phase[i]);

        DCX_setDeadtime(&DCX_phase[i].PWM_PRI, 12u);
        DCX_setDeadtime(&DCX_phase[i].PWM_SEC, 20u);
    }

    for (uint32_t i = 0; i < DCX_NUMBER_OF_PHASES; i++)
    {
        DCX_SetupTriggerMux(&DCX_phase[i]);
    }

    DCX_TriggerStart();

    Cy_TCPWM_PWM_Init(PWM_STOP_PRIM_HW, PWM_STOP_PRIM_NUM, &PWM_STOP_PRIM_config);
    Cy_TCPWM_PWM_SetPeriod0    (PWM_STOP_PRIM_HW, PWM_STOP_PRIM_NUM, 100u);
    Cy_TCPWM_PWM_SetCompare0Val(PWM_STOP_PRIM_HW, PWM_STOP_PRIM_NUM, 1000u);
    Cy_TCPWM_PWM_Enable        (PWM_STOP_PRIM_HW, PWM_STOP_PRIM_NUM);
    Cy_TCPWM_TriggerStart_Single(PWM_STOP_PRIM_HW, PWM_STOP_PRIM_NUM);

    Cy_TCPWM_PWM_Init(PWM_STOP_SEC_HW, PWM_STOP_SEC_NUM, &PWM_STOP_SEC_config);
    Cy_TCPWM_PWM_SetPeriod0    (PWM_STOP_SEC_HW, PWM_STOP_SEC_NUM, 100u);
    Cy_TCPWM_PWM_SetCompare0Val(PWM_STOP_SEC_HW, PWM_STOP_SEC_NUM, 1000u);
    Cy_TCPWM_PWM_Enable        (PWM_STOP_SEC_HW, PWM_STOP_SEC_NUM);
    Cy_TCPWM_TriggerStart_Single(PWM_STOP_SEC_HW, PWM_STOP_SEC_NUM);

    Cy_TCPWM_Counter_Init   (TIMER_SOFT_START_HW, TIMER_SOFT_START_NUM, &TIMER_SOFT_START_config);
    Cy_TCPWM_Counter_Enable (TIMER_SOFT_START_HW, TIMER_SOFT_START_NUM);
    Cy_TCPWM_TriggerStart_Single(TIMER_SOFT_START_HW, TIMER_SOFT_START_NUM);
}

void DCX_setFsw(DCX_phase_t* phase, uint32_t fsw)
{
    uint32_t ClkFreq = Cy_SysClk_ClkHfGetFrequency(3);
    uint32_t period = ClkFreq / fsw;
    uint32_t compare = period / 2;

    phase->fsw     = fsw;
    phase->period  = period;
    phase->compare = compare;

    if(phase->PWM_PRI.config->hrpwm_enable)
    {
        Cy_TCPWM_PWM_SetPeriod0    (phase->PWM_PRI.hw, phase->PWM_PRI.num,   HRPWM_SCALE * period);
        Cy_TCPWM_PWM_SetCompare0Val(phase->PWM_PRI.hw, phase->PWM_PRI.num,   HRPWM_SCALE * compare);
    }
    else
    {
        Cy_TCPWM_PWM_SetPeriod0    (phase->PWM_PRI.hw, phase->PWM_PRI.num,   period);
        Cy_TCPWM_PWM_SetCompare0Val(phase->PWM_PRI.hw, phase->PWM_PRI.num,   compare);
    }

    if(phase->PWM_SEC.config->hrpwm_enable)
    {
        Cy_TCPWM_PWM_SetPeriod0    (phase->PWM_SEC.hw, phase->PWM_SEC.num,   HRPWM_SCALE * period);
        Cy_TCPWM_PWM_SetCompare0Val(phase->PWM_SEC.hw, phase->PWM_SEC.num,   HRPWM_SCALE * compare);
    }
    else
    {
        Cy_TCPWM_PWM_SetPeriod0    (phase->PWM_SEC.hw, phase->PWM_SEC.num,   period);
        Cy_TCPWM_PWM_SetCompare0Val(phase->PWM_SEC.hw, phase->PWM_SEC.num,   compare);
    }
}

void DCX_UpdatePhaseShift(DCX_phase_t* phase)
{
    uint32_t counterValuePri;
    uint32_t counterValueSec;
    
    float phaseToPhase = fmodf((360.0f + phase->phaseShift_PhaseToPhase), 360.0f);
    
    counterValuePri = (uint32_t)(phase->period * phaseToPhase / 360.0f);
    counterValueSec = (uint32_t)(phase->period * phaseToPhase / 360.0f + phase->phaseShift_PrimaryToSecondary);
    
    Cy_TCPWM_PWM_SetCounter(phase->PWM_PRI.hw, phase->PWM_PRI.num, counterValuePri);
    Cy_TCPWM_PWM_SetCounter(phase->PWM_SEC.hw, phase->PWM_SEC.num, counterValueSec);
}

void DCX_SetDutyCycle(DCX_phase_t* phase, float duty)
{
    if (duty > 1.0f)      duty = 1.0f;
    else if (duty < 0.0f) duty = 0.0f;

    phase->duty = duty;

    uint32_t compare = (uint32_t)(duty * phase->period);
    phase->compare = compare;

    if (phase->PWM_PRI.config->hrpwm_enable)
    {
        Cy_TCPWM_PWM_SetCompare0Val(phase->PWM_PRI.hw,
                                    phase->PWM_PRI.num,
                                    HRPWM_SCALE * compare);
    }
    else
    {
        Cy_TCPWM_PWM_SetCompare0Val(phase->PWM_PRI.hw,
                                    phase->PWM_PRI.num,
                                    compare);
    }

    if (phase->PWM_SEC.config->hrpwm_enable)
    {
        Cy_TCPWM_PWM_SetCompare0Val(phase->PWM_SEC.hw,
                                    phase->PWM_SEC.num,
                                    HRPWM_SCALE * compare);
    }
    else
    {
        Cy_TCPWM_PWM_SetCompare0Val(phase->PWM_SEC.hw,
                                    phase->PWM_SEC.num,
                                    compare);
    }
}

void DCX_TriggerStart()
{
    Cy_TrigMux_SwTrigger(DCX_SWTRIG_START, CY_TRIGGER_TWO_CYCLES);
}

void DCX_startPrim()
{
    Cy_TCPWM_PWM_SetCompare0Val(PWM_STOP_PRIM_HW, PWM_STOP_PRIM_NUM, 0);
}

void DCX_startSec()
{
    Cy_TCPWM_PWM_SetCompare0Val(PWM_STOP_SEC_HW, PWM_STOP_SEC_NUM, 0);
}

void DCX_stopSec()
{
    Cy_TCPWM_PWM_SetCompare0Val(PWM_STOP_SEC_HW, PWM_STOP_SEC_NUM, 1000);
}

void DCX_startAll()
{
    Cy_TCPWM_PWM_SetCompare0Val(PWM_STOP_PRIM_HW, PWM_STOP_PRIM_NUM, 0);
    Cy_TCPWM_PWM_SetCompare0Val(PWM_STOP_SEC_HW,  PWM_STOP_SEC_NUM,  0);
}

void DCX_stopAll()
{
    Cy_TCPWM_PWM_SetCompare0Val(PWM_STOP_PRIM_HW, PWM_STOP_PRIM_NUM, 1000);
    Cy_TCPWM_PWM_SetCompare0Val(PWM_STOP_SEC_HW,  PWM_STOP_SEC_NUM,  1000);
}

void DCX_softStartPrim()
{
    DCX_setDeadtimePrimary(12u);
    DCX_phase[0].PWM_PRI.deadtime = 12u;

    for (uint32_t i = 0; i < DCX_NUMBER_OF_PHASES; i++)
    {
        DCX_connectOutputs(&DCX_phase[i]);
    }

    DCX_startPrim();
}

void DCX_softStopPrim()
{
    cy_stc_sysint_t interruptus;
    interruptus.intrSrc      = TIMER_SOFT_START_IRQ;
    interruptus.intrPriority = 1;
    Cy_SysInt_Init(&interruptus, ISR_SoftStopPrim);
    NVIC_ClearPendingIRQ(interruptus.intrSrc);
    NVIC_EnableIRQ(TIMER_SOFT_START_IRQ);
}

void DCX_softStartSec()
{
    cy_stc_sysint_t interruptus;
    interruptus.intrSrc      = TIMER_SOFT_START_IRQ;
    interruptus.intrPriority = 1;
    Cy_SysInt_Init(&interruptus, ISR_SoftStartSec);
    NVIC_ClearPendingIRQ(interruptus.intrSrc);

    DCX_phase[0].PWM_SEC.deadtime = DCX_MaxDeadtime;
    DCX_setDeadtimeSecondary(DCX_MaxDeadtime);

    DCX_startSec();
    NVIC_EnableIRQ(TIMER_SOFT_START_IRQ);
}
