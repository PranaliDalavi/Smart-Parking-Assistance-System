/*
 * park_logic.h
 *
 *  Created on: Nov 6, 2026
 *      Author: prana
 */

#ifndef INC_PARK_LOGIC_H_
#define INC_PARK_LOGIC_H_

#include <stdint.h>
typedef enum {
	STATE_SAFE,
	STATE_WARNING,
	STATE_DANGER,
	STATE_CRITICAL,
	STATE_SENSOR_ERROR,
	STATE_COUNT
} ParkState;

/**
 * @brief  Map a distance to a parking zone.
 * @param  dist_cm  Measured distance in cm; 0 means no valid echo.
 * @return The zone, or STATE_SENSOR_ERROR if dist_cm is 0.
 * @note   Pure function: no hardware access, safe to unit-test on a PC.
 */
ParkState park_distance_to_state(uint32_t dist_cm);
ParkState filter_state(ParkState raw_state);
const char* park_state_name(ParkState s);

#endif /* INC_PARK_LOGIC_H_ */
