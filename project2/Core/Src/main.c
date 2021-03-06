#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "stm32l476xx.h"
#include "SysClock.h"
#include "LED.h"
#include "UART.h"

char* null = (void *)0;
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

void write_cmd(char *message){
  char buffer[strlen(message) + strlen("\r\n")];
  strcpy(buffer, message);
  strcat(buffer, "\r\n");
  USART_Write(USART2, (uint8_t *)"\r\n", strlen("\r\n"));
  USART_Write(USART2, (uint8_t *)buffer, strlen(buffer));
}

void usart_real_time_write(char data, int print_newline){
  char write_buffer[1] = {null};
  write_buffer[0] = data;
  USART_Write(USART2, (uint8_t *)write_buffer, sizeof(write_buffer));
  if(print_newline){
    write_cmd("");
  }
}

void usart_write_data_string(char *message, ...){
  va_list data_points;
  va_start(data_points, message);
  char buffer[2000];
  vsprintf(buffer, message, data_points);
  va_end(data_points);
  write_cmd(buffer);
}

int is_valid(char *input, char *valid_characters){
  if(!strpbrk(input, valid_characters)){
    return 0;
  }
  return 1;
}

int check_for_continuation(){
	char input = null;
	int keep_going = 0;
	write_cmd("Skip next instruction? Press y or n");
	input = USART_Read(USART2);
	usart_real_time_write(input, 1);
	while(!is_valid(&input, "YyNn")){
		write_cmd("");
		usart_write_data_string("Invalid input. Press y or n", input);
		input = USART_Read(USART2);
		usart_real_time_write(input, 1);
	}
	if(is_valid(&input, "Yy")){
		keep_going = 1;
	}
	return keep_going;
}

int stop(){
	int keep_going = 0;
	keep_going = check_for_continuation();
	if(!keep_going){
		write_cmd("Exiting the program");
		while(1);
	}
	return keep_going;
}

void delay(uint32_t delay_time) {
	delay_time = (10000 * delay_time);
	for(uint32_t index = 0; index < delay_time; index++);
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

uint16_t get_current_time(int servo_num){
	if(servo_num == 0){
		return (uint16_t)TIM3->CNT;
	}
	else {
		return (uint16_t)TIM4->CNT;
	}
}

uint16_t move_motor(int motor_num, servo_data *motor, uint16_t next_pos, int recipe){
	position last_position;
	position new_position = (position)next_pos;
	uint16_t current_time = get_current_time(motor_num);
	uint16_t delay = calculate_delay(last_position, new_position, recipe);
	if(motor_num == 0){
		TIM2->CCR1 = positions[new_position];
	}
	else {
		TIM2->CCR2 = positions[new_position];
	}
	last_position = motor->position;
	motor->position = new_position;
	motor->next_pos = (position)next_pos;
	motor->last_start = current_time;
	motor->delay = delay;
	return motor->delay;
}

uint8_t get_opcode(uint8_t byte_register){
	return byte_register & 224;
}

uint8_t get_parameter(uint8_t byte_register){
	return byte_register & 31;
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
		usart_write_data_string("Recipe %d complete on servo %d, performing reset...", motor->recipe_idx, index, index);
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

int parse_cmd(char commands[2]){
	int move_command_entered = 0;
	int recipe_start = 0;
	int already_printed_warning = 0;
	int restart = 0;
	uint16_t next_pos;
	uint16_t current_delay_time = 0;

	Green_LED_On();
	for(int index = 0; index < 2; index++){

		switch(commands[index]){
			case 'B':
			case 'b':
				reset_servo(index, &servos[index], 1);
				current_delay_time = 1000;
				servos[index].status = active;
				recipe_start = 1;
				restart = 1;
				break;
			case 'C':
			case 'c':
				servos[index].status = active;
				servos[index].recipe_status = idle;
				recipe_start = 1;
				break;
			case 'L':
			case 'l':
				next_pos = servos[index].position - 1;
				if(servos[index].position != zero_degrees) {
					current_delay_time = move_motor(index, &servos[index], next_pos, 0);
					move_command_entered = 1;
				}
				else {
					write_cmd("");
					usart_write_data_string("Cannot move motor %d further left", index);
				}
				break;
			case 'N':
			case 'n':
				break;
			case 'P':
			case 'p':
				servos[index].status = inactive;
				break;
			case 'R':
			case 'r':
				next_pos = servos[index].position + 1;
				if(servos[index].position != one_hundred_and_sixty_degrees) {
					current_delay_time = move_motor(index, &servos[index], next_pos, 0);
					move_command_entered = 1;
				}
				else {
					write_cmd("");
					usart_write_data_string("Cannot move motor %d further right", index, next_pos);
				}
				break;
			default:
				if(!already_printed_warning){
					write_cmd("");
					usart_write_data_string("Invalid command", commands);
					already_printed_warning = 1;
					recipe_start = 0;
					restart = 0;
					move_command_entered = 0;
				}
		}
	}
	Green_LED_Off();
	if(move_command_entered || restart){
		Green_LED_Off();
		Red_LED_On();
		delay(current_delay_time);
		Red_LED_Off();
		Green_LED_On();
	}
	return recipe_start;
}

void run_recipe(){
	Green_LED_On();
	Red_LED_Off();
	write_cmd("Running recipes...");
	int recipe_ended = 0;
	int keep_going = 0;
	int servos_paused = 0;
	char pause = null;

	while(1){
		if ((recipe_ended == 2) || (servos_paused == 2) || ((servos_paused == 1) && (recipe_ended == 1)) || ((servos_paused == 1) && (some_servo_inactive(servos))) || (both_servos_inactive_or_paused(servos))){
			break;
		}
		for(int servo_index = 0; servo_index < 2; servo_index++){
			if(servos[servo_index].status == active){
				pause = USART_Read_No_Block(USART2);
				if(is_valid(&pause, "Pp")){
					Green_LED_Off();
					Red_LED_Off();

					usart_real_time_write(pause, 1);
					usart_write_data_string("Pausing servo %d ...", servo_index);
					servos[servo_index].status = paused;
					servos_paused += 1;
					break;
				}
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
						Green_LED_Off();
						Red_LED_On();
						write_cmd("ERROR: Input out of bounds");
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
						Green_LED_On();
						Red_LED_On();
						write_cmd("ERROR: Input creates nested loops");
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
						write_cmd("ERROR: Input creates nested loops");
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
					Green_LED_Off();
					Red_LED_On();
					write_cmd("ERROR: Invalid recipe");
					keep_going = stop();
					if(keep_going){
						servos[servo_index].instr_idx++;
					}
					break;
				}
			}
		}
	}
	Green_LED_Off();
	reset_all_servos(0);
	if(servos_paused == 2){
		write_cmd("Recipe finished");
	}
	stop_timers();
}

int get_cmd(){
	int index = 0;
	int first = 0;
	int recipe_start = 0;
	char input = null;
	char command_buffer[2 + 1] = {null, null, null};
	write_cmd("");
	write_cmd("Enter a command:");

	input = USART_Read(USART2);
	while(input != 13){
		if(is_valid(&input, "Xx")){
			usart_real_time_write(input, 1);

			return recipe_start;
		}
		else {
			if(index >= 2){
				break;
			}
			command_buffer[index] = input;
			index++;
			usart_real_time_write(input, 0);
		}
		input = USART_Read(USART2);
	}
	recipe_start = parse_cmd(command_buffer);
	return recipe_start;
}

void additional_init(void) {

	RCC->AHB2ENR = RCC_AHB2ENR_GPIOAEN;
	GPIOA->MODER &= ~(0xFF);
	GPIOA->MODER |= 0xA;
	GPIOA->AFR[0] |= 0x11;

	RCC->APB1ENR1 |= RCC_APB1ENR1_TIM2EN;

	TIM2->PSC = 8000;
	TIM2->CCMR1 &= ~(0x00000303);
	TIM2->CCMR1 |= 0x00006868;
	TIM2->CR1 |= 0x00000080;
	TIM2->CCER |= 0x00000011;
	TIM2->ARR = 200;

	TIM2->CCR1 = positions[zero_degrees];
	TIM2->CCR2 = positions[zero_degrees];
	TIM2->EGR = TIM_EGR_UG;
	TIM2->CR1 = 0x1;

	// Enable TIM3 and TIM4
	RCC->APB1ENR1 |= RCC_APB1ENR1_TIM3EN;
	RCC->APB1ENR1 |= RCC_APB1ENR1_TIM4EN;

	// Configure TIM3
	TIM3->PSC = 8000;            // Load prescale
	TIM3->EGR |= TIM_EGR_UG;
	TIM3->CCER &= ~(0xFFFFFFFF); // Clear register
	TIM3->CCMR1 |= 0x1;          // Set to input mode
	TIM3->CCER |= 0x1;           // Enable capture

	// Configure TIM4
	TIM4->PSC = 8000;            // Load prescale
	TIM4->EGR |= TIM_EGR_UG;
	TIM4->CCER &= ~(0xFFFFFFFF); // Clear register
	TIM4->CCMR1 |= 0x1;          // Set to input mode
	TIM4->CCER|= 0x1;            // Enable capture

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
}

int main(void){

	System_Clock_Init();
	UART2_Init();
	additional_init();
	LED_Init();

	write_cmd("Enter a two letter command to control each servo motor...");
	write_cmd("Valid letters are:");
	write_cmd("    --p: Pause recipe");
	write_cmd("    --c: Continue recipe");
	write_cmd("    --r: Move servo to right");
	write_cmd("    --l: Move servo to left");
	write_cmd("    --n: Do nothing");
	write_cmd("    --b: Start recipe");

	int recipe_start = 0;
	while(1){
		recipe_start = 0;
		recipe_start = get_cmd();
		if(recipe_start){
			run_recipe();
		}
	}
}
