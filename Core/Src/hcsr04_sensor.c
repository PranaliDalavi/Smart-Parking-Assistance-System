/*
 * hcsr04_sensor.c
 *
 *  Created on: Nov 6, 2026
 *      Author: prana
 */
#include <stdbool.h>
#include "hcsr04_sensor.h"
#include "main.h"

/* --- HC-SR04 timing --- */
#define TRIG_PULSE_US            (11U)     /* datasheet: at least 10 us */
#define ECHO_US_PER_CM           (58U)     /* round trip at ~343 m/s    */
#define ECHO_MIN_US              (100U)    /* shorter than 2 cm: glitch */
#define ECHO_MAX_US              (40000U)  /* longer than any real pulse (~38 ms): glitch */

static TIM_HandleTypeDef *hcsr04_tim;
static uint32_t ic_rise;
static volatile uint32_t ic_width;
static volatile bool ic_ready = false;

static void delay_us(uint32_t us) {
	uint32_t start = __HAL_TIM_GET_COUNTER(hcsr04_tim);
	while ( __HAL_TIM_GET_COUNTER(hcsr04_tim) - start < us) {

	}

}

Hcsr04Status hcsr04_init(TIM_HandleTypeDef *htim) {
	Hcsr04Status status = HCSR04_ERROR;

	if (htim != NULL) {
		hcsr04_tim = htim;

		if (HAL_TIM_Base_Start(hcsr04_tim) == HAL_OK) {
			if (HAL_TIM_IC_Start_IT(hcsr04_tim, TIM_CHANNEL_3) == HAL_OK) {
				status = HCSR04_OK;
			}
		}
	}
	return status;
}

static void send_trigger(void) {
	{
		HAL_GPIO_WritePin(TRIG_GPIO_Port, TRIG_Pin, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(TRIG_GPIO_Port, TRIG_Pin, GPIO_PIN_SET);
		delay_us(TRIG_PULSE_US);
		HAL_GPIO_WritePin(TRIG_GPIO_Port, TRIG_Pin, GPIO_PIN_RESET);
	}
}

uint32_t hcsr04_measure_cm(void) {
	uint32_t dist = 0;
	uint32_t width = 0U;
	bool have_new = false;
	uint32_t primask;
	if (hcsr04_tim != NULL) {
		primask = __get_PRIMASK();
		__disable_irq();
		if (ic_ready) {
			width = ic_width;
			ic_ready = false;
			have_new = true;
		}
		__set_PRIMASK(primask);

		if (have_new && (width >= ECHO_MIN_US) && (width <= ECHO_MAX_US)) {
			dist = width / ECHO_US_PER_CM;
		}

		send_trigger();
	}
	return dist;
}

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim) {
	uint32_t value;

	if ((htim == hcsr04_tim) && (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_3)) {
		value = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_3);

		if (HAL_GPIO_ReadPin(ECHO_GPIO_Port, ECHO_Pin) == GPIO_PIN_SET) {
			ic_rise = value;
		} else {
			ic_width = value - ic_rise;
			ic_ready = true;
		}
	}
}
