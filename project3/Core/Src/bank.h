#ifndef __BANK_MAN_H
#define __BANK_MAN_H
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "string.h"
#include "semphr.h"
#include "queue.h"
#include "task.h"
#include "cmsis_os.h"
#include "usart.h"
#include "rng.h"
#include <stdbool.h>

#define NUM_TELLERS 3
#define CONVERSION_FACTOR 100

struct Customer_t {
	int id;
	uint32_t time_entered_queue;
	int time_left_queue;
	int time_left_teller;
	int transaction_time;
};

enum status{
	IDLE,
	BUSY,
	BREAK
};

struct teller{
	enum status teller_status;
	int num_customers;
	int num_breaks;
	int total_wait_time;
	int total_transaction_time;
	int total_break_time;
};

struct Metrics_t{
	int customers_served;
	int customers_served_per_teller[NUM_TELLERS];
	double avg_customer_waiting_time;
	double avg_teller_time;
	double avg_teller_waiting_time;
	uint32_t max_customer_wait_time;
	uint32_t max_teller_wait_time;
	uint32_t max_transaction_time;
	int max_queue_depth;
	int total_num_breaks[NUM_TELLERS];
	double avg_break_time[NUM_TELLERS];
	uint32_t max_break_time[NUM_TELLERS];
	uint32_t min_break_time[NUM_TELLERS];
	uint32_t total_break_time[NUM_TELLERS];
	uint32_t total_customer_queue_time;
	uint32_t total_customer_teller_time;
	uint32_t total_teller_wait_time;
};

struct Bank_t {
	int is_open;
	QueueHandle_t customers;
	TaskHandle_t customer_entering_queue;
	TaskHandle_t teller_thread;
	struct teller tellers[NUM_TELLERS];
};


void print_line(char* text);
void print_metrics(void);
void int_format_print(char* text, int num);
void float_format_print(char* text, float num);
void set_current_time(int new_time);
int get_current_time(void);
struct teller teller_init(void);
void bank_init(void);
struct Metrics_t metric_init(void);
void bank_thread(void* argument);
void thread_init(void);
void teller_thread(void* argument);
int get_random(int max);
char* teller_status_to_string(enum status teller_status);

#endif
