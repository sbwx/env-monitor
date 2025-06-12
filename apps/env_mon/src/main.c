#include <stdio.h>
#include <stdlib.h>
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/mpsc_lockfree.h>
#include <zephyr/data/json.h>
#include <SEGGER_RTT.h>
#include "rtc.h"
#include "sensor.h"

// thread macros
#define STACK_SIZE 4096
#define SHELL_THREAD_PRIORITY 5
#define SAMPLE_THREAD_PRIORITY 6

// global vars
char* sensorNames[] = {"Temperature", "Humidity", "Pressure", "VOC"};
uint32_t sampleRate = 1000;
uint8_t sensorEnable[4] = {0};
struct k_sem* sensorSems[] = {&temp_sem, &humid_sem, &pressure_sem, &gas_sem};
uint8_t lastSampled = 16;

static uint8_t channel1UpBuf[1024];
static uint8_t channel1DownBuf[64];
static uint8_t channel0UpBuf[1024];
static uint8_t channel0DownBuf[64];

const struct shell* sh_p;

// prototypes
void uartshell_thread();
void sensor_handler();
void rtc_read();
void rtc_write();
void sample_start();
void sample_stop();
void sample_time_set();
void sampling_thread();

// shell commands
SHELL_CMD_ARG_REGISTER(sensor, NULL, "Returns the sensor value of the DID provided.", &sensor_handler, 2, 0);

SHELL_STATIC_SUBCMD_SET_CREATE(rtc_rw, SHELL_CMD_ARG(w, NULL, "Writes the system time.", &rtc_write, 2, 0), SHELL_CMD(r, NULL, "Read the current system time.", &rtc_read), SHELL_SUBCMD_SET_END);
SHELL_CMD_ARG_REGISTER(rtc, &rtc_rw, "Reads/write to the RTC.", NULL, 2, 1);

SHELL_STATIC_SUBCMD_SET_CREATE(sample_sub, SHELL_CMD_ARG(s, NULL, "Starts continuous sampling of the specified DID.", &sample_start, 2, 0), SHELL_CMD_ARG(p, NULL, "Stops continuous sampling of the specified DID.", &sample_stop, 2, 0), SHELL_CMD_ARG(w, NULL, "Sets the sampling time in seconds.", &sample_time_set, 2, 0), SHELL_SUBCMD_SET_END);
SHELL_CMD_ARG_REGISTER(sample, &sample_sub, "Starts/stops continuous sampling.", NULL, 3, 0);

K_THREAD_DEFINE(shell_tid, STACK_SIZE, uartshell_thread, NULL, NULL, NULL, SHELL_THREAD_PRIORITY, 0, 0);
const struct device* dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_shell_uart));
LOG_MODULE_REGISTER(cli_module, LOG_LEVEL_DBG);

K_THREAD_DEFINE(sample_tid, STACK_SIZE, sampling_thread, NULL, NULL, NULL, SAMPLE_THREAD_PRIORITY, 0, 0);

const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);
static struct gpio_callback button_cb_data;

void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
	if (lastSampled == 16) {
		LOG_ERR("No sampling has been selected yet.");
		return;
	}
	if (lastSampled == 15) {
		for (int i = 0; i < 4; i++) {
			sensorEnable[i] = !sensorEnable[i];
		}
	} else {
		sensorEnable[lastSampled] = !sensorEnable[lastSampled];
	}
}

// thread to initialise the shell
void uartshell_thread() {
	k_sched_lock();

	init_rtc();
	
	uint32_t dtr = 0;

	gpio_pin_configure_dt(&button, GPIO_INPUT);

	gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_TO_ACTIVE);

	gpio_init_callback(&button_cb_data, button_pressed, BIT(button.pin));
	gpio_add_callback(button.port, &button_cb_data);

	k_sched_unlock();

    if (!device_is_ready(dev)) {
		LOG_ERR("device not ready");
	}	
	
	SEGGER_RTT_Init();

	SEGGER_RTT_ConfigUpBuffer(0, "CHANNEL 0", channel0UpBuf, 1024, SEGGER_RTT_MODE_BLOCK_IF_FIFO_FULL);
	SEGGER_RTT_ConfigDownBuffer(0, "CHANNEL 0", channel0DownBuf, 64, SEGGER_RTT_MODE_BLOCK_IF_FIFO_FULL);

	SEGGER_RTT_ConfigUpBuffer(1, "CHANNEL 1", channel1UpBuf, 1024, SEGGER_RTT_MODE_BLOCK_IF_FIFO_FULL);
	SEGGER_RTT_ConfigDownBuffer(1, "CHANNEL 1", channel1DownBuf, 64, SEGGER_RTT_MODE_BLOCK_IF_FIFO_FULL);

	while (!dtr) {
		uart_line_ctrl_get(dev, UART_LINE_CTRL_DTR, &dtr);
		k_sleep(K_MSEC(100));
	}
}

// handler to write to rtc
void rtc_write(const struct shell *sh, int argc, char** argv) {
	uint32_t num = atoll(argv[1]);
	set_time(num);
}

// handler for shell rtc read
void rtc_read(const struct shell *sh) {
	uint32_t hour = 0;
	uint32_t min = 0;
	uint32_t sec = get_time();
	if (sec >= 60) {
		min = sec / 60;
		sec %= 60;
	}
	if (min >= 60) {
		hour = (min / 60);
		min %= 60;
	}
	shell_print(sh, "\r\n   CURRENT TIME\r\nHours:   %10d\r\nMinutes: %10d\r\nSeconds: %10d\r\n", hour, min, sec);
}

void pop_node() {
	// to get from queue (put in main)
	struct mpsc_node* node = mpsc_pop(&queue);
	while (!node) {
		node = mpsc_pop(&queue);
	}
	struct custom_mpsc_node *custom_node = (struct custom_mpsc_node *)node;
	float data = custom_node->data;
	enum SensorID sensorDevice = custom_node->sensor;

	shell_print(sh_p, "%s: %f\r\n", sensorNames[sensorDevice], data);

	k_free(node);
}

void sensor_handler(const struct shell *sh, int argc, char** argv) {
	sh_p = sh;
	uint8_t did = atoi(argv[1]);
	if (did == 0 && strcmp(argv[1], "0")) {
		LOG_ERR("Not a valid DID.");
		return;
	}

	if (did != 0 && did != 1 && did != 2 && did != 3 && did != 15) {
		LOG_ERR("Device not implemented.");
		return;
	}
	if (did == 15) {
		for (int i = 0; i < 4; i++) {
			k_sem_give(sensorSems[i]);
			pop_node();
		}
	} else {
		k_sem_give(sensorSems[atoi(argv[1])]);
		pop_node();
	}
}

void sample_start(const struct shell *sh, int argc, char** argv) {
	sh_p = sh;
	uint8_t did = atoi(argv[1]);
	if (did == 0 && strcmp(argv[1], "0")) {
		LOG_ERR("Not a valid DID.");
		return;
	}
	if (did != 0 && did != 1 && did != 2 && did != 3 && did != 15) {
		LOG_ERR("Device not implemented.");
		return;
	}
	if (did == 15) {
		lastSampled = 15;
		for (int i = 0; i < 4; i++) {
			sensorEnable[i] = 1;
		}
	} else {
		lastSampled = did;
		sensorEnable[did] = 1;
	}
}

void sample_stop(const struct shell *sh, int argc, char** argv) {
	uint8_t did = atoi(argv[1]);
	if (did == 0 && strcmp(argv[1], "0")) {
		LOG_ERR("Not a valid DID.");
		return;
	}
	if (did != 0 && did != 1 && did != 2 && did != 3 && did != 15) {
		LOG_ERR("Device not implemented.");
		return;
	}
	if (did == 15) {
		lastSampled = 15;
		for (int i = 0; i < 4; i++) {
			sensorEnable[i] = 0;
		}
	} else {
		lastSampled = did;
		sensorEnable[did] = 0;
	}
}

void sample_time_set(const struct shell *sh, int argc, char** argv) {
	if (atoi(argv[1]) <= 0) {
		LOG_ERR("Invalid sample rate.");
		sampleRate = 1;
	} else {
		sampleRate = (atoi(argv[1])* 1000);
	}
}

void get_time_str(char buf[9]) {
	uint32_t hour = 0;
	uint32_t min = 0;
	uint32_t sec = get_time();
	if (sec >= 60) {
		min = sec / 60;
		sec %= 60;
	}
	if (min >= 60) {
		hour = (min / 60);
		min %= 60;
	}
	sprintf(buf, "%02d:%02d:%02d", hour, min, sec);
}

struct sensorData{
	int did;
	char* time;
	char* value[4];
	size_t values_len;
};

static const struct json_obj_descr sensor_descr[] = {
	JSON_OBJ_DESCR_PRIM(struct sensorData, did, JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_PRIM(struct sensorData, time, JSON_TOK_STRING),
	JSON_OBJ_DESCR_ARRAY(struct sensorData, value, 4, values_len, JSON_TOK_STRING),
};

void pop_sampling() {
	// to get from queue (put in main)
	if (sensorEnable[0] == 1 && sensorEnable[1] == 1 && sensorEnable[2] == 1 && sensorEnable[3] == 1) {
		// DID 15
		struct sensorData sData;

		sData.did = 15;

		char time_str[9];
		get_time_str(time_str);
		sData.time = (char*) k_malloc(sizeof(char) * 50);
		strcpy(sData.time, time_str);

		sData.values_len = 4;

		for (int i = 0; i < 4; i++) {
			struct mpsc_node* node = mpsc_pop(&queue);
			while (!node) {
				node = mpsc_pop(&queue);
			}
			struct custom_mpsc_node *custom_node = (struct custom_mpsc_node *)node;
			char float_str[16];
			sprintf(float_str, "%f", custom_node->data);
			sData.value[i] = (char*) k_malloc(sizeof(char) * 50);
			strcpy(sData.value[i], float_str);
			k_free(node);
		}

		char jason_buf[128];
		json_obj_encode_buf(sensor_descr, 3, &sData, jason_buf, 128);
	
		shell_print(sh_p, "%s\r\n", jason_buf);
		strcat(jason_buf, "\n");
		SEGGER_RTT_WriteString(1, jason_buf);

		k_free(sData.time);
		for (int i = 0; i < 4; i++) {
			k_free(sData.value[i]);
		}
	} else {
		struct mpsc_node* node = mpsc_pop(&queue);
		while (!node) {
			node = mpsc_pop(&queue);
		}
		struct custom_mpsc_node *custom_node = (struct custom_mpsc_node *)node;

		struct sensorData sData;

		sData.did = custom_node->sensor;

		char time_str[9];
		get_time_str(time_str);
		sData.time = (char*) k_malloc(sizeof(char) * 50);
		strcpy(sData.time, time_str);

		char float_str[16];
		sprintf(float_str, "%f", custom_node->data);
		sData.value[0] = (char*) k_malloc(sizeof(char) * 50);
		strcpy(sData.value[0], float_str);

		sData.values_len = 1;

		char jason_buf[128];
		json_obj_encode_buf(sensor_descr, 3, &sData, jason_buf, 128);
	
		shell_print(sh_p, "%s\r\n", jason_buf);
		strcat(jason_buf, "\n");
		SEGGER_RTT_WriteString(1, jason_buf);

		k_free(sData.time);
		k_free(sData.value[0]);
		k_free(node);
	}
}

void sampling_thread() {
	while (1) {
		if (sensorEnable[0] == 1 && sensorEnable[1] == 1 && sensorEnable[2] == 1 && sensorEnable[3] == 1) {
			for (int i = 0; i < 4; i++) {
				k_sem_give(sensorSems[i]);
			}
			pop_sampling();
		} else {
			for (int i = 0; i < 4; i++) {
				if (sensorEnable[i]) {
					k_sem_give(sensorSems[i]);
					pop_sampling();
				}
			}
		}
		k_msleep(sampleRate);
	}
}