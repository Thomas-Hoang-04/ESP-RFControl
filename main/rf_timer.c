#include "rf_common.h"
#include "esp_log.h"

static bool IRAM_ATTR rf_timer_callback(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *arg) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    RFTransmitter* rf_rmt = (RFTransmitter*)arg;
    if (rf_rmt->tx_active) {
        if (rf_rmt->pulse_index < rf_rmt->pulse_count) {
            gpio_set_level(rf_rmt->tx_gpio, rf_rmt->pulses[rf_rmt->pulse_index].level);

            gptimer_alarm_config_t next_alarm = {
                .alarm_count = edata->alarm_value + rf_rmt->pulses[rf_rmt->pulse_index].pulse_length,
            };
            gptimer_set_alarm_action(timer, &next_alarm);

            rf_rmt->pulse_index++;
        } else {
            gptimer_stop(timer);
            gptimer_set_raw_count(timer, 0);
            gpio_set_level(rf_rmt->tx_gpio, 0);
            rf_rmt->pulse_index = 0;
            rf_rmt->current_rep++;
            if (rf_rmt->current_rep < rf_rmt->repeat_count) {
                gpio_set_level(rf_rmt->tx_gpio, 1);
                gptimer_alarm_config_t alarm_config = {
                    .alarm_count = rf_rmt->pulses[rf_rmt->pulse_index].pulse_length,
                };
                gptimer_set_alarm_action(timer, &alarm_config);
                gptimer_start(timer);
                rf_rmt->pulse_index++;
            } else {
                rf_rmt->tx_active = false;
                rf_rmt->current_rep = 0;
                if (rf_rmt->rf_trans_handle)
                    vTaskNotifyGiveFromISR(rf_rmt->rf_trans_handle, &xHigherPriorityTaskWoken);
            }
        }
    }

    return (xHigherPriorityTaskWoken == pdTRUE);
}

esp_err_t rf_timer_init(RFTransmitter* rf_rmt) {
    gptimer_config_t rf_timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = DEFAULT_RESOLUTION,
    };
    ESP_ERROR_CHECK(gptimer_new_timer(&rf_timer_config, &rf_rmt->timer));
    ESP_LOGI(TAG, "Timer created");
    gptimer_event_callbacks_t rf_timer_cb = {
        .on_alarm = rf_timer_callback,
    };
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(rf_rmt->timer, &rf_timer_cb, rf_rmt));
    ESP_LOGI(TAG, "Timer callbacks registered");
    ESP_ERROR_CHECK(gptimer_enable(rf_rmt->timer));
    ESP_LOGI(TAG, "Timer enabled");
    return ESP_OK;
}

esp_err_t rf_timer_deinit(gptimer_handle_t timer) {
    gptimer_stop(timer);
    ESP_ERROR_CHECK(gptimer_disable(timer));
    ESP_ERROR_CHECK(gptimer_del_timer(timer));
    ESP_LOGI(TAG, "Timer deleted");
    return ESP_OK;
}

esp_err_t rf_timer_reset(RFTransmitter* rf_rmt) {
    gptimer_stop(rf_rmt->timer);
    ESP_ERROR_CHECK(gptimer_set_raw_count(rf_rmt->timer, 0));
    rf_rmt->pulse_index = 0;
    rf_rmt->current_rep = 0;
    rf_rmt->tx_active = false;
    gpio_set_level(rf_rmt->tx_gpio, 0);
    ESP_LOGI(TAG, "Timer reset");
    return ESP_OK;
}