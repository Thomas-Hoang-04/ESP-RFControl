#include <string.h>
#include "rf_common.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"

static inline uint32_t diff(uint32_t a, uint32_t b) {
    return (a > b) ? (a - b) : (b - a);
}

static bool IRAM_ATTR recv_proto(RFTransmitter* rf_rmt, uint8_t proto_idx, uint32_t edge_count) {
    const Protocol curr_proto = proto[proto_idx];

    uint32_t code = 0;
    const uint32_t sync_len_in_pulses = curr_proto.sync_factor.low > curr_proto.sync_factor.high
        ? curr_proto.sync_factor.low : curr_proto.sync_factor.high;
    const uint32_t delay = rf_rmt->recv_timings[0] / sync_len_in_pulses;
    const uint32_t delay_tolerance = delay * rf_rmt->recv_tolerance / 100;

    const uint8_t first_data_timing = curr_proto.inverted ? 2 : 1;
    for (uint8_t i = first_data_timing; i < edge_count - 1; i += 2) {
        code <<= 1;
        if (diff(rf_rmt->recv_timings[i], delay * curr_proto.zero.high) < delay_tolerance &&
                diff(rf_rmt->recv_timings[i + 1], delay * curr_proto.zero.low) < delay_tolerance) {
            code |= 0;
        } else if (diff(rf_rmt->recv_timings[i], delay * curr_proto.one.high) < delay_tolerance &&
                diff(rf_rmt->recv_timings[i + 1], delay * curr_proto.one.low) < delay_tolerance) {
            code |= 1;
        } else {
            return false;
        }
    }

    // Ignore very short transmissions (Presumably noise)
    if (edge_count > 7) {
        rf_rmt->recv_value = code;
        rf_rmt->recv_bit_length = (edge_count - 1) / 2;
        rf_rmt->recv_delay = delay;
        rf_rmt->recv_proto = proto_idx;
        return true;
    }

    return false;
}

static void IRAM_ATTR rf_recv_isr_handler(void* arg) {
    RFTransmitter* rf_rmt = (RFTransmitter*)arg;

    static uint32_t edge_count = 0;
    static uint64_t last_time = 0;
    static uint32_t repeat_count = 0;

    const int64_t time = esp_timer_get_time();
    const uint32_t duration = (uint32_t)(time - last_time);

    if (duration > rf_rmt->separation_limit) {
        // Long stretch without signal level change -> presumably the gap between two transmissions
        if ((repeat_count == 0) || (diff(duration, rf_rmt->recv_timings[0]) < 200)) {
            /* Assuming the sender sending the signal multiple times with roughly the same gap period, 
            this long signal is close in length to the signal which start the previous records
            -> potentially confirming it being a gap between two transmissions */
            repeat_count++;
            if (repeat_count == 2) {
                for (uint8_t i = 0; i < PROTO_COUNT; i++)
                    if (recv_proto(rf_rmt, i, edge_count)) break;
                repeat_count = 0;
            }
        }

        edge_count = 0;
    }

    if (edge_count >= MAX_EDGES) {
        edge_count = 0;
        repeat_count = 0;
    }

    rf_rmt->recv_timings[edge_count++] = duration;
    last_time = (uint64_t)time;
}

esp_err_t rf_recv_init(gpio_num_t rx_gpio, RFTransmitter* rf_rmt) {
    ESP_RETURN_ON_FALSE(rf_rmt && rf_rmt->init, ESP_ERR_INVALID_ARG, TAG, "Invalid RF receiver");

    rf_rmt->rx_gpio = rx_gpio;
    ESP_LOGI(TAG, "GPIO %d configured for RF reception", rx_gpio);

    gpio_config_t io_conf_rx = {
        .pin_bit_mask = (1ULL << rx_gpio),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf_rx));
    ESP_LOGI(TAG, "ISR service installed");
    ESP_ERROR_CHECK(gpio_isr_handler_add(rx_gpio, rf_recv_isr_handler, rf_rmt));
    ESP_LOGI(TAG, "ISR handler added for GPIO %d", rx_gpio);

    rf_rmt->recv_value = 0;
    rf_rmt->recv_bit_length = 0;
    rf_rmt->recv_delay = 0;
    rf_rmt->recv_proto = 0;
    rf_rmt->separation_limit = SEPARATION_LIMIT;
    rf_rmt->recv_tolerance = RECV_TOLERANCE;

    ESP_LOGI(TAG, "RF receiver initialized");
    return ESP_OK;

}

esp_err_t rf_recv_deinit(RFTransmitter* rf_rmt, bool restore) {
    ESP_RETURN_ON_FALSE(rf_rmt && rf_rmt->init, ESP_ERR_INVALID_ARG, TAG, "Invalid RF receiver");
    ESP_RETURN_ON_FALSE(rf_rmt->rx_gpio != GPIO_NUM_NC, ESP_ERR_INVALID_STATE, TAG, "RF receiver is not active");

    ESP_ERROR_CHECK(gpio_isr_handler_remove(rf_rmt->rx_gpio));
    ESP_LOGI(TAG, "ISR handler removed for GPIO %d", rf_rmt->rx_gpio);
    ESP_ERROR_CHECK(gpio_reset_pin(rf_rmt->rx_gpio));
    rf_rmt->rx_gpio = GPIO_NUM_NC;
    if (!restore) rf_rmt->rx_gpio_state = GPIO_NUM_NC;

    ESP_LOGI(TAG, "RF receiver deinitialized");
    return ESP_OK;
}

esp_err_t recv_available(RFTransmitter* rf_rmt) {
    ESP_RETURN_ON_FALSE(rf_rmt && rf_rmt->init, ESP_ERR_INVALID_ARG, TAG, "Invalid RF module");

    if (rf_rmt->rx_gpio == GPIO_NUM_NC)
        return ESP_ERR_INVALID_STATE;

    return (rf_rmt->recv_value != 0) ? ESP_OK : ESP_ERR_NOT_FOUND;
}

void reset_recv(RFTransmitter* rf_rmt) {
    ESP_RETURN_VOID_ON_FALSE(rf_rmt && rf_rmt->init, TAG, "Invalid RF module");
    ESP_RETURN_VOID_ON_FALSE(rf_rmt->rx_gpio != GPIO_NUM_NC, TAG, "RF receiver is not active");

    rf_rmt->recv_value = 0;
}

static void decode_recv(RFTransmitter* rf_rmt, RFRecvData* recv_data) {
    ESP_RETURN_VOID_ON_FALSE(rf_rmt && rf_rmt->init, TAG, "Invalid RF module");
    ESP_RETURN_VOID_ON_FALSE(rf_rmt->rx_gpio != GPIO_NUM_NC, TAG, "RF receiver is not active");

    uint32_t code = rf_rmt->recv_value;
    const uint32_t recv_len = rf_rmt->recv_bit_length;
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

void output_recv(RFTransmitter* rf_rmt) {
    RFRecvData recv_data;
    decode_recv(rf_rmt, &recv_data);
    ESP_LOGI(TAG, "Received data!");
    ESP_LOGI(TAG, "Original value: %lu", recv_data.original_value);
    ESP_LOGI(TAG, "Hexadecimal: 0x%lx", recv_data.original_value);
    ESP_LOGI(TAG, "Binary: %s", recv_data.binary);
    ESP_LOGI(TAG, "Tri-state: %s", recv_data.tri_state);
    ESP_LOGI(TAG, "Pulse length: %lu", rf_rmt->recv_delay);
}