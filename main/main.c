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
            led_flash(rf_recv_mod);
            if (rf_recv_mod->recv_value == ref_val) {
                rf_recv_mod->motor_status = !rf_recv_mod->motor_status;
                ESP_ERROR_CHECK(gpio_set_level(rf_recv_mod->motor_gpio, rf_recv_mod->motor_status));
                vTaskDelay(pdMS_TO_TICKS(25));
                ESP_ERROR_CHECK(gpio_set_level(rf_recv_mod->motor_status_gpio, rf_recv_mod->motor_status));
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
}
