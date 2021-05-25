#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "stm32l476xx.h"
#include "SysClock.h"
#include "LED.h"
#include "UART.h"

char buffer[BufferSize];
int results[101];
int LOWER_LIMIT = 950;
int UPPER_LIMIT = 1050;

// Enable GPIO and timer processes
void gpio_and_timer_init(void) {
	// enable the peripheral clock of GPIO Port
	RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;

	// Connect the TIM2 timer to the GPIO Pin A0
	GPIOA->MODER &= ~0xFF;  // Clear register
	GPIOA->MODER |= 0x2;    // Alternate function mode
	GPIOA->AFR[0] |= 0x1;   // Set PA0 to the timer

    // Enable clock
    RCC->APB1ENR1 |= RCC_APB1ENR1_TIM2EN;

	// Configure timer
    TIM2->PSC = 80;              // Load prescale
    TIM2->EGR |= TIM_EGR_UG;
    TIM2->CCER &= ~(0xFFFFFFFF); // Clear register
    TIM2->CCMR1 |= 0x1;          // Set to input mode
	TIM2->CCER |= 0x1;           // Enable capture
}


// Function starts capturing to check for rising edge signals and checks for signal
// If timeout occurs, return fail
int post(){
	int pulse_found = 0;
	uint32_t first_val = 0;
	uint32_t current_val = 0;
	TIM2->CR1 |= 0x1;
	first_val = (uint32_t)TIM2->CCR1;
	while(!pulse_found){
		if(TIM2->SR & 0x2){
			current_val = (uint32_t)TIM2->CCR1;
			if(current_val - first_val <= 100000){
				TIM2->CR1 &= 0x0;
				return 1;
			}
			else {
				return 0;
			}
		}
		else {
			current_val = (uint32_t)TIM2->CNT;
			if(current_val - first_val > 100000){
			  return 0;
			}
		}
	}
	return 0;
}

// Waits for measurements, records when found
void measure(){
	int count = 0;
	int first_val = 1;
	uint32_t prev_val = 0;
	uint32_t current_measurement = 0;
	uint32_t diff = 0;

	USART_Write(USART2, (uint8_t *)"Taking measurements...\r\n", 25);
	TIM2->CR1 |= 0x1;
	while(count <= 1000){
		if(TIM2->SR & 0x2){
		    if (first_val) {
			    prev_val = (uint32_t)TIM2->CCR1;
			    first_val = 0;
		    }
		    else {
			    current_measurement = (uint32_t)TIM2->CCR1;
			    diff = current_measurement - prev_val;
			    prev_val = current_measurement;
				if (diff <= UPPER_LIMIT && diff >= LOWER_LIMIT) {
					results[diff-LOWER_LIMIT]++;
				}
			count++;
		    }
		}
	}
	TIM2->CR1 &= 0x0;
}

int main(void){

	System_Clock_Init();
	LED_Init();
	UART2_Init();
	gpio_and_timer_init();

	uint8_t rxByte;
	int buff_size = 0;
	int running = 1;

	// Post
	USART_Write(USART2, (uint8_t *)"Starting Post...\r\n", 18);
	while (post() == 0){
		USART_Write(USART2, (uint8_t *)"Post Failed. Try again? Press Y or N\r\n", 39);
		char rxByte = USART_Read(USART2);
		if (rxByte == 'N' || rxByte == 'n'){
			USART_Write(USART2, (uint8_t *)"Exiting program...\r\n", 18);
			return 0;
		}
	}

	// Main loop
	while(running){
		char current_limit_ask_msg[55];
		while(1){
			sprintf(current_limit_ask_msg, "Current limits are %d to %d. Change lower limit? Y or N\r\n", LOWER_LIMIT, UPPER_LIMIT);
			USART_Write(USART2, (uint8_t *)current_limit_ask_msg, strlen(current_limit_ask_msg));
			rxByte = USART_Read(USART2);
			if (rxByte == 'N' || rxByte == 'n'){
				break;
			}
			else if (rxByte == 'Y' || rxByte == 'y'){
				USART_Write(USART2, (uint8_t *)"Enter the lower limit: \r\n", 30);
				memset(buffer, (void *)0, BufferSize);
				buff_size = 0;
				while(1){
					rxByte = USART_Read(USART2);
					if(rxByte == '\r'){ //Enter is pressed
						USART_Write(USART2, (uint8_t *)"\r\n", 2);
						break;
					}
					else{
						buffer[buff_size] = (char*)*&rxByte;
						buff_size++;
						USART_Write(USART2, &rxByte, sizeof(rxByte)); //Write out user input back into the terminal
					}
				}
				LOWER_LIMIT = atoi(buffer);
				UPPER_LIMIT = LOWER_LIMIT + 100;
			}
		}

		// Wait for user to press enter
		while(1){
			USART_Write(USART2, (uint8_t *)"Press enter to start...\r\n", 25);
			rxByte = USART_Read(USART2);
			if(rxByte == '\r'){
				break;
			}
		}

		measure();

		// Find results
		char result_msg[20];
		int idx = LOWER_LIMIT;
		for(int x = 0; x < 101; x++){
			idx++;
			if(results[x] != 0){
				sprintf(result_msg, "%d   %d\r\n", idx, results[x]);
				USART_Write(USART2, (uint8_t *)result_msg, strlen(result_msg));
			}
		}

		// Ask to restart
		while(1){
			USART_Write(USART2, (uint8_t *)"Start again? Y or N\r\n", 24);
			rxByte = USART_Read(USART2);
			if (rxByte == 'N' || rxByte == 'n'){
				running = 0;
				break;
			}
			else if (rxByte == 'Y' || rxByte == 'y'){
				break;
			}
		}
	}

	USART_Write(USART2, (uint8_t *)"Exiting program...", 18);
	return 1;
}
