#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <stdio.h>
#include <zephyr/drivers/gpio.h>
#include "gas_indicator.h"

const struct device *const ind_sensor = DEVICE_DT_GET(DT_ALIAS(gassensor));

K_THREAD_DEFINE(ind_tid, STACK_SIZE, ind_thread, NULL, NULL, NULL, IND_THREAD_PRIORITY, 0, 0);

static const struct gpio_dt_spec led_red = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec led_green = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);
static const struct gpio_dt_spec led_blue = GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios);

void ind_thread() {
    struct sensor_value gas;
    float gas_f;

    gpio_pin_configure_dt(&led_red, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&led_green, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&led_blue, GPIO_OUTPUT_INACTIVE);

    while (1) {
        // fetch sensor data
        sensor_sample_fetch(ind_sensor);
        // get sensor data
        sensor_channel_get(ind_sensor, SENSOR_CHAN_VOC, &gas);
        // convert sensor data to double, cast to float
        gas_f = sensor_value_to_double(&gas);

        uint8_t index = (uint8_t)(gas_f / 66);
        if (index > 6) {
            index = 6;
        }
    
        gpio_pin_set_dt(&led_red, colors[index].r);
        gpio_pin_set_dt(&led_green, colors[index].g);
        gpio_pin_set_dt(&led_blue, colors[index].b);

        k_msleep(1000);
    }

}