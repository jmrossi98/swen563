/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////

#include "string.h"
#include "stdio.h"
#include "stm32l476xx.h"
#include "usart.h"
#include "semphr.h"
#include "tim.h"
#include <stdarg.h>

#define MOV (32)
#define WAIT (64)
#define LOOP (128)
#define END_LOOP (160)
#define RECIPE_END (0)
#define ERROR (192)
#define ZERO_DEGREES (5)
#define THIRY_TWO_DEGREES (7)
#define SIXTY_FOUR_DEGREES (10)
#define NINETY_SIX_DEGREES (13)
#define ONE_HUNDRED_AND_TWENTY_EIGHT_DEGREES (16)
#define ONE_HUNDRED_AND_SIXTY_DEGREES (19)

typedef enum {
	inactive,
	active,
	paused,
} servo_status;

typedef enum {
	idle,
	running,
} recipe_status;

typedef enum {
	zero_degrees,
	thiry_two_degrees,
	sixty_four_degrees,
	ninety_six_degrees,
	one_hundred_and_twenty_eight_degrees,
	one_hundred_and_sixty_degrees,
	END_OF_POSITION_ARRAY
} position;

typedef struct{
	servo_status status;
	position position;
	uint16_t last_start;
	position next_pos;
	uint16_t delay;
	recipe_status recipe_status;
	int recipe_idx;
	int instr_idx;
	int loop_cnt;
	int is_in_loop;
	int loop_idx;
} servo_data;

typedef struct{
	uint8_t opcode;
	uint8_t parameter;
} current_instruction;

typedef struct{
	int motor_idx;
	char cmd;
} cmd_status_t;

extern int positions[END_OF_POSITION_ARRAY];

int positions[END_OF_POSITION_ARRAY] = {
	ZERO_DEGREES,
	THIRY_TWO_DEGREES,
	SIXTY_FOUR_DEGREES,
	NINETY_SIX_DEGREES,
	ONE_HUNDRED_AND_TWENTY_EIGHT_DEGREES,
	ONE_HUNDRED_AND_SIXTY_DEGREES
};

servo_data servos[2];

int recipes[4][100] = {
	{
		MOV + 0,
		MOV + 5,
		MOV + 0,
		MOV + 3,
		LOOP + 0,
		MOV + 1,
		MOV + 4,
		END_LOOP,
		MOV + 0,
		MOV + 2,
		WAIT + 0,
		MOV + 3,
		WAIT + 0,
		MOV + 2,
		MOV + 3,
		WAIT + 31,
		WAIT + 31,
		WAIT + 31,
		MOV + 4,
		RECIPE_END
	},
	{
		MOV + 0,
		MOV + 1,
		MOV + 2,
		MOV + 3,
		MOV + 4,
		MOV + 5,
		RECIPE_END
	},
	{
		MOV + 5,
		ERROR + 1,
		MOV + 1,
		RECIPE_END
	},
	{
		MOV + 2,
		LOOP + 1,
		MOV + 1,
		LOOP + 1,
		MOV + 5,
		END_LOOP,
		END_LOOP,
		RECIPE_END,
		MOV + 1,
		RECIPE_END
	}
};

xQueueHandle rxQueue;               // RX ISR places char in queue, ReceiveTask pulls them out.
uint8_t rx_char;                    // HAL will place received char here
#define CMD_BUF_SIZE 100            // this is the max length of a command line expected from the user.
static char cmd_line[CMD_BUF_SIZE]; // Received chars are buffered here until printed
int idx = 0;
int first = 0;
int recipe_start = 0;
char command_buffer[3] = {NULL, NULL, NULL};
void printBytes(char *bytes, int num);
xSemaphoreHandle txMutex;
xSemaphoreHandle recipe_mutex;
xSemaphoreHandle cmd_mutex;
SemaphoreHandle_t servo_mutex;

void printBytes(char *bytes, int num){
    xSemaphoreTake( txMutex, portMAX_DELAY );
    HAL_UART_Transmit_IT(&huart2, (uint8_t *)bytes, num);
}


void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    BaseType_t pxHigherPriorityTaskWoken;
    xSemaphoreGiveFromISR( txMutex, &pxHigherPriorityTaskWoken );
    portYIELD_FROM_ISR(pxHigherPriorityTaskWoken);
}

void TransmitTask( void *pvParameters) {
    char *buffer = (char *)pvParameters;
    printBytes(buffer, strlen(buffer));
    for(;;) {
        vTaskDelay(1000);
    }
}

void usart_write_data_string(char *message, ...){
  va_list data_points;
  va_start(data_points, message);
  char buffer[2000];
  vsprintf(buffer, message, data_points);
  va_end(data_points);
  HAL_UART_Transmit_IT(&huart2, (uint8_t *)buffer, strlen(buffer));
}

uint8_t get_opcode(uint8_t byte_register){
	return byte_register & 224;
}

uint8_t get_parameter(uint8_t byte_register){
	return byte_register & 31;
}

uint16_t get_current_time(int servo_num){
	if(servo_num == 0){
		return (uint16_t)TIM3->CNT;
	}
	else {
		return (uint16_t)TIM4->CNT;
	}
}

uint16_t calculate_delay(position last_position, position new_position, int recipe){
	uint16_t number_of_steps, delay = 0;
	number_of_steps = abs(last_position - new_position);
	if(recipe){
		delay = (uint16_t)1000 * number_of_steps;
	}
	else {
		delay = (uint16_t)200 * number_of_steps;
	}
	return delay;
}

uint16_t move_motor(int motor_num, servo_data *motor, uint16_t next_pos, int recipe){
	position last_position;
	position new_position = (position)next_pos;
	uint16_t current_time = get_current_time(motor_num);
	uint16_t delay = calculate_delay(last_position, new_position, recipe);
	if(motor_num == 0){
		__HAL_TIM_SetCompare(&htim2, TIM_CHANNEL_1, positions[new_position]);
	}
	else {
		__HAL_TIM_SetCompare(&htim2, TIM_CHANNEL_2, positions[new_position]);
	}
	HAL_Delay(1000);
	last_position = motor->position;
	motor->position = new_position;
	motor->next_pos = (position)next_pos;
	motor->last_start = current_time;
	motor->delay = delay;
}


current_instruction get_instruction(uint8_t byte_register){
	current_instruction instruction_struct;
	instruction_struct.opcode = get_opcode(byte_register);
	instruction_struct.parameter = get_parameter(byte_register);
	return instruction_struct;
}

int instruction_in_bounds(current_instruction instruction){
	return (instruction.parameter <= one_hundred_and_sixty_degrees);
}

int parse_cmd(cmd_status_t cmd_status){
	int move_command_entered = 0;
	int recipe_start = 0;
	int already_printed_warning = 0;
	int restart = 0;
	uint16_t next_pos;
	uint16_t current_delay_time = 0;\
	vTaskDelay(2);
	xSemaphoreTake(servo_mutex, 10000);
	int index = cmd_status.motor_idx;
	char cmd = cmd_status.cmd;
	if (cmd == 'b'){
			reset_servo(index, &servos[index], 1);
			current_delay_time = 1000;
			servos[index].status = active;
			recipe_start = 1;
			restart = 1;
			run_recipe();
	}
	else if (cmd == 'c'){
			servos[index].status = active;
			servos[index].recipe_status = idle;
			run_recipe();
			vTaskDelay(2);
	}
	else if (cmd == 'l'){
			next_pos = servos[index].position - 1;
			if(servos[index].position != zero_degrees) {
				current_delay_time = move_motor(index, &servos[index], next_pos, 0);
				move_command_entered = 1;
			}
			else {
				xTaskCreate( TransmitTask, "Transmit", 50, "Cannot move motor further left\r\n", 0, NULL);
			}
	}
	else if (cmd == 'p'){
		servos[index].status = inactive;
	}
	else if (cmd == 'n'){
		;
	}
	else if (cmd == 'r'){
		next_pos = servos[index].position + 1;
		if(servos[index].position != one_hundred_and_sixty_degrees) {
			current_delay_time = move_motor(index, &servos[index], next_pos, 0);
			move_command_entered = 1;
		}
		else {
			xTaskCreate( TransmitTask, "Transmit", 50, "Cannot move motor further right\r\n", 0, NULL);
		}
	}
	else{
		xTaskCreate( TransmitTask, "Transmit", 50, "Invalid command\r\n", 0, NULL);
	}
	xSemaphoreGive(servo_mutex);
}

void run_recipe(){
	xTaskCreate( TransmitTask, "Transmit", 256, "Running recipes...\r\n", 0, NULL);
	int recipe_ended = 0;
	int keep_going = 0;
	int servos_paused = 0;
	char pause = NULL;

	while(1){
		if ((recipe_ended == 2) || (servos_paused == 2) || ((servos_paused == 1) && (recipe_ended == 1)) || ((servos_paused == 1) && (some_servo_inactive(servos))) || (both_servos_inactive_or_paused(servos))){
			break;
		}
		for(int servo_index = 0; servo_index < 2; servo_index++){
			if(servos[servo_index].status == active){
				current_instruction instruction = get_instruction(recipes[servos[servo_index].recipe_idx][servos[servo_index].instr_idx]);
				switch(instruction.opcode){
				case MOV:
					if(instruction_in_bounds(instruction)){
						if(servos[servo_index].recipe_status == idle){
							start_timer(servo_index);
							move_motor(servo_index, &servos[servo_index], instruction.parameter, 1);
							servos[servo_index].recipe_status = running;
						}
						if(servos[servo_index].recipe_status == running){
							if(servo_ready(servo_index)){
								stop_timer(servo_index);
								servos[servo_index].recipe_status = idle;
								servos[servo_index].instr_idx++;
							}
						}
					}
					else{
						xTaskCreate( TransmitTask, "Transmit", 256, "ERROR: Input out of bounds\r\n", 0, NULL);
						keep_going = stop();
						if(keep_going){
							servos[servo_index].instr_idx++;
						}
					}
					break;
				case WAIT:
					if(servos[servo_index].recipe_status == idle){
						servos[servo_index].delay = (uint16_t)1000 * instruction.parameter;
						servos[servo_index].recipe_status = running;
						start_timer(servo_index);
						uint16_t current_time = get_current_time(servo_index);
						servos[servo_index].last_start = current_time;
					}
					if(servos[servo_index].recipe_status == running){
						if(servo_ready(servo_index)){
							stop_timer(servo_index);
							servos[servo_index].recipe_status = idle;
							servos[servo_index].instr_idx++;
						}
					}
					break;
				case LOOP:
					if(servos[servo_index].is_in_loop == 1){
						xTaskCreate( TransmitTask, "Transmit", 256, "ERROR: Input creates nested loops\r\n", 0, NULL);
						keep_going = stop();
						if(keep_going){
							servos[servo_index].instr_idx++;
							servos[servo_index].is_in_loop = 0;
						}
					}
					else{
						servos[servo_index].instr_idx++;
						servos[servo_index].is_in_loop = 1;
						servos[servo_index].loop_idx = servos[servo_index].instr_idx;
						servos[servo_index].loop_cnt = instruction.parameter - 1;
					}
					break;
				case END_LOOP:
					if(servos[servo_index].is_in_loop != 1){
						xTaskCreate( TransmitTask, "Transmit", 256, "ERROR: Input creates nested loops\r\n", 0, NULL);
						keep_going = stop();
						if(keep_going){
							servos[servo_index].instr_idx++;
							servos[servo_index].is_in_loop = 0;
						}
					}
					else{
						if(servos[servo_index].loop_cnt < 0){
							servos[servo_index].is_in_loop = 0;
							servos[servo_index].instr_idx++;
						}
						else {
							servos[servo_index].instr_idx = servos[servo_index].loop_idx;
							servos[servo_index].loop_cnt--;
						}
					}
					break;
				case RECIPE_END:
					recipe_ended += 1;
					reset_servo(servo_index, &servos[servo_index], 0);
					break;
				default:
					xTaskCreate( TransmitTask, "Transmit", 256, "ERROR: Invalid recipe\r\n", 0, NULL);
					keep_going = stop();
					if(keep_going){
						servos[servo_index].instr_idx++;
					}
					break;
				}
			}
		}
	}
	reset_all_servos(0);
	if(servos_paused == 2){
		xTaskCreate( TransmitTask, "Transmit", 256, "Recipe finished\r\n", 0, NULL);
	}
	stop_timers();
}

// overrides __weak version in stm32l4xx_hal_uart.c
// called when a byte has been received
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    // queue the received byte for the ReceiveTask
    BaseType_t higherPriorityTaskWoken;
    xQueueSendFromISR(rxQueue, &rx_char, &higherPriorityTaskWoken); // queue the char
    HAL_UART_Receive_IT(&huart2, &rx_char, 1);      // tell HAL to receive another char
    portYIELD_FROM_ISR(higherPriorityTaskWoken);
}

// pulls chars from rxQueue. When '\n' encountered, the line is printed
void ReceiveTask( void *pvParameters) {
    char letter;
    int cmd_idx = 0;
    HAL_UART_Receive_IT(&huart2, &rx_char, 1);      // tell HAL to start receiving chars!!!

    for(;;) {
        // keep pulling letters from rxQueue until its empty
        while(xQueuePeek( rxQueue, &letter, 0 ) != errQUEUE_EMPTY) {
            xQueueReceive( rxQueue, &letter, 1000);    // SHOULD return immediately because queue not empty

            // add letter to command line
            cmd_line[cmd_idx] = letter & 0xff;
            cmd_idx = (cmd_idx+1) % CMD_BUF_SIZE;   // safe if user types > CMD_BUF_SIZE with no \n
            printBytes((char *)&letter, 1);
            // print a full command line (ending in '\n')
            if(letter == '\r') {
                cmd_line[cmd_idx] = 0;          // nul terminate string
                printBytes(cmd_line, cmd_idx);  // this call blocks
                printBytes("\r\n", 4);
                cmd_idx = 0;
            	for(int i = 0; i < 2; i++){
        			cmd_status_t params;
            		params.cmd = cmd_line[i];
            		params.motor_idx = (int)i;
            		parse_cmd(params);
            	}
            }
            if (recipe_start) {
            	run_recipe();
            	recipe_start = 0;
            }
        }
    }
    xTaskCreate( ReceiveTask, "Receive", 256, "", 0, NULL );
}

int check_for_continuation(){
	char input = NULL;
	int keep_going = 0;
	xTaskCreate( TransmitTask, "Transmit", 256, "Skip next instruction? Press y or n\r\n", 0, NULL);
	xTaskCreate( ReceiveTask, "Receive", 256, "", 0, NULL );
}

int stop(){
	int keep_going = 0;
	keep_going = check_for_continuation();
	if(!keep_going){
		xTaskCreate( TransmitTask, "Transmit", 256, "Exiting program\r\n", 0, NULL);
		while(1);
	}
	return keep_going;
}

void delay(uint32_t delay_time) {
	delay_time = (10000 * delay_time);
	for(uint32_t index = 0; index < delay_time; index++);
}

void start_timer(int servo_num){
	if(servo_num == 0){
		TIM3->CR1 |= 0x1;
	}
	else {
		TIM4->CR1 |= 0x1;
	}
}

void start_timers(){
	for(int servo_num = 0; servo_num < 2; servo_num++){
		start_timer(servo_num);
	}
}

void stop_timer(int servo_num){
	if(servo_num == 0){
		TIM3->CR1 &= 0x0;
	}
	else {
		TIM4->CR1 &= 0x0E;
	}
}

void stop_timers(){
	for(int servo_num = 0; servo_num < 2; servo_num++){
		stop_timer(servo_num);
	}
}

uint16_t get_current_measurement(int servo_num){
	if(servo_num == 0){
		return (uint16_t)TIM3->CCR1;
	}
	else {
		return (uint16_t)TIM4->CCR1;
	}
}

void increment_recipe(servo_data *motor){
	if(motor->recipe_idx >= 3){
		motor->recipe_idx = 0;
	}
	else{
		motor->recipe_idx++;
	}
}

void reset_servo(int index, servo_data *motor, int restart){
	if(!restart){
		xTaskCreate( TransmitTask, "Transmit", 256, "Recipe complete, resetting...\r\n", 0, NULL);
		increment_recipe(motor);
		motor->status = inactive;
	}
	else {
		motor->recipe_idx = 0;
	}
	move_motor(index, motor, zero_degrees, 1);
	motor->instr_idx = 0;
	motor->loop_cnt = 0;
	motor->loop_idx = 0;
	motor->is_in_loop = 0;
	motor->next_pos = zero_degrees;
	motor->delay = 0;
	motor->recipe_status = idle;
}

void reset_all_servos(int restart){
	for(int servo_data_index; servo_data_index < 2; servo_data_index++){
		if((servos[servo_data_index].status == active) || restart){
			reset_servo(servo_data_index, &servos[servo_data_index], restart);
		}
	}
}

int servo_ready(int servo_num){
	uint16_t current_time = get_current_time(servo_num);
	uint16_t last_start = servos[servo_num].last_start;
	uint16_t delay = servos[servo_num].delay;
	if(abs(current_time - last_start) > delay){
		return 1;
	}
	else {
		return 0;
	}
}

int some_servo_inactive(servo_data *servos){
	int servo_inactive = 0;
	for(int servo_num = 0; servo_num < 2; servo_num++){
		if(servos[servo_num].status == inactive){
			servo_inactive = 1;
			break;
		}
	}
	return servo_inactive;
}

int both_servos_inactive_or_paused(servo_data *servos){
	int stop = 0;
	for(int servo_num = 0; servo_num < 2; servo_num++){
			if((servos[servo_num].status == inactive) || (servos[servo_num].status == paused)){
				stop += 1;
			}
	}
	if(stop == 2){
		return 1;
	}
	else {
		return 0;
	}
}

int is_valid(char *input, char *valid_characters){
  if(!strpbrk(input, valid_characters)){
    return 0;
  }
  return 1;
}

/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* Hook prototypes */
void configureTimerForRunTimeStats(void);
unsigned long getRunTimeCounterValue(void);

/* USER CODE BEGIN 1 */
/* Functions needed when configGENERATE_RUN_TIME_STATS is on */
__weak void configureTimerForRunTimeStats(void)
{

}

__weak unsigned long getRunTimeCounterValue(void)
{
return 0;
}
/* USER CODE END 1 */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */

  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////

    ///////////////////////////////  TRANSMIT STUFF ////////////////////////////

    // create mutex for transmission (printing)
    txMutex = xSemaphoreCreateBinary();     // protects HAL_UART in printBytes()
    xSemaphoreGive(txMutex);                // the mutex is ready. First come, first served.
    servo_mutex = xSemaphoreCreateMutex();
    cmd_mutex = xSemaphoreCreateMutex();

    xTaskCreate( TransmitTask, "Transmit", 256, "Enter a two letter command to control each servo motor...\r\n", 0, NULL);
    xTaskCreate( TransmitTask, "Transmit", 256, "Valid letters are:\r\n", 0, NULL);
    xTaskCreate( TransmitTask, "Transmit", 256, "    --p: Pause recipe\r\n", 0, NULL);
    xTaskCreate( TransmitTask, "Transmit", 256, "    --c: Continue recipe\r\n", 0, NULL);
    xTaskCreate( TransmitTask, "Transmit", 256, "    --r: Move servo to right\r\n", 0, NULL);
    xTaskCreate( TransmitTask, "Transmit", 256, "    --l: Move servo to left\r\n", 0, NULL);
    xTaskCreate( TransmitTask, "Transmit", 256, "    --n: Do nothing\r\n", 0, NULL);
    xTaskCreate( TransmitTask, "Transmit", 256, "    --b: Start recipe\r\n", 0, NULL);

    xSemaphoreTake(servo_mutex, 10000);
	for(int servo_data_index; servo_data_index < 2; servo_data_index++){
		servos[servo_data_index].status = inactive;
		servos[servo_data_index].position = zero_degrees;
		servos[servo_data_index].last_start = 0;
		servos[servo_data_index].next_pos = zero_degrees;
		servos[servo_data_index].delay = 0;
		servos[servo_data_index].recipe_status = idle;
		servos[servo_data_index].recipe_idx = 0;
		servos[servo_data_index].instr_idx = 0;
		servos[servo_data_index].loop_cnt = 0;
		servos[servo_data_index].is_in_loop = 0;
		servos[servo_data_index].loop_idx = 0;
	}
	xSemaphoreGive(servo_mutex);
//	xTaskCreate( TransmitTask, "Transmit", 256, "Enter a two letter command to control each servo motor...\r\n", 0, NULL);
//	xTaskCreate( TransmitTask, "Transmit", 35, "Valid letters are:\r\n", 0, NULL);
//	xTaskCreate( TransmitTask, "Transmit", 35, "    --p: Pause recipe\r\n", 0, NULL);
//	xTaskCreate( TransmitTask, "Transmit", 35, "    --c: Continue recipe\r\n", 0, NULL);
//	xTaskCreate( TransmitTask, "Transmit", 35, "    --r: Move servo to right\r\n", 0, NULL);
//	xTaskCreate( TransmitTask, "Transmit", 35, "    --l: Move servo to left\r\n", 0, NULL);
//	xTaskCreate( TransmitTask, "Transmit", 35, "    --n: Do nothing\r\n", 0, NULL);
//	xTaskCreate( TransmitTask, "Transmit", 35, "    --b: Start recipe\r\n", 0, NULL);
    ///////////////////////////////  RECEIVE STUFF ////////////////////////////
	int recipe_start = 0;
	rxQueue = xQueueCreate( 10, 1);
	xTaskCreate( ReceiveTask, "Receive", 256, "", 0, NULL );

    /////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////

  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
