#include <string.h>
#include "rf_common.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"

static inline uint32_t diff(uint32_t a, uint32_t b) {
    return (a > b) ? (a - b) : (b - a);
}

static bool IRAM_ATTR recv_proto(RFReceiver* rf_recv_mod, uint8_t proto_idx, uint32_t edge_count) {
    const Protocol curr_proto = proto[proto_idx];

    uint32_t code = 0;
    const uint32_t sync_len_in_pulses = curr_proto.sync_factor.low > curr_proto.sync_factor.high
        ? curr_proto.sync_factor.low : curr_proto.sync_factor.high;
    const uint32_t delay = rf_recv_mod->recv_timings[0] / sync_len_in_pulses;
    const uint32_t delay_tolerance = delay * rf_recv_mod->recv_tolerance / 100;

    const uint8_t first_data_timing = curr_proto.inverted ? 2 : 1;
    for (uint8_t i = first_data_timing; i < edge_count - 1; i += 2) {
        code <<= 1;
        if (diff(rf_recv_mod->recv_timings[i], delay * curr_proto.zero.high) < delay_tolerance &&
                diff(rf_recv_mod->recv_timings[i + 1], delay * curr_proto.zero.low) < delay_tolerance) {
            code |= 0;
        } else if (diff(rf_recv_mod->recv_timings[i], delay * curr_proto.one.high) < delay_tolerance &&
                diff(rf_recv_mod->recv_timings[i + 1], delay * curr_proto.one.low) < delay_tolerance) {
            code |= 1;
        } else {
            return false;
        }
    }

    // Ignore very short transmissions (Presumably noise)
    if (edge_count > 7) {
        rf_recv_mod->recv_value = code;
        rf_recv_mod->recv_bit_length = (edge_count - 1) / 2;
        rf_recv_mod->recv_delay = delay;
        rf_recv_mod->recv_proto = proto_idx;
        return true;
    }

    return false;
}

static void IRAM_ATTR rf_recv_isr_handler(void* arg) {
    RFReceiver* rf_recv_mod = (RFReceiver*)arg;

    static uint32_t edge_count = 0;
    static uint64_t last_time = 0;
    static uint32_t repeat_count = 0;

    const int64_t time = esp_timer_get_time();
    const uint32_t duration = (uint32_t)(time - last_time);

    if (duration > rf_recv_mod->separation_limit) {
        // Long stretch without signal level change -> presumably the gap between two transmissions
        if ((repeat_count == 0) || (diff(duration, rf_recv_mod->recv_timings[0]) < 200)) {
            /* Assuming the sender sending the signal multiple times with roughly the same gap period, 
            this long signal is close in length to the signal which start the previous records
            -> potentially confirming it being a gap between two transmissions */
            repeat_count++;
            if (repeat_count == 2) {
                for (uint8_t i = 0; i < PROTO_COUNT; i++)
                    if (recv_proto(rf_recv_mod, i, edge_count)) break;
                repeat_count = 0;
            }
        }

        edge_count = 0;
    }

    if (edge_count >= MAX_EDGES) {
        edge_count = 0;
        repeat_count = 0;
    }

    rf_recv_mod->recv_timings[edge_count++] = duration;
    last_time = (uint64_t)time;
}

esp_err_t rf_init(gpio_num_t signal_status_gpio, gpio_num_t motor_gpio, gpio_num_t motor_status_gpio,
    gpio_num_t reset_gpio, RFReceiver* rf_recv_mod) {
    ESP_RETURN_ON_FALSE(rf_recv_mod, ESP_ERR_INVALID_ARG, TAG, "Invalid RF module");

    rf_recv_mod->init = false;

    rf_recv_mod->rx_gpio = GPIO_NUM_NC;
    rf_recv_mod->rx_gpio_state = GPIO_NUM_NC;

    ESP_ERROR_CHECK(gpio_install_isr_service(ESP_INTR_FLAG_LEVEL3));

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << signal_status_gpio),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    ESP_ERROR_CHECK(gpio_set_level(signal_status_gpio, 0));
    ESP_LOGI(TAG, "GPIO %d configured for LED output", signal_status_gpio);
    rf_recv_mod->signal_status_gpio = signal_status_gpio;

    io_conf.pin_bit_mask = (1ULL << motor_gpio);
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    ESP_ERROR_CHECK(gpio_set_level(motor_gpio, 0));
    ESP_LOGI(TAG, "GPIO %d configured for motor output", motor_gpio);
    rf_recv_mod->motor_gpio = motor_gpio;
    rf_recv_mod->motor_status = 0;

    io_conf.pin_bit_mask = (1ULL << motor_status_gpio);
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    ESP_ERROR_CHECK(gpio_set_level(motor_status_gpio, 0));
    ESP_LOGI(TAG, "GPIO %d configured for motor status LED output", motor_status_gpio);
    rf_recv_mod->motor_status_gpio = motor_status_gpio;

    io_conf.pin_bit_mask = (1ULL << reset_gpio);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    ESP_ERROR_CHECK(set_reset_isr(reset_gpio, rf_recv_mod));
    ESP_LOGI(TAG, "GPIO %d configured for reset input", reset_gpio);
    rf_recv_mod->reset_gpio = reset_gpio;

    ESP_ERROR_CHECK(rf_timer_init(rf_recv_mod));
    rf_recv_mod->timer_running = false;

    rf_recv_mod->init = true;
    ESP_LOGI(TAG, "RF receiver initialized");

    return ESP_OK;
}

esp_err_t rf_deinit(RFReceiver* rf_recv_mod) {
    ESP_RETURN_ON_FALSE(rf_recv_mod && rf_recv_mod->init, ESP_ERR_INVALID_ARG, TAG, "Invalid RF module");

    ESP_ERROR_CHECK(rf_recv_deinit(rf_recv_mod, false));
    gpio_uninstall_isr_service();

    ESP_ERROR_CHECK(gpio_reset_pin(rf_recv_mod->signal_status_gpio));
    rf_recv_mod->signal_status_gpio = GPIO_NUM_NC;

    ESP_ERROR_CHECK(gpio_reset_pin(rf_recv_mod->motor_gpio));
    rf_recv_mod->motor_gpio = GPIO_NUM_NC;

    ESP_ERROR_CHECK(gpio_reset_pin(rf_recv_mod->motor_status_gpio));
    rf_recv_mod->motor_status_gpio = GPIO_NUM_NC;

    ESP_ERROR_CHECK(gpio_reset_pin(rf_recv_mod->reset_gpio));
    rf_recv_mod->reset_gpio = GPIO_NUM_NC;

    ESP_ERROR_CHECK(rf_timer_deinit(rf_recv_mod->timer));

    rf_recv_mod->init = false;
    ESP_LOGI(TAG, "RF module deinitialized");
    return ESP_OK;
}

esp_err_t rf_recv_init(gpio_num_t rx_gpio, RFReceiver* rf_recv_mod) {
    ESP_RETURN_ON_FALSE(rf_recv_mod && rf_recv_mod->init, ESP_ERR_INVALID_ARG, TAG, "Invalid RF receiver");

    rf_recv_mod->rx_gpio = rx_gpio;
    ESP_LOGI(TAG, "GPIO %d configured for RF reception", rx_gpio);

    gpio_config_t io_conf_rx = {
        .pin_bit_mask = (1ULL << rx_gpio),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf_rx));
    ESP_LOGI(TAG, "GPIO %d configured for RF reception", rx_gpio);
    ESP_ERROR_CHECK(gpio_isr_handler_add(rx_gpio, rf_recv_isr_handler, rf_recv_mod));
    ESP_LOGI(TAG, "ISR handler added for GPIO %d", rx_gpio);

    rf_recv_mod->recv_value = 0;
    rf_recv_mod->recv_bit_length = 0;
    rf_recv_mod->recv_delay = 0;
    rf_recv_mod->recv_proto = 0;
    rf_recv_mod->separation_limit = SEPARATION_LIMIT;
    rf_recv_mod->recv_tolerance = RECV_TOLERANCE;

    ESP_LOGI(TAG, "RF receiver initialized");
    return ESP_OK;

}

esp_err_t rf_recv_deinit(RFReceiver* rf_recv_mod, bool restore) {
    ESP_RETURN_ON_FALSE(rf_recv_mod && rf_recv_mod->init, ESP_ERR_INVALID_ARG, TAG, "Invalid RF receiver");
    ESP_RETURN_ON_FALSE(rf_recv_mod->rx_gpio != GPIO_NUM_NC, ESP_ERR_INVALID_STATE, TAG, "RF receiver is not active");

    ESP_ERROR_CHECK(gpio_isr_handler_remove(rf_recv_mod->rx_gpio));
    ESP_LOGI(TAG, "ISR handler removed for GPIO %d", rf_recv_mod->rx_gpio);
    ESP_ERROR_CHECK(gpio_reset_pin(rf_recv_mod->rx_gpio));
    rf_recv_mod->rx_gpio = GPIO_NUM_NC;
    if (!restore) rf_recv_mod->rx_gpio_state = GPIO_NUM_NC;

    ESP_LOGI(TAG, "RF receiver deinitialized");
    return ESP_OK;
}

esp_err_t recv_available(RFReceiver* rf_recv_mod) {
    ESP_RETURN_ON_FALSE(rf_recv_mod && rf_recv_mod->init, ESP_ERR_INVALID_ARG, TAG, "Invalid RF module");

    if (rf_recv_mod->rx_gpio == GPIO_NUM_NC)
        return ESP_ERR_INVALID_STATE;

    return (rf_recv_mod->recv_value != 0) ? ESP_OK : ESP_ERR_NOT_FOUND;
}

void reset_recv(RFReceiver* rf_recv_mod) {
    ESP_RETURN_VOID_ON_FALSE(rf_recv_mod && rf_recv_mod->init, TAG, "Invalid RF module");
    ESP_RETURN_VOID_ON_FALSE(rf_recv_mod->rx_gpio != GPIO_NUM_NC, TAG, "RF receiver is not active");

    rf_recv_mod->recv_value = 0;
}

static void decode_recv(RFReceiver* rf_recv_mod, RFRecvData* recv_data) {
    ESP_RETURN_VOID_ON_FALSE(rf_recv_mod && rf_recv_mod->init, TAG, "Invalid RF module");
    ESP_RETURN_VOID_ON_FALSE(rf_recv_mod->rx_gpio != GPIO_NUM_NC, TAG, "RF receiver is not active");

    uint32_t code = rf_recv_mod->recv_value;
    const uint32_t recv_len = rf_recv_mod->recv_bit_length;
    recv_data->original_value = code;
    recv_data->tri_state = NULL;
    recv_data->binary = NULL;

    static char buffer[64];
    uint8_t pos = 0;
    while (code) {
        buffer[32 + pos] = (code & 1) ? '1' : '0';
        code >>= 1;
        pos++;
    }

    for (uint8_t j = 0; j < recv_len; j++) {
        if (j >= recv_len - pos) buffer[j] = buffer[31 - j + recv_len];
        else buffer[j] = '0';
    }
    buffer[recv_len] = '\0';
    recv_data->binary = malloc(recv_len + 1);
    if (recv_data->binary) {
        strcpy(recv_data->binary, buffer);
    } else {
        ESP_LOGE(TAG, "Memory allocation failed for binary data");
        return;
    }

    pos = 0;
    uint8_t pos2 = 0;
    memset(buffer, 0, sizeof(buffer));
    const char* ref = recv_data->binary;
    while (ref[pos] && ref[pos + 1]) {
        if (ref[pos] == '0' && ref[pos + 1] == '0') 
            buffer[pos2] = '0';
        else if (ref[pos] == '1' && ref[pos + 1] == '1')
            buffer[pos2] = '1';
        else if (ref[pos] == '0' && ref[pos + 1] == '1')
            buffer[pos2] = 'F';
        else {
            ESP_LOGE(TAG, "Invalid data: %c%c", ref[pos], ref[pos + 1]);
            return;
        }
        pos += 2;
        pos2++;
    }
    buffer[pos2] = '\0';
    recv_data->tri_state = malloc(pos2 + 1);
    if (recv_data->tri_state) {
        strcpy(recv_data->tri_state, buffer);
    } else {
        ESP_LOGE(TAG, "Memory allocation failed for tri-state data");
        return;
    }

    ESP_LOGI(TAG, "Decode completed");
}

void output_recv(RFReceiver* rf_recv_mod) {
    RFRecvData recv_data;
    decode_recv(rf_recv_mod, &recv_data);
    ESP_LOGI(TAG, "Received data!");
    ESP_LOGI(TAG, "Original value: %lu", recv_data.original_value);
    ESP_LOGI(TAG, "Hexadecimal: 0x%lx", recv_data.original_value);
    ESP_LOGI(TAG, "Binary: %s", recv_data.binary);
    ESP_LOGI(TAG, "Tri-state: %s", recv_data.tri_state);
    ESP_LOGI(TAG, "Pulse length: %lu", rf_recv_mod->recv_delay);
}