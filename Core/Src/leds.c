/*
 * leds.c
 *
 *  Created on: Nov 6, 2026
 *      Author: prana
 */
#include "main.h"
#include "leds.h"
#include <stdbool.h>

#define BLINK_WARNING_MS         (400U)
#define BLINK_DANGER_MS          (200U)
#define BLINK_CRITICAL_MS        (100U)

#define LED_ALL_PINS   (GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15)

static uint32_t blink_interval_ms(ParkState park_state) {
	uint32_t interval;

	switch (park_state) {
	case STATE_WARNING:
		interval = BLINK_WARNING_MS;
		break;
	case STATE_DANGER:
		interval = BLINK_DANGER_MS;
		break;
	case STATE_CRITICAL:
		interval = BLINK_CRITICAL_MS;
		break;
	default:
		interval = 0U;
		break;
	}
	return interval;
}

void update_leds(ParkState park_state) {
	static uint32_t last_toggle_ms = 0U;
	static bool blink_on = true;
	uint32_t now = HAL_GetTick();
	uint32_t interval = blink_interval_ms(park_state);

	if (interval == 0U) {
		blink_on = true;
	} else if ((now - last_toggle_ms) >= interval) {
		blink_on = !blink_on;
		last_toggle_ms = now;
	} else {
		/* not yet time to toggle */
	}

	HAL_GPIO_WritePin(GPIOD, LED_ALL_PINS, GPIO_PIN_RESET);

	if (blink_on) {
		switch (park_state) {
		case STATE_SAFE:
			HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12, GPIO_PIN_SET);
			break;
		case STATE_WARNING:
			HAL_GPIO_WritePin(GPIOD, GPIO_PIN_13, GPIO_PIN_SET);
			break;
		case STATE_DANGER:
			HAL_GPIO_WritePin(GPIOD, GPIO_PIN_14, GPIO_PIN_SET);
			break;
		case STATE_CRITICAL:
			HAL_GPIO_WritePin(GPIOD, GPIO_PIN_15, GPIO_PIN_SET);
			break;
		case STATE_SENSOR_ERROR:
		default: /* unknown state is treated as a fault (fail-safe) */
			HAL_GPIO_WritePin(GPIOD, LED_ALL_PINS, GPIO_PIN_SET);
			break;
		}
	}
}

void leds_safe_state(void) {
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIODEN; /* the clock may still be off */
	(void) RCC->AHB1ENR; /* read back: short delay after enabling the clock */
	GPIOD->MODER = (GPIOD->MODER & ~(0xFFUL << 24U)) | (0x55UL << 24U); /* PD12–PD15 = outputs (01 each) */
	GPIOD->BSRR = LED_ALL_PINS; /* all four on */
}

