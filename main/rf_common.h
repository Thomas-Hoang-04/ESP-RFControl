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
#define MAX_EDGES 67
#define RECV_TOLERANCE 60
#define SEPARATION_LIMIT 4300
#define PROTO_COUNT 12

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
    bool inverted;
} Protocol;

static const DRAM_ATTR Protocol proto[] = {
    { FAST_PULSE_LEN, {  1, 31 }, {  1,  3 }, {  3,  1 }, false },    // protocol 1
    { 650, {  1, 10 }, {  1,  2 }, {  2,  1 }, false },    // protocol 2
    { 100, { 30, 71 }, {  4, 11 }, {  9,  6 }, false },    // protocol 3
    { 380, {  1,  6 }, {  1,  3 }, {  3,  1 }, false },    // protocol 4
    { 500, {  6, 14 }, {  1,  2 }, {  2,  1 }, false },    // protocol 5
    { 450, { 23,  1 }, {  1,  2 }, {  2,  1 }, true },     // protocol 6 (HT6P20B)
    { 150, {  2, 62 }, {  1,  6 }, {  6,  1 }, false },    // protocol 7 (HS2303-PT, i. e. used in AUKEY Remote)
    { 200, {  3, 130}, {  7, 16 }, {  3,  16}, false },     // protocol 8 Conrad RS-200 RX
    { 200, { 130, 7 }, {  16, 7 }, { 16,  3 }, true },      // protocol 9 Conrad RS-200 TX
    { 365, { 18,  1 }, {  3,  1 }, {  1,  3 }, true },     // protocol 10 (1ByOne Doorbell)
    { 270, { 36,  1 }, {  1,  2 }, {  2,  1 }, true },     // protocol 11 (HT12E)
    { 320, { 36,  1 }, {  1,  2 }, {  2,  1 }, true }      // protocol 12 (SM5212)
};

typedef struct {
    uint8_t level;
    uint32_t pulse_length;
} RFPulse;

typedef struct {
    // Pulse data for transmission
    RFPulse* pulses;
    size_t pulse_count;
    uint8_t pulse_index;

    // Transmission state
    uint8_t current_rep;
    uint8_t repeat_count;

    // Reception configuration
    uint32_t recv_value, recv_bit_length, recv_delay, separation_limit;
    uint8_t recv_proto, recv_tolerance;
    uint32_t recv_timings[MAX_EDGES];

    // GPIO configuration
    bool tx_active, init;
    gpio_num_t tx_gpio;
    gpio_num_t rx_gpio, rx_gpio_state;

    Protocol* proto;
    TaskHandle_t rf_trans_handle;
    gptimer_handle_t timer;
} RFTransmitter;

esp_err_t rf_init(gpio_num_t tx_gpio, int8_t repeat_count, Protocol* tx_proto, RFTransmitter* rf_rmt);

esp_err_t rf_deinit(RFTransmitter* rf_rmt);

esp_err_t rf_send(RFTransmitter* rf_rmt);

esp_err_t rf_timer_init(RFTransmitter* rf_rmt);

esp_err_t rf_timer_deinit(gptimer_handle_t timer);

esp_err_t rf_timer_reset(RFTransmitter* rf_rmt);

esp_err_t rf_recv_init(gpio_num_t rx_gpio, RFTransmitter* rf_rmt);

esp_err_t rf_recv_deinit(RFTransmitter* rf_rmt);

esp_err_t translate_tristate(const char* data, RFTransmitter* rf_rmt);

bool recv_available(RFTransmitter* rf_rmt);

void reset_recv(RFTransmitter* rf_rmt);

#endif