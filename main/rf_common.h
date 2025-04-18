#ifndef RF_TEST_H
#define RF_TEST_H

#include <stdbool.h>
#include <inttypes.h>
#include "esp_err.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gptimer.h"

#define TAG "RF_TEST"

#define DEFAULT_RESOLUTION 1000000

enum {
    RC_SWITCH_1_PULSE_LEN = 350,
    COM_PULSE_LEN = 320,
    FAST_PULSE_LEN = 240,
    FLASH_PULSE_LEN = 150,
};

enum {
    RC_SWITCH_REPEAT_COUNT = 10,
    OPT_REPEAT_COUNT = 5,
    FAST_REPEAT_COUNT = 4,
};

typedef struct {
    uint8_t high;
    uint8_t low;
} RFTicks;

typedef struct {
    uint16_t pulse_length;
    RFTicks sync_factor;
    RFTicks zero;
    RFTicks one;
} Protocol;

typedef struct {
    uint8_t level;
    uint32_t pulse_length;
} RFPulse;

typedef struct {
    RFPulse* pulses;
    size_t pulse_count;
    uint8_t pulse_index;
    bool active, init;
    gpio_num_t tx_gpio;
    uint8_t current_rep;
    uint8_t repeat_count;
    Protocol* proto;
    TaskHandle_t rf_trans_handle;
    gptimer_handle_t timer;
} RFTransmitter;

esp_err_t rf_init(gpio_num_t tx_gpio, int8_t repeat_count, Protocol* proto, RFTransmitter* rf_rmt);

esp_err_t rf_deinit(RFTransmitter* rf_rmt);

esp_err_t rf_send(RFTransmitter* rf_rmt);

esp_err_t rf_timer_init(RFTransmitter* rf_rmt);

esp_err_t rf_timer_deinit(gptimer_handle_t timer);

esp_err_t rf_timer_reset(RFTransmitter* rf_rmt);

esp_err_t translate_tristate(const char* data, RFTransmitter* rf_rmt);

#endif