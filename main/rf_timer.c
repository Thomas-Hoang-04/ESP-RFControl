#include "rf_common.h"
#include "esp_check.h"

static bool IRAM_ATTR rf_timer_callback(gptimer_handle_t timer, const gptimer_alarm_event_data_t* edata, void* arg) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    RFReceiver* rf_recv_mod = (RFReceiver*)arg;

    gptimer_stop(timer);
    gptimer_set_raw_count(timer, 0);
    gpio_set_level(rf_recv_mod->signal_status_gpio, 0);
    rf_recv_mod->timer_running = false;

    return (xHigherPriorityTaskWoken == pdTRUE);
}

esp_err_t rf_timer_init(RFReceiver* rf_recv_mod) {
    gptimer_config_t rf_timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = DEFAULT_RESOLUTION,
        .intr_priority = ESP_INTR_FLAG_LEVEL1
    };
    ESP_ERROR_CHECK(gptimer_new_timer(&rf_timer_config, &rf_recv_mod->timer));
    ESP_LOGI(TAG, "Timer created");
    gptimer_event_callbacks_t rf_timer_cb = {
        .on_alarm = rf_timer_callback,
    };
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(rf_recv_mod->timer, &rf_timer_cb, rf_recv_mod));
    ESP_LOGI(TAG, "Timer callbacks registered");
    ESP_ERROR_CHECK(gptimer_enable(rf_recv_mod->timer));
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