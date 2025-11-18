/*******************************************************************************
* File Name:   DCX_ISOP.c
********************************************************************************/

/*
 * DCX_ISOP.c
 *
 *  Created on: 26.03.2025
 *      Author: Sachira Liyanage
 */

#include "cybsp.h"
#include "DCX_ISOP.h"
#include <stdio.h>
#include <math.h>

DCX_ISOP_config_t DCX_ISOP_config;

void DCX_ISOP_init(uint32_t fsw)
{
	DCX_ISOP_config.duty_p = 0.5f;
	DCX_ISOP_config.duty_s = 0.5f;
	DCX_ISOP_config.phase_shift =10.5f;

	Cy_TCPWM_PWM_Init(PWM_PRIM_HW, PWM_PRIM_NUM, &PWM_PRIM_config);
	Cy_TCPWM_PWM_Init(PWM_SEC_HW, PWM_SEC_NUM, &PWM_SEC_config);

	DCX_ISOP_setFsw(fsw);
	//Period0 is only updated via shadow register **WHILE** the counter is running
	//Right now the counter should not be running and we have to manually write the value to period0
	Cy_TCPWM_PWM_SetPeriod0(PWM_PRIM_HW, PWM_PRIM_NUM, DCX_ISOP_config.period);
	Cy_TCPWM_PWM_SetPeriod0(PWM_SEC_HW, PWM_SEC_NUM, DCX_ISOP_config.period);

	DCX_ISOP_setDeadtimePrimary(8);
	DCX_ISOP_setDeadtimeSecondary(26);

	Cy_TCPWM_PWM_Enable(PWM_PRIM_HW, PWM_PRIM_NUM);
	Cy_TCPWM_PWM_Enable(PWM_SEC_HW, PWM_SEC_NUM);

	Cy_TCPWM_InputTriggerSetup(PWM_PRIM_HW, PWM_PRIM_NUM, CY_TCPWM_INPUT_TR_START, CY_TCPWM_INPUT_RISINGEDGE, CY_TCPWM_INPUT_TRIG(7));
	Cy_TCPWM_InputTriggerSetup(PWM_SEC_HW, PWM_SEC_NUM, CY_TCPWM_INPUT_TR_START, CY_TCPWM_INPUT_RISINGEDGE, CY_TCPWM_INPUT_TRIG(7));

}

void DCX_ISOP_start()
{
	Cy_TrigMux_SwTrigger(TRIG_OUT_MUX_10_TCPWM0_TR_IN7, CY_TRIGGER_TWO_CYCLES);
	DCX_ISOP_config.isRunning = true;
}

void DCX_ISOP_stop()

{
	Cy_TrigMux_SwTrigger(TRIG_OUT_MUX_10_TCPWM0_TR_IN8, CY_TRIGGER_TWO_CYCLES);
	DCX_ISOP_config.isRunning = false;

}


void DCX_ISOP_setFsw(uint32_t fsw)
{
	DCX_ISOP_config.fsw = fsw;

	uint32_t ClkFreq = Cy_SysClk_ClkHfGetFrequency(3);
	printf("Clock frequency: %ld Hz\n", ClkFreq);
	DCX_ISOP_config.period = ClkFreq/(2*fsw);

	Cy_TCPWM_PWM_SetPeriod1(PWM_PRIM_HW, PWM_PRIM_NUM, DCX_ISOP_config.period);
	Cy_TCPWM_PWM_SetPeriod1(PWM_SEC_HW, PWM_SEC_NUM, DCX_ISOP_config.period);

	DCX_ISOP_setDutyP(DCX_ISOP_config.duty_p);
	DCX_ISOP_setDutyS(DCX_ISOP_config.duty_s, DCX_ISOP_config.phase_shift);


	Cy_TrigMux_SwTrigger(TRIG_OUT_MUX_10_TCPWM0_TR_IN9, CY_TRIGGER_TWO_CYCLES);
}


void DCX_ISOP_setDutyP(float duty_p)
{
	DCX_ISOP_config.duty_p = duty_p;
	uint16_t comparep = DCX_ISOP_config.period * duty_p;

	Cy_TCPWM_PWM_SetCompare0Val(PWM_PRIM_HW, PWM_PRIM_NUM, comparep);

}

void DCX_ISOP_setDutyS(float duty_s, float phase_shift)
{
    DCX_ISOP_config.duty_s = duty_s;
    uint16_t compares = DCX_ISOP_config.period * duty_s;

    Cy_TCPWM_PWM_SetCompare0Val(PWM_SEC_HW, PWM_SEC_NUM, compares);

	// Get the current counter value of the primary PWM block
    float primary_counter = Cy_TCPWM_Block_GetCounter(PWM_PRIM_HW, PWM_PRIM_NUM);

    // Set the counter value for both PWM blocks
    Cy_TCPWM_Block_SetCounter(PWM_PRIM_HW, PWM_PRIM_NUM, primary_counter);
    float secondary_counter = primary_counter + phase_shift;
    Cy_TCPWM_Block_SetCounter(PWM_SEC_HW, PWM_SEC_NUM, secondary_counter);
}


void DCX_ISOP_setDeadtimePrimary(uint32_t deadtime_Prim)
{
	Cy_TCPWM_PWM_PWMDeadTime(PWM_PRIM_HW, PWM_PRIM_NUM, deadtime_Prim);
	Cy_TCPWM_PWM_PWMDeadTimeN(PWM_PRIM_HW, PWM_PRIM_NUM, deadtime_Prim);

	DCX_ISOP_config.deadtime_Prim = deadtime_Prim;
}

void DCX_ISOP_setDeadtimeSecondary(uint32_t deadtime_Sec)
{
	Cy_TCPWM_PWM_PWMDeadTime(PWM_SEC_HW, PWM_SEC_NUM, deadtime_Sec);
	Cy_TCPWM_PWM_PWMDeadTimeN(PWM_SEC_HW, PWM_SEC_NUM, deadtime_Sec);

	DCX_ISOP_config.deadtime_Sec = deadtime_Sec;
}



/* [] END OF FILE */
