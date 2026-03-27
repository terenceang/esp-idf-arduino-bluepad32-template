#include "k10_hardware.h"

#include "driver/i2s.h"
#include "driver/i2c.h"

#include "k10_idf.h"
#include "k10_bt_gamepad.h"
#include "k10_config.h"
#include "k10_input.h"

#ifndef I2S_MCLK_MULTIPLE_DEFAULT
#define I2S_MCLK_MULTIPLE_DEFAULT I2S_MCLK_MULTIPLE_256
#endif

namespace {

constexpr i2c_port_t kI2CPort = I2C_NUM_0;
constexpr uint32_t kI2CFrequencyHz = 400000;
constexpr TickType_t kI2CTimeoutTicks = pdMS_TO_TICKS(50);

bool g_i2c_ready = false;
bool g_i2s_ready = false;
uint8_t g_expander_output_port1 = K10_EXPANDER_OUTPUT_PORT1;

bool ensure_i2c_ready() {
    if (g_i2c_ready) {
        return true;
    }

    i2c_config_t config = {};
    config.mode = I2C_MODE_MASTER;
    config.sda_io_num = static_cast<gpio_num_t>(K10_I2C_SDA);
    config.scl_io_num = static_cast<gpio_num_t>(K10_I2C_SCL);
    config.sda_pullup_en = GPIO_PULLUP_ENABLE;
    config.scl_pullup_en = GPIO_PULLUP_ENABLE;
    config.master.clk_speed = kI2CFrequencyHz;

    if (i2c_param_config(kI2CPort, &config) != ESP_OK) {
        return false;
    }

    const esp_err_t result = i2c_driver_install(kI2CPort, config.mode, 0, 0, 0);
    if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) {
        return false;
    }

    g_i2c_ready = true;
    return true;
}

bool write_register(uint8_t address, uint8_t reg, uint8_t value) {
    if (!ensure_i2c_ready()) {
        return false;
    }

    const uint8_t data[] = {reg, value};
    return i2c_master_write_to_device(kI2CPort, address, data, sizeof(data), kI2CTimeoutTicks) == ESP_OK;
}

bool read_registers(uint8_t address, uint8_t reg, uint8_t* buffer, size_t length) {
    if (buffer == nullptr || length == 0) {
        return false;
    }

    if (!ensure_i2c_ready()) {
        return false;
    }

    return i2c_master_write_read_device(kI2CPort, address, &reg, sizeof(reg), buffer, length, kI2CTimeoutTicks) ==
           ESP_OK;
}

bool read_expander_inputs(uint8_t* port0, uint8_t* port1) {
    uint8_t ports[2] = {0, 0};
    if (!read_registers(K10_EXPANDER_ADDR, XL9535_INPUT_REG_0, ports, sizeof(ports))) {
        return false;
    }

    *port0 = ports[0];
    *port1 = ports[1];
    return true;
}

bool write_expander_port1(uint8_t value) {
    if (!write_register(K10_EXPANDER_ADDR, XL9535_OUTPUT_REG_1, value)) {
        return false;
    }

    g_expander_output_port1 = value;
    return true;
}

bool set_user_led(bool enabled) {
    if (!k10_prepare_expander()) {
        return false;
    }

    uint8_t next_value = g_expander_output_port1;
    if (enabled) {
        next_value |= K10_MASK_USER_LED;
    } else {
        next_value &= static_cast<uint8_t>(~K10_MASK_USER_LED);
    }

    return write_expander_port1(next_value);
}

}  // namespace

bool k10_prepare_expander() {
    static bool expander_prepared = false;

    if (expander_prepared) {
        return true;
    }

    const bool port0_configured = write_register(K10_EXPANDER_ADDR, XL9535_CONFIG_REG_0, K10_EXPANDER_CONFIG_PORT0);
    const bool port1_configured = write_register(K10_EXPANDER_ADDR, XL9535_CONFIG_REG_1, K10_EXPANDER_CONFIG_PORT1);
    const bool port0_output_set = write_register(K10_EXPANDER_ADDR, XL9535_OUTPUT_REG_0, K10_EXPANDER_OUTPUT_PORT0);
    const bool port1_output_set = write_expander_port1(K10_EXPANDER_OUTPUT_PORT1);

    expander_prepared = port0_configured && port1_configured && port0_output_set && port1_output_set;
    return expander_prepared;
}

void k10_audio_init() {
    if (g_i2s_ready) {
        return;
    }

    static const i2s_config_t config = {
        .mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = K10_GALAGA_AUDIO_SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL2,
        .dma_buf_count = 4,
        .dma_buf_len = 64,
        .use_apll = false,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0,
        .mclk_multiple = I2S_MCLK_MULTIPLE_DEFAULT,
        .bits_per_chan = I2S_BITS_PER_CHAN_16BIT,
    };

    if (i2s_driver_install(I2S_NUM_0, &config, 0, nullptr) != ESP_OK) {
        printf("Audio: I2S driver install failed\n");
        return;
    }

    static const i2s_pin_config_t pin_config = {
        .mck_io_num = K10_I2S_MCLK,
        .bck_io_num = K10_I2S_BCK,
        .ws_io_num = K10_I2S_WS,
        .data_out_num = K10_I2S_DOUT,
        .data_in_num = -1,
    };

    if (i2s_set_pin(I2S_NUM_0, &pin_config) != ESP_OK) {
        printf("Audio: I2S set pin failed\n");
        return;
    }

    printf("Audio: I2S pins configured (BCK:%d, WS:%d, DO:%d, MCLK:%d)\n", K10_I2S_BCK, K10_I2S_WS,
           K10_I2S_DOUT, K10_I2S_MCLK);
    i2s_zero_dma_buffer(I2S_NUM_0);
    g_i2s_ready = true;
}

void k10_audio_set_dkong_rate(bool is_dkong) {
    i2s_set_sample_rates(I2S_NUM_0, is_dkong ? K10_DKONG_AUDIO_SAMPLE_RATE : K10_GALAGA_AUDIO_SAMPLE_RATE);
}

void k10_hw_init() {
    k10_prepare_expander();

    k10_audio_init();
    k10_bt_gamepad_setup();
}

uint8_t k10_read_inputs() {
    uint8_t input_states = k10_bt_gamepad_read();

#if !K10_DISABLE_ONBOARD_BUTTONS
    uint8_t port0 = 0;
    uint8_t port1 = 0;

    if (read_expander_inputs(&port0, &port1)) {
        const bool key_a_pressed = (port1 & K10_MASK_KEY_A) == 0;
        const bool key_b_pressed = (port0 & K10_MASK_KEY_B) == 0;

        if (key_a_pressed && key_b_pressed) {
            input_states |= K10_BUTTON_START;
        } else {
            if (key_a_pressed) {
                input_states |= K10_BUTTON_FIRE;
            }
            if (key_b_pressed) {
                input_states |= K10_BUTTON_COIN;
            }
        }
    }
#endif

    return input_states;
}