/*
 * DCX.h
 *
 *  Created on: 26.08.2025
 *      Author: wattenberg
 */

#ifndef DCX_H_
#define DCX_H_

#include "cybsp.h"


typedef struct {
    TCPWM_Type* hw;
    uint32_t num;
    cy_stc_tcpwm_pwm_config_t* config;
    uint32_t deadtime;
} DCX_pwm_channel_t;

typedef struct DCX_phase
{
    float duty;
    uint32_t fsw;
    uint32_t period;
    uint32_t compare;
    bool isRunning;
    int32_t phaseShift_PrimaryToSecondary;	//in ticks
	float phaseShift_PhaseToPhase;			//in degree
    // PWM channels for primary and secondary side
    DCX_pwm_channel_t PWM_PRI;
    DCX_pwm_channel_t PWM_SEC;
    
} DCX_phase_t;


#define DCX_NUMBER_OF_PHASES 4

extern DCX_phase_t DCX_phase[DCX_NUMBER_OF_PHASES];

#define TIMER_SOFT_START_IRQ tcpwm_0_interrupts_2_IRQn



//extern DCX_phase_t Phase_A_TOP;
//extern DCX_phase_t Phase_A_BOT;
//extern DCX_phase_t Phase_B_TOP;
//extern DCX_phase_t Phase_B_BOT;

//extern DCX_phase_t* DCX_allPhases[DCX_NUMBER_OF_PHASES];

void DCX_init();
void DCX_TriggerStart();
void DCX_startPrim();
void DCX_startSec();
void DCX_stopAll();
void DCX_startAll();
void DCX_setFsw(uint32_t fsw);
void DCX_UpdatePhaseShift(DCX_phase_t* phase);
void DCX_setDeadtimePrimary(uint32_t counts);
void DCX_setDeadtimeSecondary(uint32_t counts);
void DCX_setDeadtimeHRSecondary(uint32_t counts);
void DCX_stopSec();
void DCX_softStartPrim();
void DCX_softStopPrim();
void DCX_SetDutyCycle(DCX_phase_t* phase, float duty);
void DCX_softStartSec();

#endif /* DCX_H_ */

