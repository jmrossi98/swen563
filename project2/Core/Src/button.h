/*
 * button.h
 *
 *  Created on: Jan 18, 2021
 */

#ifndef INC_BUTTON_H_
#define INC_BUTTON_H_

#include "stm32l476xx.h"

//   Right = PA2        Up   = PA3         Center = PA0
//   Left  = PA1        Down = PA5
enum BTN {
	BTN_CTR		= GPIO_IDR_ID0,
	BTN_LEFT	= GPIO_IDR_ID1,
	BTN_RIGHT	= GPIO_IDR_ID2,
	BTN_UP		= GPIO_IDR_ID3,
	BTN_DOWN	= GPIO_IDR_ID5,
	BTN_ALL		= BTN_CTR | BTN_LEFT | BTN_RIGHT | BTN_UP | BTN_DOWN
};

uint32_t read_buttons(void);

#endif /* INC_BUTTON_H_ */
