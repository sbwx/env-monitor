#ifndef SENSOR_H
#define SENSOR_H

#define CO2 0
#define VOC 1

extern struct k_sem temp_sem;
extern struct k_sem humid_sem;
extern struct k_sem pressure_sem;
extern struct k_sem gas_sem;
extern struct mpsc queue;

enum SensorID {
    TEMP,
    HUMIDITY,
    PRESSURE,
    GAS
};

struct custom_mpsc_node {
    struct mpsc_node node;
    float data;
    enum SensorID sensor;
};

void temp_thread();
void humid_thread();
void pressure_thread();
void gas_thread();

//extern void sense();

#endif