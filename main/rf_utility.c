#include "rf_common.h"
#include "esp_check.h"

static void IRAM_ATTR reset_isr_handler(void* arg) {
    RFReceiver* rf_recv_mod = (RFReceiver*)arg;
    ESP_RETURN_VOID_ON_FALSE_ISR(rf_recv_mod->reset_gpio != GPIO_NUM_NC, ESP_ERR_INVALID_STATE, TAG, "Reset GPIO is not active");

    gpio_set_level(rf_recv_mod->motor_gpio, 0);
    if (rf_recv_mod->timer_running) {
        gptimer_stop(rf_recv_mod->timer);
        rf_recv_mod->timer_running = false;
    }
    gpio_set_level(rf_recv_mod->led_status_gpio, 0);
    rf_recv_mod->led_status = 0;
    rf_recv_mod->motor_status = 0;
}

void led_flash(RFReceiver* rf_recv_mod) {
    ESP_RETURN_VOID_ON_FALSE(rf_recv_mod && rf_recv_mod->init, TAG, "Invalid RF module");
    ESP_RETURN_VOID_ON_FALSE(rf_recv_mod->rx_gpio != GPIO_NUM_NC, TAG, "RF receiver is not active");

    if (rf_recv_mod->timer_running) {
        gptimer_stop(rf_recv_mod->timer);
        rf_recv_mod->timer_running = false;
    }
    gpio_set_level(rf_recv_mod->led_status_gpio, 1);
    gptimer_alarm_config_t alarm_conf = {
        .alarm_count = DEFAULT_RESOLUTION * 2,
    };
    ESP_ERROR_CHECK(gptimer_set_alarm_action(rf_recv_mod->timer, &alarm_conf));
    ESP_ERROR_CHECK(gptimer_start(rf_recv_mod->timer));
    rf_recv_mod->timer_running = true;
    ESP_LOGI(TAG, "LED flash triggered");
}

void motor_trigger_led(RFReceiver* rf_recv_mod) {
    ESP_RETURN_VOID_ON_FALSE(rf_recv_mod && rf_recv_mod->init, TAG, "Invalid RF module");
    ESP_RETURN_VOID_ON_FALSE(rf_recv_mod->rx_gpio != GPIO_NUM_NC, TAG, "RF receiver is not active");

    if (rf_recv_mod->timer_running) {
        gptimer_stop(rf_recv_mod->timer);
        rf_recv_mod->timer_running = false;
    }

    if (!rf_recv_mod->motor_status) {
        ESP_ERROR_CHECK(gpio_set_level(rf_recv_mod->led_status_gpio, 0));
        rf_recv_mod->led_status = 0;
        ESP_LOGI(TAG, "Motor trigger LED deactivated");
        return;
    }

    rf_recv_mod->led_status = 1;
    gpio_set_level(rf_recv_mod->led_status_gpio, 1);
    gptimer_alarm_config_t alarm_conf = {
        .reload_count = 0,
        .alarm_count = DEFAULT_RESOLUTION,
        .flags.auto_reload_on_alarm = true,
    };
    ESP_ERROR_CHECK(gptimer_set_alarm_action(rf_recv_mod->timer, &alarm_conf));
    ESP_ERROR_CHECK(gptimer_start(rf_recv_mod->timer));
    rf_recv_mod->timer_running = true;
    ESP_LOGI(TAG, "Motor trigger LED activated");
}

esp_err_t set_reset_isr(gpio_num_t reset_gpio, RFReceiver* rf_recv_mod) {
    ESP_ERROR_CHECK(gpio_isr_handler_add(reset_gpio, reset_isr_handler, rf_recv_mod));
    ESP_LOGI(TAG, "ISR handler added for GPIO %d", reset_gpio);

    return ESP_OK;
}