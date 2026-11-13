/*
 * hcsr04_sensor.h
 *
 *  Created on: Nov 6, 2026
 *      Author: prana
 */

#ifndef INC_HCSR04_SENSOR_H_
#define INC_HCSR04_SENSOR_H_

#include <stdint.h>
#include "stm32f4xx_hal.h"      /* TIM_HandleTypeDef; leaner than main.h */

typedef enum {
	HCSR04_OK = 0, HCSR04_ERROR
} Hcsr04Status;

Hcsr04Status hcsr04_init(TIM_HandleTypeDef *htim);
uint32_t hcsr04_measure_cm(void); /* returns 0 = no valid echo */

#endif /* INC_HCSR04_SENSOR_H_ */
