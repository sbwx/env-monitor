#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <stdio.h>
#include <string.h>
#include <zephyr/drivers/counter.h>
#include "rtc.h"

static uint32_t initial_time = 0;
static uint32_t past_secs = 0;

const struct counter_top_cfg counter_struct = {
	.ticks = 4294967000,
	.callback = NULL,
	.user_data = NULL,
	.flags = 0
};

const struct device* counter_rtc = DEVICE_DT_GET(DT_ALIAS(rtc));

void init_rtc() {
	counter_set_top_value(counter_rtc, &counter_struct);
	counter_start(counter_rtc);
}

uint32_t get_time() {
	uint32_t ticks;
	uint32_t usecs;
	counter_get_value(counter_rtc, &ticks);
	usecs = counter_ticks_to_us(counter_rtc, ticks);
	return (initial_time + (usecs/1000000) - past_secs);
}

void set_time(uint32_t time) {
	uint32_t ticks;
	uint32_t usecs;
	counter_get_value(counter_rtc, &ticks);
	usecs = counter_ticks_to_us(counter_rtc, ticks);
	past_secs = (usecs/1000000);
	initial_time = time;
}