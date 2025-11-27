/*
 * DCX.c
 *
 *  Created on: 26.08.2025
 *      Author: wattenberg
 */
 
// Includes
#include "cy_syslib.h"
#include "cy_tcpwm.h"
#include "cy_trigmux.h"
#include "cybsp.h"
#include "cy_tcpwm_pwm.h"
#include <math.h>
#include "DCX.h"


#define DCX_TRIGGER_START_INDEX   6
#define DCX_TRIGGER_RELOAD_INDEX  7


#define DCX_SWTRIG_RELOAD   TRIG_OUT_MUX_10_TCPWM0_TR_IN6
#define DCX_SWTRIG_START    TRIG_OUT_MUX_10_TCPWM0_TR_IN7


#define DCX_MaxDeadtime (Phase_A_TOP.period / 2 + 2)

//DCX_phase_t Phase_A_TOP;
//DCX_phase_t Phase_A_BOT;
//DCX_phase_t Phase_B_TOP;
//DCX_phase_t Phase_B_BOT;

DCX_phase_t DCX_phase[DCX_NUMBER_OF_PHASES];

//Non "public" functions
void DCX_disconntectOutputs(DCX_phase_t* phase);
void DCX_connectOutputs(DCX_phase_t* phase);

uint16_t counter = 0;

//Local helper function
static inline void DCX_PWM_init(DCX_phase_t* phase)
{
	Cy_TCPWM_PWM_Enable(phase->PWM_PRI.hw, phase->PWM_PRI.num);
	Cy_TCPWM_PWM_Init(phase->PWM_PRI.hw, phase->PWM_PRI.num, phase->PWM_PRI.config);
			
	//Enable the counter
	Cy_TCPWM_PWM_Enable(phase->PWM_SEC.hw, phase->PWM_SEC.num); //Maybe we should not do this for the sec. side right now but for testing it's okay
	Cy_TCPWM_PWM_Init(phase->PWM_SEC.hw, phase->PWM_SEC.num, phase->PWM_SEC.config);
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

void ISR_SoftStartPrim()
{
	counter++;
	if(counter == 10000)
	{
		if(Phase_A_TOP.PWM_PRI.deadtime >3)
		{
			Phase_A_TOP.PWM_PRI.deadtime--;
			DCX_setDeadtimePrimary(Phase_A_TOP.PWM_PRI.deadtime);
		}
		else
		{
			NVIC_DisableIRQ(TIMER_SOFT_START_IRQ);
		}
		counter = 0;		
	}


	/* Get all the enabled pending interrupts */
    uint32_t interrupts = Cy_TCPWM_GetInterruptStatusMasked(TIMER_SOFT_START_HW, TIMER_SOFT_START_NUM);
    Cy_TCPWM_ClearInterrupt(TIMER_SOFT_START_HW, TIMER_SOFT_START_NUM, interrupts);
	NVIC_ClearPendingIRQ(TIMER_SOFT_START_IRQ);
}

void ISR_SoftStopPrim()
{
	
	if(Phase_A_TOP.PWM_PRI.deadtime < DCX_MaxDeadtime)
	{
		Phase_A_TOP.PWM_PRI.deadtime++;
		DCX_setDeadtimePrimary(Phase_A_TOP.PWM_PRI.deadtime);
	}
	else
	{
		NVIC_DisableIRQ(TIMER_SOFT_START_IRQ);
	}

	/* Get all the enabled pending interrupts */
    uint32_t interrupts = Cy_TCPWM_GetInterruptStatusMasked(TIMER_SOFT_START_HW, TIMER_SOFT_START_NUM);
    Cy_TCPWM_ClearInterrupt(TIMER_SOFT_START_HW, TIMER_SOFT_START_NUM, interrupts);
	NVIC_ClearPendingIRQ(TIMER_SOFT_START_IRQ);
}

void ISR_SoftStartSec()
{
	
	if((Phase_A_TOP.PWM_SEC.deadtime>>6) > (12))
	{
		Phase_A_TOP.PWM_SEC.deadtime = Phase_A_TOP.PWM_SEC.deadtime - 64;
		DCX_setDeadtimeHRSecondary(Phase_A_TOP.PWM_SEC.deadtime);
	}
	else
	{
		NVIC_DisableIRQ(TIMER_SOFT_START_IRQ);
	}

//	DCX_setDeadtimeSecondary(10);
//	NVIC_DisableIRQ(TIMER_SOFT_START_IRQ);

	/* Get all the enabled pending interrupts */
    uint32_t interrupts = Cy_TCPWM_GetInterruptStatusMasked(TIMER_SOFT_START_HW, TIMER_SOFT_START_NUM);
    Cy_TCPWM_ClearInterrupt(TIMER_SOFT_START_HW, TIMER_SOFT_START_NUM, interrupts);
	NVIC_ClearPendingIRQ(TIMER_SOFT_START_IRQ);
}


void DCX_init(void)
{
	// Assign hardware and counter numbers to phase structs
	
	DCX_phase[0].PWM_PRI.hw = PWM_PRI_A_TOP_HW;
	DCX_phase[0].PWM_PRI.num = PWM_PRI_A_TOP_NUM;
	DCX_phase[0].PWM_PRI.config = &PWM_PRI_A_TOP_config;
	DCX_phase[0].phaseShift_PhaseToPhase = 0.0f;
	DCX_phase[0].phaseShift_PrimaryToSecondary = 5; 
	
	//...
	DCX_phase[1].phaseShift_PhaseToPhase = -90.0f;
	DCX_phase[1].phaseShift_PrimaryToSecondary = 5;
	
//	Phase_A_TOP.PWM_PRI.hw = PWM_PRI_A_TOP_HW;
//	Phase_A_TOP.PWM_PRI.num = PWM_PRI_A_TOP_NUM;
//	Phase_A_TOP.PWM_PRI.config = &PWM_PRI_A_TOP_config;
//	Phase_A_TOP.PWM_PRI.phaseShift = 0.0f;

	Phase_A_TOP.PWM_SEC.hw = PWM_SEC_A_TOP_HW;
	Phase_A_TOP.PWM_SEC.num = PWM_SEC_A_TOP_NUM;
	Phase_A_TOP.PWM_SEC.config = &PWM_SEC_A_TOP_config;
    Phase_A_TOP.PWM_SEC.phaseShift = Phase_A_TOP.PWM_PRI.phaseShift + 6.0f;


    Phase_A_BOT.PWM_PRI.hw = PWM_PRI_A_BOT_HW;
    Phase_A_BOT.PWM_PRI.num = PWM_PRI_A_BOT_NUM;
    Phase_A_BOT.PWM_PRI.config = &PWM_PRI_A_BOT_config;
    Phase_A_BOT.PWM_PRI.phaseShift = 90.0f;

    Phase_A_BOT.PWM_SEC.hw = PWM_SEC_A_BOT_HW;
    Phase_A_BOT.PWM_SEC.num = PWM_SEC_A_BOT_NUM;
    Phase_A_BOT.PWM_SEC.config = &PWM_SEC_A_BOT_config;
    Phase_A_BOT.PWM_SEC.phaseShift = Phase_A_BOT.PWM_PRI.phaseShift + 6.0f;


    Phase_B_TOP.PWM_PRI.hw = PWM_PRI_B_TOP_HW;
    Phase_B_TOP.PWM_PRI.num = PWM_PRI_B_TOP_NUM;
    Phase_B_TOP.PWM_PRI.config = &PWM_PRI_B_TOP_config;
    Phase_B_TOP.PWM_PRI.phaseShift = 0.0f;

    Phase_B_TOP.PWM_SEC.hw = PWM_SEC_B_TOP_HW;
    Phase_B_TOP.PWM_SEC.num = PWM_SEC_B_TOP_NUM;
    Phase_B_TOP.PWM_SEC.config = &PWM_SEC_B_TOP_config;
    Phase_B_TOP.PWM_SEC.phaseShift = Phase_B_TOP.PWM_PRI.phaseShift + 6.0f;


    Phase_B_BOT.PWM_PRI.hw = PWM_PRI_B_BOT_HW;
    Phase_B_BOT.PWM_PRI.num = PWM_PRI_B_BOT_NUM;
    Phase_B_BOT.PWM_PRI.config = &PWM_PRI_B_BOT_config;
    Phase_B_BOT.PWM_PRI.phaseShift = 90.0f;

    Phase_B_BOT.PWM_SEC.hw = PWM_SEC_B_BOT_HW;
    Phase_B_BOT.PWM_SEC.num = PWM_SEC_B_BOT_NUM;
    Phase_B_BOT.PWM_SEC.config = &PWM_SEC_B_BOT_config;
    Phase_B_BOT.PWM_SEC.phaseShift = Phase_B_BOT.PWM_PRI.phaseShift + 6.0f;   
    
//     = &Phase_A_TOP;
//    DCX_allPhases[1] = &Phase_A_BOT;
//    DCX_allPhases[2] = &Phase_B_TOP;
//    DCX_allPhases[3] = &Phase_B_BOT;

    for (uint32_t i = 0; i < DCX_NUMBER_OF_PHASES; i++)
    {
       DCX_PWM_init(DCX_phase[i]);
       DCX_disconntectOutputs(DCX_phase[i]);
       DCX_setFsw(DCX_phase[i], 800000);
       //same for duty
       DCX_UpdatePhaseShift(DCX_phase[i]);
       
   		DCX_setDeadtime(DCX_phase[i].PWM_PRI,64*10);
		DCX_setDeadtime(DCX_phase[i].PWM_SEC,10);
    }
    



    // Set initial phase shift for each phase
/*	DCX_UpdatePhaseShift(&Phase_A_TOP);
	DCX_UpdatePhaseShift(&Phase_A_BOT);
	DCX_UpdatePhaseShift(&Phase_B_TOP);
	DCX_UpdatePhaseShift(&Phase_B_BOT);
*/
    for (uint32_t i = 0; i < DCX_NUMBER_OF_PHASES; i++) 
    {
		DCX_UpdatePhaseShift(DCX_allPhases[i]);
    }
    


	// Setup trigger mux for each phase: start, stop, reload
/*  DCX_SetupTriggerMux(&Phase_A_TOP);
    DCX_SetupTriggerMux(&Phase_A_BOT);
    DCX_SetupTriggerMux(&Phase_B_TOP);
    DCX_SetupTriggerMux(&Phase_B_BOT);
*/
    
    for (uint32_t i = 0; i < DCX_NUMBER_OF_PHASES; i++)
    {
       DCX_SetupTriggerMux(DCX_allPhases[i]);
    }
    
//	DCX_setDeadtimePrimary(DCX_MaxDeadtime);
//	DCX_setDeadtimeSecondary(DCX_MaxDeadtime);

    
    DCX_TriggerStart(); 
    
    Cy_TCPWM_PWM_Init(PWM_STOP_PRIM_HW,PWM_STOP_PRIM_NUM, &PWM_STOP_PRIM_config);
    
    Cy_TCPWM_PWM_SetPeriod0(PWM_STOP_PRIM_HW,PWM_STOP_PRIM_NUM, 100);
    Cy_TCPWM_PWM_SetCompare0Val(PWM_STOP_PRIM_HW,PWM_STOP_PRIM_NUM, 1000);
    Cy_TCPWM_PWM_Enable(PWM_STOP_PRIM_HW,PWM_STOP_PRIM_NUM);
    Cy_TCPWM_TriggerStart_Single(PWM_STOP_PRIM_HW,PWM_STOP_PRIM_NUM);

    Cy_TCPWM_PWM_Init(PWM_STOP_SEC_HW,PWM_STOP_SEC_NUM, &PWM_STOP_SEC_config);
    
    Cy_TCPWM_PWM_SetPeriod0(PWM_STOP_SEC_HW,PWM_STOP_SEC_NUM, 100);
    Cy_TCPWM_PWM_SetCompare0Val(PWM_STOP_SEC_HW,PWM_STOP_SEC_NUM, 1000);
    Cy_TCPWM_PWM_Enable(PWM_STOP_SEC_HW,PWM_STOP_SEC_NUM);
    Cy_TCPWM_TriggerStart_Single(PWM_STOP_SEC_HW,PWM_STOP_SEC_NUM);
	

    Cy_TCPWM_Counter_Init(TIMER_SOFT_START_HW, TIMER_SOFT_START_NUM, &TIMER_SOFT_START_config);
    Cy_TCPWM_Counter_Enable(TIMER_SOFT_START_HW, TIMER_SOFT_START_NUM);
    Cy_TCPWM_TriggerStart_Single(TIMER_SOFT_START_HW, TIMER_SOFT_START_NUM);
}

void DCX_disconntectOutputs(DCX_phase_t* phase)
{
    Cy_TCPWM_PWM_Configure_LineSelect(phase->PWM_PRI.hw, phase->PWM_PRI.num,CY_TCPWM_OUTPUT_CONSTANT_0,CY_TCPWM_OUTPUT_CONSTANT_0);
    Cy_TCPWM_PWM_Configure_LineSelect(phase->PWM_SEC.hw, phase->PWM_SEC.num,CY_TCPWM_OUTPUT_CONSTANT_0,CY_TCPWM_OUTPUT_CONSTANT_0);
}

void DCX_connectOutputs(DCX_phase_t* phase)
{
	Cy_TCPWM_PWM_Configure_LineSelect(phase->PWM_PRI.hw, phase->PWM_PRI.num,CY_TCPWM_OUTPUT_PWM_SIGNAL,CY_TCPWM_OUTPUT_INVERTED_PWM_SIGNAL);
    Cy_TCPWM_PWM_Configure_LineSelect(phase->PWM_SEC.hw, phase->PWM_SEC.num,CY_TCPWM_OUTPUT_PWM_SIGNAL,CY_TCPWM_OUTPUT_INVERTED_PWM_SIGNAL);
}

void DCX_setFsw(DCX_phase_t* phase, uint32_t fsw)
{
	//For PWM it's always ClkHf3
    uint32_t ClkFreq = Cy_SysClk_ClkHfGetFrequency(3);
    uint32_t period = ClkFreq / fsw;
    uint32_t compare = period / 2;
    
    // update cached values
    phase->fsw     = fsw;
    phase->period  = period;
    phase->compare = compare;
    
    if(phase->PWM_PRI.config->hrpwm_enable)
    {
		Cy_TCPWM_PWM_SetPeriod0   (phase->PWM_PRI.hw, phase->PWM_PRI.num,   64*period); //Use define instead of magic number
		Cy_TCPWM_PWM_SetCompare0Val(phase->PWM_PRI.hw, phase->PWM_PRI.num, 64*compare);
	}
	else
	{
		Cy_TCPWM_PWM_SetPeriod0   (phase->PWM_PRI.hw, phase->PWM_PRI.num,   period);
		Cy_TCPWM_PWM_SetCompare0Val(phase->PWM_PRI.hw, phase->PWM_PRI.num, compare);
	}
	
	
	if(phase->PWM_SEC.config->hrpwm_enable)
	{
		Cy_TCPWM_PWM_SetPeriod0   (phase->PWM_SEC.hw, phase->PWM_SEC.num,   64*period);
    	Cy_TCPWM_PWM_SetCompare0Val(phase->PWM_SEC.hw, phase->PWM_SEC.num, 64*compare);
	}
	else 
	{
		Cy_TCPWM_PWM_SetPeriod0   (phase->PWM_SEC.hw, phase->PWM_SEC.num,   period);
    	Cy_TCPWM_PWM_SetCompare0Val(phase->PWM_SEC.hw, phase->PWM_SEC.num, compare);
	}
}


// Set duty cycle for a given phase (0.0f to 1.0f)
void DCX_SetDutyCycle(DCX_phase_t* phase, float duty)
{
	phase->duty = duty;

	uint32_t compare = (uint32_t)(duty * phase->period);
	phase->compare = compare;
	Cy_TCPWM_PWM_SetCompare0Val(phase->PWM_PRI.hw, phase->PWM_PRI.num, compare);
	Cy_TCPWM_PWM_SetCompare0Val(phase->PWM_SEC.hw, phase->PWM_SEC.num, compare<<6);
}

//Warning: The counter needs to stopped elsewhere.
void DCX_UpdatePhaseShift(DCX_phase_t* phase)
{
    // Calculate counter value for phase shift (assuming DCX_PWM_PERIOD is 360°)
    uint32_t counterValuePri;
    uint32_t counterValueSec;
    
    //calculate positive phase shift for negative values
    float phaseToPhase = fmodf((360.0f + phase->phaseShift_PhaseToPhase), 360.0f);
    
    //calculate counter value
    counterValuePri = (uint32_t)(phase->period * shiftPrim / 360.0f);
    counterValueSec = (uint32_t)(phase->period * shiftSec / 360.0f + phase->phaseShift_PrimaryToSecondary);
    
    Cy_TCPWM_PWM_SetCounter(phase->PWM_PRI.hw, phase->PWM_PRI.num, counterValuePri);
    Cy_TCPWM_PWM_SetCounter(phase->PWM_SEC.hw, phase->PWM_SEC.num, counterValueSec);
}

void DCX_setDeadtime(DCX_pwm_channel_t* channel, uint32_t counts)
{	
	channel->deadtime = counts;
    Cy_TCPWM_PWM_PWMDeadTime(channel->hw, channel->num, counts);
    Cy_TCPWM_PWM_PWMDeadTimeN(channel->hw, channel->num, counts);   
}

void DCX_TriggerStart()
{
    Cy_TrigMux_SwTrigger(DCX_SWTRIG_START, CY_TRIGGER_TWO_CYCLES);
}



void DCX_startPrim()
{
	Cy_TCPWM_PWM_SetCompare0Val(PWM_STOP_PRIM_HW,PWM_STOP_PRIM_NUM, 0);
	
}

void DCX_startSec()
{
	Cy_TCPWM_PWM_SetCompare0Val(PWM_STOP_SEC_HW,PWM_STOP_SEC_NUM, 0);
	
}

void DCX_stopSec()
{
	Cy_TCPWM_PWM_SetCompare0Val(PWM_STOP_SEC_HW,PWM_STOP_SEC_NUM, 1000);
}

void DCX_startAll()
{
	Cy_TCPWM_PWM_SetCompare0Val(PWM_STOP_PRIM_HW,PWM_STOP_PRIM_NUM, 0);
	Cy_TCPWM_PWM_SetCompare0Val(PWM_STOP_SEC_HW,PWM_STOP_SEC_NUM, 0);
}

void DCX_stopAll()
{
	Cy_TCPWM_PWM_SetCompare0Val(PWM_STOP_PRIM_HW,PWM_STOP_PRIM_NUM, 1000);
	Cy_TCPWM_PWM_SetCompare0Val(PWM_STOP_SEC_HW,PWM_STOP_SEC_NUM, 1000);
}



void DCX_softStartPrim()
{
//	cy_stc_sysint_t interruptus;
//	interruptus.intrSrc = TIMER_SOFT_START_IRQ; // @suppress("Field cannot be resolved")
//	interruptus.intrPriority = 1; // @suppress("Field cannot be resolved")
//	Cy_SysInt_Init(&interruptus, ISR_SoftStartPrim);
//	NVIC_ClearPendingIRQ(interruptus.intrSrc); // @suppress("Field cannot be resolved")
//	
//	DCX_setDeadtimePrimary(DCX_MaxDeadtime);
	
	DCX_setDeadtimePrimary(4);
	
/*
	DCX_connectOutputs(&Phase_A_TOP);
    DCX_connectOutputs(&Phase_A_BOT);
    DCX_connectOutputs(&Phase_B_TOP);
    DCX_connectOutputs(&Phase_B_BOT);
*/

    
   for (uint32_t i = 0; i < DCX_NUMBER_OF_PHASES; i++)
   {
    DCX_connectOutputs(DCX_allPhases[i]);
   }
   
	DCX_startPrim();
//	NVIC_EnableIRQ(TIMER_SOFT_START_IRQ);

}

void DCX_softStopPrim()
{
	cy_stc_sysint_t interruptus;
	interruptus.intrSrc = TIMER_SOFT_START_IRQ; // @suppress("Field cannot be resolved")
	interruptus.intrPriority = 1; // @suppress("Field cannot be resolved")
	Cy_SysInt_Init(&interruptus, ISR_SoftStopPrim);
	NVIC_ClearPendingIRQ(interruptus.intrSrc); // @suppress("Field cannot be resolved")
	NVIC_EnableIRQ(TIMER_SOFT_START_IRQ);
}

void DCX_softStartSec()
{
	cy_stc_sysint_t interruptus;
	interruptus.intrSrc = TIMER_SOFT_START_IRQ; // @suppress("Field cannot be resolved")
	interruptus.intrPriority = 1; // @suppress("Field cannot be resolved")
	Cy_SysInt_Init(&interruptus, ISR_SoftStartSec);
	NVIC_ClearPendingIRQ(interruptus.intrSrc); // @suppress("Field cannot be resolved")
	
	DCX_setDeadtimeSecondary(DCX_MaxDeadtime);
	DCX_startSec();
	
	NVIC_EnableIRQ(TIMER_SOFT_START_IRQ);	
}