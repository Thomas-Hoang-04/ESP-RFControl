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

static void transmission_task(void* arg)
{
    RFTransmitter* rf_rmt = (RFTransmitter*)arg;
    const char* code = "FFFFFFFF";
    const char* chn[4] = { "0001", "0010", "0100", "1000" };

    char buffer[13];
    while (1) {
        for (int i = 0; i < 4; i++) {
            memset(buffer, 0, sizeof(buffer));
            snprintf(buffer, sizeof(buffer), "%s%s", code, chn[i]);
            translate_tristate(buffer, rf_rmt);
            ESP_LOGI(TAG, "RF data prepared");
            vTaskDelay(pdMS_TO_TICKS(500));

            ESP_LOGI(TAG, "Waiting for transmission...");
            ESP_ERROR_CHECK(rf_send(rf_rmt));
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            ESP_LOGI(TAG, "Transmission completed");
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}

void app_main(void)
{
    Protocol proto = {
        .pulse_length = FAST_PULSE_LEN,
        .sync_factor = { .high = 1, .low = 31 },
        .zero = { .high = 1, .low = 3 },
        .one = { .high = 3, .low = 1 },
    };

    RFTransmitter rf_rmt;
    ESP_ERROR_CHECK(rf_init(GPIO_NUM_40, OPT_REPEAT_COUNT, &proto, &rf_rmt));

    vTaskDelay(1000 / portTICK_PERIOD_MS);

    xTaskCreate(transmission_task, "transmission_task", 8192, &rf_rmt, 5, &rf_rmt.rf_trans_handle);
    ESP_LOGI(TAG, "RF transmission task created");

    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
