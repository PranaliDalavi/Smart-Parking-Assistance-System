/*
 * status.c
 *
 *  Created on: Nov 6, 2026
 *      Author: prana
 */

#include "status.h"
#include "main.h"
#include <stdio.h>

#define PRINT_INTERVAL_MS        (500U)

void print_status(ParkState park_state, uint32_t dist_cm) {
#ifdef DEBUG
	static ParkState last_printed = STATE_SAFE;
	static uint32_t last_print_ms = 0U;
	uint32_t now = HAL_GetTick();

	if ((park_state != last_printed)
			|| ((now - last_print_ms) >= PRINT_INTERVAL_MS)) {
		(void) printf("State: %s, distance: %lu cm\r\n",
				park_state_name(park_state), (unsigned long) dist_cm);
		last_printed = park_state;
		last_print_ms = now;
	}
#else
	    (void)park_state;       /* parameters unused in release builds */
	    (void)dist_cm;
	#endif
}
