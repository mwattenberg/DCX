/*
 * DCX_ISOP_main.c
 *
 *  Created on: 08.04.2025
 *      Author: Sachira Liyanage
 */


#include "cybsp.h"
#include "DCX_ISOP.h"


void init()
{
	DCX_ISOP_init(800000);
	DCX_ISOP_start();
}

int main(void)
{
	cybsp_init();

    // Enable global interrupts
    __enable_irq();

    // Initialize DCX_ISOP
    init();
    test;

    // Main application loop
    for (;;)
    {
        // Application code
    	DCX_ISOP_setDutyP(DCX_ISOP_config.duty_p);
    	DCX_ISOP_setDeadtimePrimary(DCX_ISOP_config.deadtime_Prim);
    	DCX_ISOP_setDeadtimeSecondary(DCX_ISOP_config.deadtime_Sec);
    }
}

