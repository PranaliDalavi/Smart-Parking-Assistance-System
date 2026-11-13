/*
 * leds.h
 *
 *  Created on: Nov 6, 2026
 *      Author: prana
 */

#ifndef INC_LEDS_H_
#define INC_LEDS_H_

#include "park_logic.h"

void update_leds(ParkState park_state);
void leds_safe_state(void);   /* usable from fault handlers; no HAL */

#endif /* INC_LEDS_H_ */
