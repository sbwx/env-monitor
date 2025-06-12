/*
#ifndef RGB_H
#define RGB_H

#include <zephyr/kernel.h>
#include <stdint.h>

typedef struct {
    uint8_t b; // Blue
    uint8_t g; // Green
    uint8_t r; // Red
} color_t;

extern struct k_msgq color_msgq;

void clk_pulse();
int chainable_led_init();
void send_byte(uint8_t b);
void send_color(uint8_t red, uint8_t green, uint8_t blue);
void set_rgb_color(uint8_t red, uint8_t green, uint8_t blue);
void rgb_thread();

#endif
*/

