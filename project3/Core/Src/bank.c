#include "bank.h"

int current_time;
int id[NUM_TELLERS];
struct Bank_t bank;
struct Metrics_t metrics;
SemaphoreHandle_t metric_mutex;
SemaphoreHandle_t HAL_mutex;
TickType_t start;

void print_line(char* text){
	xSemaphoreTake(HAL_mutex, 1000000);
	HAL_UART_Transmit(&huart2, (uint8_t *)text, strlen(text), 1000000);
	xSemaphoreGive(HAL_mutex);
}

void print_metrics(){
	char current_sim_time[60];
	char teller_status_1[60];
	char teller_status_2[60];
	char teller_status_3[60];
	int hours = current_time/6000;
	int mins = (current_time/100)%60;
	sprintf(current_sim_time, "Current Simulation Time: %d:%02d \r\n", hours+9, mins);
	sprintf(teller_status_1, "Teller 1 Status: %s, Customers Served: %d\r\n", teller_status_to_string(bank.tellers[0].teller_status), bank.tellers[0].num_customers);
	sprintf(teller_status_2, "Teller 2 Status: %s, Customers Served: %d\r\n", teller_status_to_string(bank.tellers[1].teller_status), bank.tellers[1].num_customers);
	sprintf(teller_status_3, "Teller 3 Status: %s, Customers Served: %d\r\n", teller_status_to_string(bank.tellers[2].teller_status), bank.tellers[2].num_customers);
	print_line(current_sim_time);
	print_line(teller_status_1);
	print_line(teller_status_2);
	print_line(teller_status_3);
}

void int_format_print(char* text, int num){
	char msg[strlen(text)];
	sprintf(msg, text, num);
	xSemaphoreTake(HAL_mutex, 1000000);
	HAL_UART_Transmit(&huart2, (uint8_t *)msg, strlen(msg), 1000000);
	xSemaphoreGive(HAL_mutex);
}

void float_format_print(char* text, float num){
	char msg[strlen(text)];
	sprintf(msg, text, num);
	xSemaphoreTake(HAL_mutex, 1000000);
	HAL_UART_Transmit(&huart2, (uint8_t *)msg, strlen(msg), 1000000);
	xSemaphoreGive(HAL_mutex);
}

void set_current_time(int new_time){
	current_time = new_time;
}

int get_current_time(void){
	int sim;
	sim = current_time;
	return sim;
}
struct teller teller_init(void){
	struct teller t = {IDLE, 0, 0, 0, 0, 0};
	return t;
}

void bank_init(void){
	bank.customers = xQueueCreate(50, sizeof(struct Customer_t));
	for(int i = 0; i<NUM_TELLERS; i++){
		bank.tellers[i] = teller_init();
	}
}

struct Metrics_t metric_init(void){
	struct Metrics_t metrics;
	metrics.customers_served = 0;
	for(int i = 0; i<NUM_TELLERS; i++){
		metrics.customers_served_per_teller[i] = 0;
	}
	metrics.avg_customer_waiting_time = 0;
	metrics.avg_teller_time = 0;
	metrics.avg_teller_waiting_time = 0;
	metrics.max_customer_wait_time = 0;
	metrics.max_teller_wait_time = 0;
	metrics.max_transaction_time = 0;
	metrics.max_queue_depth = 0;
	metrics.total_customer_queue_time = 0;
	metrics.total_customer_teller_time = 0;
	metrics.total_teller_wait_time = 0;
	return metrics;
}

void bank_thread(void* argument){
	print_line("Entering bank thread\r\n");
	start = xTaskGetTickCount();
	TickType_t last_thread_wake = start;
	set_current_time(0);
	bank.is_open = 1;
	int customers_entered = 0;
	int localSim = get_current_time();
	while(bank.is_open == 1){
		int customer_interval = get_random(300) + 100;
		set_current_time(localSim += customer_interval);
		if(get_current_time() >= 42000){
			bank.is_open = 0;
		}
		vTaskDelayUntil(&last_thread_wake, customer_interval);
		int customer_transaction_time = get_random(450) + 30;
		struct Customer_t customer = {customers_entered, last_thread_wake, 0,0, customer_transaction_time};
		xQueueSend(bank.customers,&customer,0);
		xSemaphoreTake(metric_mutex, 10000);
		if(metrics.max_queue_depth < uxQueueMessagesWaiting(bank.customers)){
			metrics.max_queue_depth = uxQueueMessagesWaiting(bank.customers);
		}
		xSemaphoreGive(metric_mutex);
		customers_entered++;
		print_metrics();
	}
	xSemaphoreTake(metric_mutex, 10000);
	metrics.customers_served = customers_entered;
	xSemaphoreGive(metric_mutex);
	int customers_left_in_queue = uxQueueMessagesWaiting(bank.customers);
	while(1){
		int current_customers = (int)uxQueueMessagesWaiting(bank.customers);
		if(current_customers < customers_left_in_queue - 3){
			print_metrics();
			customers_left_in_queue = current_customers;
		}
		bool teller_busy[3] = {true, true, true};
		for(int j = 0; j<NUM_TELLERS; j++){
			if(bank.tellers[j].teller_status != BUSY){
				teller_busy[j] = false;
			}
		}
		if(customers_left_in_queue <= 0 && !teller_busy[0]&& !teller_busy[1]&& !teller_busy[2] ){
			break;
		}
	}
	xSemaphoreTake(metric_mutex, 10000000);
	metrics.avg_customer_waiting_time = (metrics.total_customer_queue_time/(float)metrics.customers_served)/100.0;
	metrics.avg_teller_time = (metrics.total_customer_teller_time/(float)metrics.customers_served)/100.0;
	metrics.avg_teller_waiting_time = (metrics.total_teller_wait_time/(float)metrics.customers_served)/100.0;
	xSemaphoreGive(metric_mutex);
	print_line("Total metrics:");
	int_format_print("Total Customers Served: %d\r\n", metrics.customers_served);
	int_format_print("Teller 1 Served %d Customers\r\n", metrics.customers_served_per_teller[0]);
	int_format_print("Teller 2 Served %d Customers\r\n", metrics.customers_served_per_teller[1]);
	int_format_print("Teller 3 Served %d Customers\r\n", metrics.customers_served_per_teller[2]);
	float_format_print("Average Customer Waiting Time: %f\r\n", metrics.avg_customer_waiting_time);
	float_format_print("Average Time Tellers Spent Helping Customers: %f\r\n", metrics.avg_teller_time);
	float_format_print("Average Time Tellers Spent Waiting: %f\r\n", metrics.avg_teller_waiting_time);
	float_format_print("Max Customer Wait Time: %f\r\n", metrics.max_customer_wait_time/100.0);
	float_format_print("Max Time Tellers Were Waiting: %f\r\n", metrics.max_teller_wait_time/100.0);
	float_format_print("Max Transaction Time: %f\r\n", metrics.max_transaction_time/100.0);
	int_format_print("Max Queue Depth: %d\r\n", metrics.max_queue_depth);
	xSemaphoreGive(metric_mutex);
	while(1){}
}

void thread_init(void){
	HAL_mutex = xSemaphoreCreateMutex();
	metric_mutex = xSemaphoreCreateMutex();
	print_line( "Starting threads...\r\n");
	bank_init();
	metrics = metric_init();
	xTaskCreate(bank_thread, "bank_thread", 128, 0, osPriorityNormal, 0);
	for(int i = 0; i < NUM_TELLERS; i++){
		id[i] = i;
		xTaskCreate(teller_thread, "teller_thread", 128, (void*) &id[i], osPriorityNormal, 0);
	}
}


void teller_thread(void* argument){
	int i = *(int*) argument;
	TickType_t last_thread_wake = xTaskGetTickCount();
	float break_interval = get_random(3000)+3000;
	float break_len = get_random(300) + 100;
	float break_time_at = last_thread_wake + break_interval;
	for(;;){
		bank.tellers[i].teller_status = IDLE;
		struct Customer_t customer;
		customer.time_entered_queue = xTaskGetTickCount();
		BaseType_t rec = pdTRUE;
		for(;;){
			rec = xQueueReceive(bank.customers, &customer, break_time_at - last_thread_wake);
			if(rec != pdTRUE){
				bank.tellers[i].teller_status = BREAK;
				xSemaphoreTake(metric_mutex, 10000);
				metrics.total_num_breaks[i]++;
				xSemaphoreGive(metric_mutex);
				osDelay(break_len);
				last_thread_wake = xTaskGetTickCount();
				bank.tellers[i].teller_status = IDLE;
				xSemaphoreTake(metric_mutex, 10000);
				xSemaphoreGive(metric_mutex);
				break_interval = get_random(3000)+3000;
				break_len = get_random(300)+100;
				break_time_at = last_thread_wake + break_interval;
			}
			else{
				break;
			}
		}
		uint32_t teller_wait_time = xTaskGetTickCount() - last_thread_wake;
		bank.tellers[i].num_customers += 1;
		bank.tellers[i].teller_status = BUSY;
		uint32_t dequeue_time = xTaskGetTickCount();
		uint32_t customer_time_in_queue = dequeue_time - customer.time_entered_queue;
		uint32_t transaction_time = get_random(450) + 30;
		bank.tellers[i].total_transaction_time += transaction_time;
		osDelay(transaction_time);
		last_thread_wake = xTaskGetTickCount();
		xSemaphoreTake(metric_mutex, 10000);
		metrics.total_customer_queue_time += customer_time_in_queue;
		if(customer_time_in_queue > metrics.max_customer_wait_time){
			metrics.max_customer_wait_time = customer_time_in_queue;
		}
		metrics.total_customer_teller_time += transaction_time;
		if(transaction_time > metrics.max_transaction_time){
			metrics.max_transaction_time = transaction_time;
		}
		metrics.customers_served_per_teller[i]++;
		metrics.total_teller_wait_time = teller_wait_time;
		if(teller_wait_time > metrics.max_teller_wait_time){
			metrics.max_teller_wait_time = teller_wait_time;
		}
		xSemaphoreGive(metric_mutex);
		if(get_current_time() >= 42000 && uxQueueMessagesWaiting(bank.customers) == 0){
			bank.tellers[i].teller_status = IDLE;
			break;
		}
	}
	while(1){}
}

int get_random(int max){
	uint32_t random_number;
	HAL_RNG_GenerateRandomNumber(&hrng, &random_number);
	random_number = random_number%max;

	return (int)random_number;
}

char* teller_status_to_string(enum status teller_status){
	if(teller_status == IDLE){
		return "Idle";
	}
	else if(teller_status == BUSY){
		return "Busy";
	}
	else if(teller_status == BREAK){
		return "Break";
	}
	else{
		return "Unknown";
	}
}
