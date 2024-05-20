#include <stdio.h>
#include "bootloader_common.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "pins.h"
#include <driver/ledc.h>
#include "esp_log.h"

#define Eb5 622.25
#define E5  659.25
#define Ab5 830.61
#define Bb5 932.33
#define Cb5 987.77
#define C6  1046.5

#define STACK_SIZE 8192
StaticTask_t task_blinky_tcb;
StackType_t task_blinky_stack[STACK_SIZE];

typedef struct {
    uint8_t fwd_pin, rev_pin, ledc_channel;
} motor_t;

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

void task_blinky(void) {
    while (true) {
        gpio_set_level(BLINKY, 1);
        vTaskDelay(25);
        gpio_set_level(BLINKY, 0);
        vTaskDelay(25);
    }
}

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

void note(int freq_hz) {
    ESP_LOGW("main", "Note, Hz: %d", freq_hz);
    ESP_ERROR_CHECK(ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0, freq_hz));
    vTaskDelay(21);
}

void app_main(void) {
    gpio_config_t gpio = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask =
                BLINKY_BITMASK |
                M1_FWD_BITMASK |
                M1_REV_BITMASK |
                M2_FWD_BITMASK |
                M2_REV_BITMASK,
        .pull_down_en = 0,
        .pull_up_en = 0,
    };
    ESP_ERROR_CHECK(gpio_config(&gpio));

    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 50000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    xTaskCreateStatic(
        (TaskFunction_t)task_blinky,       /* Function that implements the task. */
        "blinky",          /* Text name for the task. */
        STACK_SIZE,      /* Number of indexes in the xStack array. */
        NULL,    /* Parameter passed into the task. */
        tskIDLE_PRIORITY,/* Priority at which the task is created. */
        task_blinky_stack,          /* Array to use as the task's stack. */
        &task_blinky_tcb);  /* Variable to hold the task's data structure. */

    ESP_LOGW("main", "Spinning up...");
    int power = 0;
    while (power < 128) {
        straight(power);
        power++;
        vTaskDelay(1);
    }

    ESP_LOGW("main", "Finished spinning up, duty: %d", power);

    note(Ab5);
    note(E5);
    note(Ab5);
    note(Bb5);
    note(C6);
    note(Bb5);
    note(Ab5);
    note(E5);
    note(Eb5);
}