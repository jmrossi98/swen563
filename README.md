# swen563
Projects compiled/run using STM32CubeIDE 1.5.1

All are executed on a STM32L476VGTX discovery board using Tera Term/PuTTy for UART

Baud rate at 9600 for project 1, 2, 4 and at 115200 for project 3 and 5

Project 1 requires a signal generator to provide a >1KHz square wave to TIM2 (PA0 on board)

Projects 2 and 4 require two 5V PWM servo motors, configured for channel 1 and 2 on TIM2 
(PWM control at PA0 and PA1 pins on board, 5V_U and 5V pins for power)

Project 5 only works in debug mode