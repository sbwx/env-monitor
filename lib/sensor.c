/*
 * Copyright (c) 2018 Peter Bigot Consulting, LLC
 * Copyright (c) 2018 Linaro Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

 #include <zephyr/kernel.h>
 #include <zephyr/device.h>
 #include <zephyr/drivers/sensor.h>
 #include <zephyr/sys/printk.h>
 #include <stdio.h>
 #include <zephyr/sys/mpsc_lockfree.h>
 #include "sensor.h"

 struct mpsc queue;

 const struct device *const temphum_sensor = DEVICE_DT_GET(DT_ALIAS(temphumsensor));
 const struct device *const pressure_sensor = DEVICE_DT_GET(DT_ALIAS(pressuresensor));
 const struct device *const gas_sensor = DEVICE_DT_GET(DT_ALIAS(gassensor));

 struct k_sem temp_sem;
 struct k_sem humid_sem;
 struct k_sem pressure_sem;
 struct k_sem gas_sem;

 #define STACK_SIZE 4096
 #define TEMP_THREAD_PRIORITY 5
 #define HUMIDITY_THREAD_PRIORITY 5
 #define PRESSURE_THREAD_PRIORITY 5
 #define GAS_THREAD_PRIORITY 5

 K_THREAD_DEFINE(temp_tid, STACK_SIZE, temp_thread, NULL, NULL, NULL, TEMP_THREAD_PRIORITY, 0, 0);
 K_THREAD_DEFINE(humid_tid, STACK_SIZE, humid_thread, NULL, NULL, NULL, HUMIDITY_THREAD_PRIORITY, 0, 0);
 K_THREAD_DEFINE(pressure_tid, STACK_SIZE, pressure_thread, NULL, NULL, NULL, PRESSURE_THREAD_PRIORITY, 0, 0);
 K_THREAD_DEFINE(gas_tid, STACK_SIZE, gas_thread, NULL, NULL, NULL, GAS_THREAD_PRIORITY, 0, 0);

 void temp_thread() {
        // init variables
        struct sensor_value temp;
        float temp_f;
        struct custom_mpsc_node* new_node;
    
        mpsc_init(&queue);
        k_sem_init(&temp_sem, 0, 1);
    
        while (1) {
            k_sem_take(&temp_sem, K_FOREVER);
            new_node = (struct custom_mpsc_node*) k_malloc(sizeof(struct custom_mpsc_node));
            // Create a new custom node and store data in it
            new_node->node.next = NULL;
    
            // fetch sensor data
            sensor_sample_fetch(temphum_sensor);
            // get sensor data
            sensor_channel_get(temphum_sensor, SENSOR_CHAN_AMBIENT_TEMP, &temp);
            // convert sensor data to double, cast to float
            temp_f = sensor_value_to_double(&temp);
    
            new_node->data = temp_f;
            new_node->sensor = TEMP;
    
            // Push the custom node onto the queue
            mpsc_push(&queue, &new_node->node);
        }
 }

 void humid_thread() {
    // init variables
    struct sensor_value humid;
    float humid_f;
    struct custom_mpsc_node* new_node;

    mpsc_init(&queue);
    k_sem_init(&humid_sem, 0, 1);

    while (1) {
        k_sem_take(&humid_sem, K_FOREVER);
        new_node = (struct custom_mpsc_node*) k_malloc(sizeof(struct custom_mpsc_node));
        // Create a new custom node and store data in it
        new_node->node.next = NULL;

        // fetch sensor data
        sensor_sample_fetch(temphum_sensor);
        // get sensor data
        sensor_channel_get(temphum_sensor, SENSOR_CHAN_HUMIDITY, &humid);
        // convert sensor data to double, cast to float
        humid_f = sensor_value_to_double(&humid);

        new_node->data = humid_f;
        new_node->sensor = HUMIDITY;

        // Push the custom node onto the queue
        mpsc_push(&queue, &new_node->node);
    }
 }

 void pressure_thread() {
    // init variables
    struct sensor_value pres;
    float pres_f;
    struct custom_mpsc_node* new_node;

    mpsc_init(&queue);
    k_sem_init(&pressure_sem, 0, 1);

    while (1) {
        k_sem_take(&pressure_sem, K_FOREVER);
        new_node = (struct custom_mpsc_node*) k_malloc(sizeof(struct custom_mpsc_node));
        // Create a new custom node and store data in it
        new_node->node.next = NULL;

        // fetch sensor data
        sensor_sample_fetch(pressure_sensor);
        // get sensor data
        sensor_channel_get(pressure_sensor, SENSOR_CHAN_PRESS, &pres);
        // convert sensor data to double, cast to float
        pres_f = sensor_value_to_double(&pres);

        new_node->data = pres_f;
        new_node->sensor = PRESSURE;

        // Push the custom node onto the queue
        mpsc_push(&queue, &new_node->node);
    }
 }

 void gas_thread() {
    // init variables
    struct sensor_value co2;
    struct sensor_value voc;
    float co2_f;
    float voc_f;
    struct custom_mpsc_node* new_node;

    mpsc_init(&queue);
    k_sem_init(&gas_sem, 0, 1);

    while (1) {
        k_sem_take(&gas_sem, K_FOREVER);
        new_node = (struct custom_mpsc_node*) k_malloc(sizeof(struct custom_mpsc_node));
        // Create a new custom node and store data in it
        new_node->node.next = NULL;

        // fetch sensor data
        sensor_sample_fetch(gas_sensor);
        // get sensor data
        sensor_channel_get(gas_sensor, SENSOR_CHAN_CO2, &co2);
        sensor_channel_get(gas_sensor, SENSOR_CHAN_VOC, &voc);
        // convert sensor data to double, cast to float
        co2_f = sensor_value_to_double(&co2);
        voc_f = sensor_value_to_double(&voc);

        new_node->data = voc_f;
        new_node->sensor = GAS;

        // Push the custom node onto the queue
        mpsc_push(&queue, &new_node->node);
    }
 }
 
 