/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <string.h>
#include "rf_common.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#define RECV_GPIO GPIO_NUM_2
#define LED_GPIO GPIO_NUM_5
#define MOTOR_STATUS_GPIO GPIO_NUM_6
#define MOTOR_GPIO GPIO_NUM_7
#define RESET_GPIO GPIO_NUM_9

static void reception_task(void* arg)
{
    RFReceiver* rf_recv_mod = (RFReceiver*)arg;
    ESP_ERROR_CHECK(rf_recv_init(RECV_GPIO, rf_recv_mod));
    ESP_LOGI(TAG, "Waiting for RF reception...");
    while (1) {
        if (recv_available(rf_recv_mod) == ESP_OK) {
            output_recv(rf_recv_mod);
            if (rf_recv_mod->recv_value == ref_val) {
                rf_recv_mod->motor_status = !rf_recv_mod->motor_status;
                ESP_ERROR_CHECK(gpio_set_level(rf_recv_mod->motor_gpio, rf_recv_mod->motor_status));
                vTaskDelay(pdMS_TO_TICKS(25));
                ESP_ERROR_CHECK(gpio_set_level(rf_recv_mod->motor_status_gpio, rf_recv_mod->motor_status));
            } else {
                led_flash(rf_recv_mod);
            }
            reset_recv(rf_recv_mod);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void app_main(void)
{
    RFReceiver rf_recv_mod;
    ESP_ERROR_CHECK(rf_init(LED_GPIO, MOTOR_GPIO, MOTOR_STATUS_GPIO, RESET_GPIO, &rf_recv_mod));

    vTaskDelay(1000 / portTICK_PERIOD_MS);

    xTaskCreate(reception_task, "reception_task", 8192, &rf_recv_mod, 5, &rf_recv_mod.rf_recv_handle);
    ESP_LOGI(TAG, "RF reception task created");

    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    // gpio_num_t led_gpio = GPIO_NUM_7;
    // gpio_config_t output_test = {
    //     .pin_bit_mask = (1ULL << led_gpio),
    //     .mode = GPIO_MODE_OUTPUT,
    //     .pull_up_en = GPIO_PULLUP_DISABLE,
    //     .pull_down_en = GPIO_PULLDOWN_DISABLE,
    //     .intr_type = GPIO_INTR_DISABLE
    // };
    // ESP_ERROR_CHECK(gpio_config(&output_test));
    // ESP_LOGI(TAG, "GPIO %d configured for output", led_gpio);

    // while (1) {
    //     ESP_ERROR_CHECK(gpio_set_level(led_gpio, 1));
    //     vTaskDelay(pdMS_TO_TICKS(5000));
    //     ESP_ERROR_CHECK(gpio_set_level(led_gpio, 0));
    //     vTaskDelay(pdMS_TO_TICKS(1500));
    // }
}
