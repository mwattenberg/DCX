/*
 * DCX_ISOP_main.c
 *
 *  Created on: 08.04.2025
 *      Author: Sachira Liyanage
 */

#include "cybsp.h"
#include "DCX.h"

static void app_init(void)
{
    // Initialize all DCX phases and infrastructure
    DCX_init();

    // If you want a different switching frequency than 50 kHz:
    // DCX_setFsw(800000);   // <- example, like in your ISOP test

    // Optional: set deadtimes (if you want fixed values instead of soft-start logic)
    // DCX_setDeadtimePrimary(4);
    // DCX_setDeadtimeSecondary(30);

    // Optional: set duty for a specific phase (e.g. Phase_A_TOP) if not 50%:
    // DCX_SetDutyCycle(&Phase_A_TOP, 0.5f);

    // Bring up primary side (connect outputs + enable via STOP_PRIM)
    DCX_softStartPrim();

    // If you also want to turn on secondary side now:
    // DCX_softStartSec();
}

int main(void)
{
    cybsp_init();

    // Enable global interrupts
    __enable_irq();

    // Initialize and start DCX
    app_init();

    // Main application loop
    for (;;)
    {
        // later: control loops, change duty, change Fsw, etc.
        // e.g. DCX_SetDutyCycle(&Phase_A_TOP, some_new_duty);
    }
}


