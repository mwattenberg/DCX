/*
 * DCX_ISOP_.h
 *
 *  Created on: 26.03.2025
 *      Author: Sachira Liyanage
 */

/*Header Guards */
/*
 * Preproessor directives that prevent the header file from being included multiple times in a single translation unit
 */
#ifndef DCX_ISOP_H_
#define DCX_ISOP_H_

#include "cybsp.h"

/*Enumeration definition*/
/*
 * Defines the operation state of the ISOP DCx
 */
typedef enum {DCX_ISOP_disabled, DCX_ISOP_enabled} DCX_ISOP_mode_t;

/*Structure definition*/
/*
 * Holds various configuration parameters for the ISOP DCx
 */

typedef struct DCX_ISOP_config
{
	float duty;
	float phase_shift;
	uint32_t deadtime_Prim;
	uint32_t deadtime_Sec;
	uint32_t fsw;
	uint16_t period0;	//the default period without spread-spectrum
	//uint16_t period;
	uint16_t compare;
	DCX_ISOP_mode_t mode;
	bool isRunning;
} DCX_ISOP_config_t;

/*Global variable declaration*/

extern DCX_ISOP_config_t DCX_ISOP_config;

/*Function Prototypes*/

void DCX_ISOP_init();
void DCX_ISOP_start();
void DCX_ISOP_stop();

void DCX_ISOP_setFsw(uint32_t fsw);
void DCX_setDuty(float duty);
void DCX_setPhaseShift(uint32_t phaseShiftCounts);
//void DCX_ISOP_setDutyS(float duty_s, uint32_t phaseShiftCounts);
void DCX_ISOP_setDeadtimePrimary(uint32_t deadtime_Prim);
void DCX_ISOP_setDeadtimeSecondary(uint32_t deadtime_Sec);
void DCX_enableSpreadSpectrum();


#endif /* DCX_ISOP_H_ */
