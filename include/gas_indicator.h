#ifndef INDICATOR_H
#define INDICATOR_H

#define STACK_SIZE 4096
#define IND_THREAD_PRIORITY 5

typedef struct {
	uint8_t r; // Red
    uint8_t g; // Green
    uint8_t b; // Blue
} color_t;

color_t colors[] = {
    {1, 0, 1}, // Magenta
    {0, 0, 1}, // Blue
    {0, 1, 0}, // Green
    {0, 1, 1}, // Cyan
    {1, 0, 0}, // Red
    {1, 1, 0}, // Yellow
    {1, 1, 1}  // White
};

void ind_thread();

#endif