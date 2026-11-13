/*
 * park_logic.c
 *
 *  Created on: Nov 6, 2026
 *      Author: prana
 */
#include "park_logic.h"
#include <stddef.h>

#define CONFIRM_LIMIT (3U)

/* Parking zones (desk-test values) */
#define PARK_CRITICAL_LIMIT_CM   (10U)
#define PARK_DANGER_LIMIT_CM     (20U)
#define PARK_WARNING_LIMIT_CM    (30U)

static const char *const state_names[] = { "SAFE", "WARNING", "DANGER",
		"CRITICAL", "SENSOR_ERROR" };

_Static_assert((sizeof(state_names) / sizeof(state_names[0])) == (size_t)STATE_COUNT,
		"state_names[] must have one entry per ParkState");

ParkState park_distance_to_state(uint32_t dist_cm) {
	ParkState result;

	if (dist_cm == 0U) {
		result = STATE_SENSOR_ERROR;
	} else if (dist_cm < PARK_CRITICAL_LIMIT_CM) {
		result = STATE_CRITICAL;
	} else if (dist_cm < PARK_DANGER_LIMIT_CM) {
		result = STATE_DANGER;
	} else if (dist_cm < PARK_WARNING_LIMIT_CM) {
		result = STATE_WARNING;
	} else {
		result = STATE_SAFE;
	}
	return result;
}

ParkState filter_state(ParkState raw_state) {
	static ParkState stable = STATE_SAFE;
	static ParkState candidate = STATE_SAFE;
	static uint8_t count = 0U;

	if (raw_state == stable) {
		count = 0U;
	} else if (raw_state == candidate) {
		count++;
		if (count >= CONFIRM_LIMIT) {
			stable = raw_state;
			count = 0U;
		}
	} else {
		candidate = raw_state;
		count = 1U;
	}
	return stable;
}

const char* park_state_name(ParkState s) {
	const char *name = "UNKNOWN";

	if (s < STATE_COUNT) {
		name = state_names[s];
	}
	return name;
}
