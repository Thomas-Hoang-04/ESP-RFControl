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

static const uint32_t ref_val = 5592323;

enum {
    RC_SWITCH_1_PULSE_LEN = 350,
    COM_PULSE_LEN = 320,
    FAST_PULSE_LEN = 240,
    FLASH_PULSE_LEN = 150,
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
    uint32_t original_value;
    char* tri_state;
    char* binary;
} RFRecvData;

typedef struct {
    // Reception configuration
    uint32_t recv_value, recv_bit_length, recv_delay, separation_limit;
    uint8_t recv_proto, recv_tolerance;
    uint32_t recv_timings[MAX_EDGES];

    // GPIO configuration
    bool init, timer_running;
    gpio_num_t rx_gpio, rx_gpio_state;
    gpio_num_t signal_status_gpio, motor_status_gpio;
    gpio_num_t motor_gpio, reset_gpio;
    volatile uint8_t motor_status;

    TaskHandle_t rf_recv_handle;
    gptimer_handle_t timer;
} RFReceiver;

esp_err_t rf_init(gpio_num_t signal_status_gpio, gpio_num_t motor_gpio, gpio_num_t motor_status_gpio,
    gpio_num_t reset_gpio, RFReceiver* rf_recv_mod);

esp_err_t rf_deinit(RFReceiver* rf_recv_mod);

esp_err_t rf_timer_init(RFReceiver* rf_recv_mod);

esp_err_t rf_timer_deinit(gptimer_handle_t timer);

esp_err_t rf_recv_init(gpio_num_t rx_gpio, RFReceiver* rf_recv_mod);

esp_err_t rf_recv_deinit(RFReceiver* rf_recv_mod, bool restore);

esp_err_t recv_available(RFReceiver* rf_recv_mod);

void reset_recv(RFReceiver* rf_recv_mod);

void output_recv(RFReceiver* rf_recv_mod);

void led_flash(RFReceiver* rf_recv_mod);

esp_err_t set_reset_isr(gpio_num_t reset_gpio, RFReceiver* rf_recv_mod);

#endif // RF_TEST_H