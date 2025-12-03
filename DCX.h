/*
 * DCX.h
 *
 *  Created on: 3.12.2025
 *      Author: Parham
 */

#ifndef DCX_H_
#define DCX_H_

#include "cybsp.h"
#include "cy_tcpwm_pwm.h"

typedef struct {
    TCPWM_Type* hw;
    uint32_t    num;
    cy_stc_tcpwm_pwm_config_t* config;
    uint32_t    deadtime;
} DCX_pwm_channel_t;

typedef struct
{
    float     duty;
    uint32_t  fsw;
    uint32_t  period;
    uint32_t  compare;
    bool      isRunning;

    int32_t   phaseShift_PrimaryToSecondary;   // in timer ticks
    float     phaseShift_PhaseToPhase;         // in degrees (0…360)

    // PWM channels for primary and secondary side
    DCX_pwm_channel_t PWM_PRI;
    DCX_pwm_channel_t PWM_SEC;

} DCX_phase_t;

#define DCX_NUMBER_OF_PHASES 4


#define TIMER_SOFT_START_IRQ tcpwm_0_interrupts_2_IRQn


extern DCX_phase_t DCX_phase[DCX_NUMBER_OF_PHASES];


void DCX_init(void);
void DCX_TriggerStart(void);

void DCX_startPrim(void);
void DCX_startSec(void);
void DCX_startAll(void);
void DCX_stopSec(void);
void DCX_stopAll(void);


void DCX_setFsw(DCX_phase_t* phase, uint32_t fsw);
void DCX_SetDutyCycle(DCX_phase_t* phase, float duty);
void DCX_UpdatePhaseShift(DCX_phase_t* phase);


void DCX_setDeadtime(DCX_pwm_channel_t* channel, uint32_t counts);
void DCX_setDeadtimePrimary(uint32_t counts);
void DCX_setDeadtimeSecondary(uint32_t counts);
void DCX_setDeadtimeHRSecondary(uint32_t counts);


void DCX_softStartPrim(void);
void DCX_softStopPrim(void);
void DCX_softStartSec(void);

#endif /* DCX_H_ */
