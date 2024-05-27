#include "motor.h"

motor_t motor_left = {
    .fwd_pin = M2_REV,
    .rev_pin = M2_FWD,
    .ledc_channel = LEDC_CHANNEL_1,
};

motor_t motor_right = {
    .fwd_pin = M1_REV,
    .rev_pin = M1_FWD,
    .ledc_channel = LEDC_CHANNEL_0,
};


void motor_power(motor_t *motor, int power) {
    uint8_t active_pin, inactive_pin;
    uint8_t duty;
    if (power > 0) {
        active_pin = motor->fwd_pin;
        inactive_pin = motor->rev_pin;
        duty = power;
    } else if (power < 0) {
        active_pin = motor->rev_pin;
        inactive_pin = motor->fwd_pin;
        duty = -power;
    } else {
        active_pin = motor->fwd_pin;
        inactive_pin = motor->rev_pin;
        duty = 0;
    }

    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = motor->ledc_channel,
        .timer_sel      = LEDC_TIMER_0,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = active_pin,
        .duty           = duty, // Set duty to 0%
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

    gpio_config_t gpio = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1 << inactive_pin,
        .pull_down_en = 0,
        .pull_up_en = 0,
    };
    ESP_ERROR_CHECK(gpio_config(&gpio));
    gpio_set_level(inactive_pin, 0);
}

void straight(int power) {
    motor_power(&motor_left, power);
    motor_power(&motor_right, power);
}
