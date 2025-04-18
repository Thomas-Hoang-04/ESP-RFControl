#include <stdio.h>
#include <string.h>
#include "rf_common.h"
#include "esp_log.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"


esp_err_t translate_tristate(const char* data, RFTransmitter* rf_rmt) {
    const char* log_data = data;
    ESP_RETURN_ON_FALSE(rf_rmt && data, ESP_ERR_INVALID_ARG, TAG, "Invalid RF transmitter");
    if (rf_rmt->pulses) {
        free(rf_rmt->pulses);
        rf_rmt->pulses = NULL;
    }
    rf_rmt->pulse_count = strlen(data) * 4 + 2;
    rf_rmt->pulses = malloc(rf_rmt->pulse_count * sizeof(RFPulse));
    const Protocol* proto = rf_rmt->proto;
    int idx = 0;

    uint8_t firstLogic = proto->inverted ? 0 : 1;
    uint8_t secondLogic = proto->inverted ? 1 : 0;
    while (*data) {
        if (*data == '0') {
            rf_rmt->pulses[idx].level = firstLogic;
            rf_rmt->pulses[idx].pulse_length = proto->zero.high * proto->pulse_length;
            rf_rmt->pulses[idx + 1].level = secondLogic;
            rf_rmt->pulses[idx + 1].pulse_length = proto->zero.low * proto->pulse_length;

            rf_rmt->pulses[idx + 2].level = firstLogic;
            rf_rmt->pulses[idx + 2].pulse_length = proto->zero.high * proto->pulse_length;
            rf_rmt->pulses[idx + 3].level = secondLogic;
            rf_rmt->pulses[idx + 3].pulse_length = proto->zero.low * proto->pulse_length;
        } else if (*data == '1') {
            rf_rmt->pulses[idx].level = firstLogic;
            rf_rmt->pulses[idx].pulse_length = proto->one.high * proto->pulse_length;
            rf_rmt->pulses[idx + 1].level = secondLogic;
            rf_rmt->pulses[idx + 1].pulse_length = proto->one.low * proto->pulse_length;

            rf_rmt->pulses[idx + 2].level = firstLogic;
            rf_rmt->pulses[idx + 2].pulse_length = proto->one.high * proto->pulse_length;
            rf_rmt->pulses[idx + 3].level = secondLogic;
            rf_rmt->pulses[idx + 3].pulse_length = proto->one.low * proto->pulse_length;
        } else if (*data == 'F') {
            rf_rmt->pulses[idx].level = firstLogic;
            rf_rmt->pulses[idx].pulse_length = proto->zero.high * proto->pulse_length;
            rf_rmt->pulses[idx + 1].level = secondLogic;
            rf_rmt->pulses[idx + 1].pulse_length = proto->zero.low * proto->pulse_length;

            rf_rmt->pulses[idx + 2].level = firstLogic;
            rf_rmt->pulses[idx + 2].pulse_length = proto->one.high * proto->pulse_length;
            rf_rmt->pulses[idx + 3].level = secondLogic;
            rf_rmt->pulses[idx + 3].pulse_length = proto->one.low * proto->pulse_length;
        } else {
            ESP_LOGE(TAG, "Invalid data: %c", *data);
            return ESP_ERR_INVALID_ARG;
        }
        idx += 4;
        data++;
    }

    rf_rmt->pulses[idx].level = firstLogic;
    rf_rmt->pulses[idx].pulse_length = proto->sync_factor.high * proto->pulse_length;
    rf_rmt->pulses[idx + 1].level = secondLogic;
    rf_rmt->pulses[idx + 1].pulse_length = proto->sync_factor.low * proto->pulse_length;

    ESP_LOGI(TAG, "Data %s translated to %d pulses", log_data, rf_rmt->pulse_count);

    return ESP_OK;
}

esp_err_t rf_init(gpio_num_t tx_gpio, int8_t repeat_count, Protocol* tx_proto, RFTransmitter* rf_rmt) {
    ESP_RETURN_ON_FALSE(rf_rmt && tx_proto, ESP_ERR_INVALID_ARG, TAG, "Invalid RF module");

    rf_rmt->init = false;
    gpio_config_t io_conf_tx = {
        .pin_bit_mask = (1ULL << tx_gpio),
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf_tx));
    ESP_ERROR_CHECK(gpio_set_level(tx_gpio, 0));
    rf_rmt->tx_gpio = tx_gpio;
    ESP_LOGI(TAG, "GPIO %d configured for RF transmission", tx_gpio);

    rf_rmt->repeat_count = repeat_count;
    rf_rmt->proto = tx_proto;

    rf_rmt->pulses = NULL;
    rf_rmt->tx_active = false;

    rf_rmt->rx_gpio = GPIO_NUM_NC;
    rf_rmt->rx_gpio_state = GPIO_NUM_NC;

    ESP_ERROR_CHECK(rf_timer_init(rf_rmt));

    rf_rmt->init = true;
    ESP_LOGI(TAG, "RF transmitter initialized");

    return ESP_OK;
}

esp_err_t rf_deinit(RFTransmitter* rf_rmt) {
    ESP_RETURN_ON_FALSE(rf_rmt && rf_rmt->init, ESP_ERR_INVALID_ARG, TAG, "Invalid RF module");
    if (rf_rmt->pulses) {
        free(rf_rmt->pulses);
        rf_rmt->pulses = NULL;
    }
    rf_rmt->pulse_count = 0;
    rf_rmt->pulse_index = 0;

    rf_rmt->current_rep = 0;
    rf_rmt->repeat_count = 0;
    rf_rmt->tx_active = false;

    rf_rmt->proto = NULL;

    ESP_ERROR_CHECK(rf_timer_deinit(rf_rmt->timer));

    ESP_ERROR_CHECK(gpio_set_level(rf_rmt->tx_gpio, 0));
    gpio_reset_pin(rf_rmt->tx_gpio);
    rf_rmt->tx_gpio = GPIO_NUM_NC;

    ESP_ERROR_CHECK(rf_recv_deinit(rf_rmt));
    gpio_uninstall_isr_service();

    rf_rmt->init = false;
    ESP_LOGI(TAG, "RF transmitter deinitialized");
    return ESP_OK;
}

esp_err_t rf_send(RFTransmitter* rf_rmt) {
    ESP_RETURN_ON_FALSE(rf_rmt && rf_rmt->init, ESP_ERR_INVALID_ARG, TAG, "Invalid RF transmitter");
    ESP_RETURN_ON_FALSE(rf_rmt->pulses && rf_rmt->pulse_count > 0, ESP_ERR_INVALID_ARG, TAG, "Invalid RF data");
    ESP_RETURN_ON_FALSE(!rf_rmt->tx_active, ESP_ERR_INVALID_STATE, TAG, "RF transmitter is already active");

    ESP_LOGI(TAG, "Starting RF transmission");
    if (rf_rmt->rx_gpio != GPIO_NUM_NC) {
        rf_rmt->rx_gpio_state = rf_rmt->rx_gpio;
        ESP_ERROR_CHECK(rf_recv_deinit(rf_rmt));
    }

    rf_rmt->tx_active = true;
    rf_rmt->current_rep = 0;
    rf_rmt->pulse_index = 0;

    gpio_set_level(rf_rmt->tx_gpio, rf_rmt->pulses[rf_rmt->pulse_index].level);
    gptimer_alarm_config_t alarm_config = {
        .alarm_count = rf_rmt->pulses[rf_rmt->pulse_index].pulse_length,
    };
    gptimer_set_alarm_action(rf_rmt->timer, &alarm_config);
    gptimer_start(rf_rmt->timer);
    rf_rmt->pulse_index++;

    return ESP_OK;
}