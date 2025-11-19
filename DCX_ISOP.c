/*******************************************************************************
* File Name:   DCX_ISOP.c
********************************************************************************/

/*
 * DCX_ISOP.c
 *
 *  Created on: 26.03.2025
 *      Author: Sachira Liyanage
 */

#include "cy_tcpwm.h"
#include "cy_tcpwm_pwm.h"
#include "cy_trigmux.h"
#include "cybsp.h"
#include "DCX_ISOP.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

uint32_t maxSpreadSpectrumCounts = 6;


DCX_ISOP_config_t DCX_ISOP_config;

void DCX_IsrHandler()
{
	static int32_t counter = 0;
	static int32_t counterDirection = 1;
	static uint32_t pwmCycleCount = 0;
	
	// Target: 9 kHz pattern repeat frequency
	// Pattern has 26 steps (±6 counts = 13 up + 13 down)
	// Advance pattern every ~3 PWM cycles (800kHz / (9kHz × 26 steps) ≈ 3.4)
	const uint32_t advanceEvery = 3;
	
	Cy_TCPWM_PWM_SetPeriod1(PWM_PRIM_HW, PWM_PRIM_NUM, DCX_ISOP_config.period0 + counter);
	Cy_TCPWM_PWM_SetPeriod1(PWM_SEC_HW, PWM_SEC_NUM, DCX_ISOP_config.period0 + counter);
	
	Cy_TCPWM_PWM_SetCompare0BufVal(PWM_PRIM_HW, PWM_PRIM_NUM, (DCX_ISOP_config.period0 + counter)/2);
	Cy_TCPWM_PWM_SetCompare0BufVal(PWM_SEC_HW, PWM_SEC_NUM, (DCX_ISOP_config.period0 + counter)/2);
	
	Cy_TrigMux_SwTrigger(TRIG_OUT_MUX_10_TCPWM0_TR_IN6, CY_TRIGGER_TWO_CYCLES);
	
	// Only advance the spread spectrum pattern every N PWM cycles
	pwmCycleCount++;
	if(pwmCycleCount >= advanceEvery)
	{
		pwmCycleCount = 0;
		counter = counter + counterDirection;
		if(abs(counter) == maxSpreadSpectrumCounts)
			counterDirection = counterDirection * -1;
	}
	
		/* Get all the enabled pending interrupts */
    uint32_t interrupts = Cy_TCPWM_GetInterruptStatusMasked(PWM_PRIM_HW, PWM_PRIM_NUM);
    Cy_TCPWM_ClearInterrupt(PWM_PRIM_HW, PWM_PRIM_NUM, interrupts);
	NVIC_ClearPendingIRQ(PWM_PRIM_IRQ);
}

void DCX_ISOP_init(uint32_t fsw)
{
	DCX_ISOP_config.duty = 0.5f;
	
	DCX_ISOP_config.phase_shift =6.0f;

	Cy_TCPWM_PWM_Init(PWM_PRIM_HW, PWM_PRIM_NUM, &PWM_PRIM_config);
	Cy_TCPWM_PWM_Init(PWM_SEC_HW, PWM_SEC_NUM, &PWM_SEC_config);

	//setFsw also sets duty cycle
	DCX_ISOP_setFsw(fsw);
	
	DCX_setPhaseShift(DCX_ISOP_config.phase_shift);
	
	
	//Period0 is only updated via shadow register **WHILE** the counter is running
	//Right now the counter should not be running and we have to manually write the value to period0
	Cy_TCPWM_PWM_SetPeriod0(PWM_PRIM_HW, PWM_PRIM_NUM, DCX_ISOP_config.period0);
	Cy_TCPWM_PWM_SetPeriod0(PWM_SEC_HW, PWM_SEC_NUM, DCX_ISOP_config.period0);

	DCX_ISOP_setDeadtimePrimary(16);
	DCX_ISOP_setDeadtimeSecondary(30);

	Cy_TCPWM_PWM_Enable(PWM_PRIM_HW, PWM_PRIM_NUM);
	Cy_TCPWM_PWM_Enable(PWM_SEC_HW, PWM_SEC_NUM);
	
	//setup common swapping
	Cy_TCPWM_InputTriggerSetup(PWM_PRIM_HW, PWM_PRIM_NUM, CY_TCPWM_INPUT_TR_INDEX_OR_SWAP, CY_TCPWM_INPUT_RISINGEDGE, CY_TCPWM_INPUT_TRIG(6));
	Cy_TCPWM_InputTriggerSetup(PWM_SEC_HW, PWM_SEC_NUM, CY_TCPWM_INPUT_TR_INDEX_OR_SWAP, CY_TCPWM_INPUT_RISINGEDGE, CY_TCPWM_INPUT_TRIG(6));

	//Setup common trigger start
	Cy_TCPWM_InputTriggerSetup(PWM_PRIM_HW, PWM_PRIM_NUM, CY_TCPWM_INPUT_TR_START, CY_TCPWM_INPUT_RISINGEDGE, CY_TCPWM_INPUT_TRIG(7));
	Cy_TCPWM_InputTriggerSetup(PWM_SEC_HW, PWM_SEC_NUM, CY_TCPWM_INPUT_TR_START, CY_TCPWM_INPUT_RISINGEDGE, CY_TCPWM_INPUT_TRIG(7));
	
	//setup common kill
	Cy_TCPWM_InputTriggerSetup(PWM_PRIM_HW, PWM_PRIM_NUM, CY_TCPWM_INPUT_TR_STOP_OR_KILL, CY_TCPWM_INPUT_LEVEL, CY_TCPWM_INPUT_TRIG(8));
	Cy_TCPWM_InputTriggerSetup(PWM_SEC_HW, PWM_SEC_NUM, CY_TCPWM_INPUT_TR_STOP_OR_KILL, CY_TCPWM_INPUT_LEVEL, CY_TCPWM_INPUT_TRIG(8));
}

void DCX_enableSpreadSpectrum()
{	
	cy_stc_sysint_t interruptus;
		//...then point ISR to handler...
	interruptus.intrSrc = PWM_PRIM_IRQ;
	interruptus.intrPriority = 1;
	Cy_SysInt_Init(&interruptus, DCX_IsrHandler);
	NVIC_ClearPendingIRQ(interruptus.intrSrc);
	NVIC_EnableIRQ(interruptus.intrSrc);
}

void DCX_ISOP_start()
{
	Cy_TrigMux_SwTrigger(TRIG_OUT_MUX_10_TCPWM0_TR_IN8, CY_TRIGGER_DEACTIVATE);
	Cy_TrigMux_SwTrigger(TRIG_OUT_MUX_10_TCPWM0_TR_IN7, CY_TRIGGER_TWO_CYCLES);
	DCX_ISOP_config.isRunning = true;
}

void DCX_ISOP_stop()
{
	Cy_TrigMux_SwTrigger(TRIG_OUT_MUX_10_TCPWM0_TR_IN8, CY_TRIGGER_INFINITE);
	DCX_ISOP_config.isRunning = false;
}


void DCX_ISOP_setFsw(uint32_t fsw)
{
	DCX_ISOP_config.fsw = fsw;

	uint32_t ClkFreq = Cy_SysClk_ClkHfGetFrequency(3);
	DCX_ISOP_config.period0 = ClkFreq/(fsw);

	Cy_TCPWM_PWM_SetPeriod1(PWM_PRIM_HW, PWM_PRIM_NUM, DCX_ISOP_config.period0);
	Cy_TCPWM_PWM_SetPeriod1(PWM_SEC_HW, PWM_SEC_NUM, DCX_ISOP_config.period0);

	//We have to set duty again to account for different compare value	
	DCX_setDuty(DCX_ISOP_config.duty);
}

void DCX_setDuty(float duty)
{
	if(duty > 1.0f)
		duty = 1.0f;
	else if(duty < 0.0f)
		duty = 0;
	
	DCX_ISOP_config.duty = duty;
	uint16_t compare = DCX_ISOP_config.period0 * duty;
	
	Cy_TCPWM_PWM_SetCompare0Val(PWM_PRIM_HW, PWM_PRIM_NUM, compare);
    Cy_TCPWM_PWM_SetCompare0Val(PWM_SEC_HW, PWM_SEC_NUM, compare);
	//Unsure about this...
	Cy_TCPWM_PWM_SetCompare0BufVal(PWM_PRIM_HW, PWM_PRIM_NUM, compare);
    Cy_TCPWM_PWM_SetCompare0BufVal(PWM_SEC_HW, PWM_SEC_NUM, compare);
    
	Cy_TrigMux_SwTrigger(TRIG_OUT_MUX_10_TCPWM0_TR_IN6, CY_TRIGGER_TWO_CYCLES);		
}

//Can only be called when timer is stopped
void DCX_setPhaseShift(uint32_t phaseShiftCounts)
{
	//Primary as reference
	Cy_TCPWM_PWM_SetCounter(PWM_PRIM_HW, PWM_PRIM_NUM, 0);
	//Secondary set counter offset
	Cy_TCPWM_PWM_SetCounter(PWM_SEC_HW, PWM_SEC_NUM, phaseShiftCounts);	
}

void DCX_ISOP_setDeadtimePrimary(uint32_t deadtime_Prim)
{
	//overwrite normal and buffered deadtime with same value
	//because shadow transfer is not needed for deadtime
	Cy_TCPWM_PWM_PWMDeadTime(PWM_PRIM_HW, PWM_PRIM_NUM, deadtime_Prim);
	Cy_TCPWM_PWM_PWMDeadTimeBuff(PWM_PRIM_HW, PWM_PRIM_NUM, deadtime_Prim);
	Cy_TCPWM_PWM_PWMDeadTimeN(PWM_PRIM_HW, PWM_PRIM_NUM, deadtime_Prim);
	Cy_TCPWM_PWM_PWMDeadTimeBuffN(PWM_PRIM_HW, PWM_PRIM_NUM, deadtime_Prim);

	DCX_ISOP_config.deadtime_Prim = deadtime_Prim;
}

void DCX_ISOP_setDeadtimeSecondary(uint32_t deadtime_Sec)
{
	Cy_TCPWM_PWM_PWMDeadTime(PWM_SEC_HW, PWM_SEC_NUM, deadtime_Sec);
	Cy_TCPWM_PWM_PWMDeadTimeBuff(PWM_SEC_HW, PWM_SEC_NUM, deadtime_Sec);
	Cy_TCPWM_PWM_PWMDeadTimeN(PWM_SEC_HW, PWM_SEC_NUM, deadtime_Sec);
	Cy_TCPWM_PWM_PWMDeadTimeBuffN(PWM_SEC_HW, PWM_SEC_NUM, deadtime_Sec);
	

	DCX_ISOP_config.deadtime_Sec = deadtime_Sec;
}



/* [] END OF FILE */
