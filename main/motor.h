#pragma once
#include <driver/ledc.h>
#include "pins.h"

#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    uint8_t fwd_pin, rev_pin, ledc_channel;
} motor_t;

extern motor_t motor_left, motor_right;

void motor_power(motor_t* motor, int power);
void straight(int power);

#ifdef __cplusplus
}
#endif