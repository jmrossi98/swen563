/*
  Function definitions for the GPIO functions
*/

#include "GPIO.h"

/*
  Function to set the necessary bits in all of the GPIO registers to
  enable the PA0 pin, PA1 pin, the GPIO clock, and tie the PAO and PA1 
	pin to the TIM2 timer
*/
void gpio_init(){

	// Enable the GPIO clock
	GPIO_CLOCK |= GPIO_CLOCK_ENABLE;

	// Initialize PA0 and PA1 as output, tie them to TIM2
	GPIO_A_PINS &= CLEAR;
	GPIO_A_PINS |= GPIO_ALTERNATE_FUNCTION_MODE;
	GPIO_PA0_ALTERNATE_FUNCTION |= GPIO_A_PA0_PA1_ALETERNATE_FUNCTION_ENABLE;

}
