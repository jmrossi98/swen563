/*
 * button.c
 *
 *  Created on: Jan 18, 2021
 */


#include "button.h"

/**
 * Returns the current state of all 5 buttons
 */
uint32_t read_buttons(void) {
	return GPIOA->IDR & BTN_ALL;
}
