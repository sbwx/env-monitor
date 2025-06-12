#ifndef RTC_H
#define RTC_H

#include <stdint.h>

extern void init_rtc();
extern uint32_t get_time();
extern void set_time(uint32_t time);

#endif