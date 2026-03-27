#include "k10_video.h"

#include <esp_heap_caps.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"

#include "arcade_core/galaga_logo.h"

#include "k10_config.h"
#include "k10_hardware.h"
#include "k10_idf.h"

namespace {

constexpr uint8_t kTftMac = 0x48;
constexpr int16_t kLogoTop = (K10_TFT_ACTIVE_HEIGHT - 96) / 2;
constexpr uint16_t kMenuBackground = 0x0000;

spi_device_handle_t g_tft_handle = nullptr;
spi_transaction_t g_transaction = {};
bool g_video_ready = false;
uint16_t* g_frame_buffers[2] = {nullptr, nullptr};
uint8_t g_buffer_index = 0;
bool g_dma_active = false;

void configure_output_gpio(int pin, uint32_t initial_level) {
    gpio_config_t config = {};
    config.pin_bit_mask = 1ULL << pin;
    config.mode = GPIO_MODE_OUTPUT;
    config.pull_up_en = GPIO_PULLUP_DISABLE;
    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&config);
    gpio_set_level(static_cast<gpio_num_t>(pin), initial_level);
}

void write_gpio(int pin, uint32_t level) {
    gpio_set_level(static_cast<gpio_num_t>(pin), level);
}

struct MenuEntry {
    const char* name;
    const uint16_t* logo;
};

MenuEntry g_menu_entries[] = {
    {"Pac-Man", nullptr},
    {"Galaga", galaga_logo},
    {"Donkey Kong", nullptr},
    {"Frogger", nullptr},
    {"Dig Dug", nullptr},
    {"1942", nullptr},
};

constexpr size_t kMenuEntryCount = sizeof(g_menu_entries) / sizeof(g_menu_entries[0]);

spi_device_interface_config_t g_if_cfg = {
    .command_bits = 0,
    .address_bits = 0,
    .dummy_bits = 0,
    .mode = 0,
    .duty_cycle_pos = 128,
    .cs_ena_pretrans = 0,
    .cs_ena_posttrans = 0,
    .clock_speed_hz = K10_TFT_SPICLK,
    .input_delay_ns = 0,
    .spics_io_num = -1,
    .flags = SPI_DEVICE_HALFDUPLEX,
    .queue_size = 1,
    .pre_cb = nullptr,
    .post_cb = nullptr,
};

spi_bus_config_t g_bus_cfg = {
    .mosi_io_num = K10_TFT_MOSI,
    .miso_io_num = K10_TFT_MISO,
    .sclk_io_num = K10_TFT_SCLK,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1,
    .data4_io_num = -1,
    .data5_io_num = -1,
    .data6_io_num = -1,
    .data7_io_num = -1,
    .max_transfer_sz = K10_TFT_WIDTH * 8 * 2,
    .flags = SPICOMMON_BUSFLAG_MASTER,
    .isr_cpu_id = ESP_INTR_CPU_AFFINITY_AUTO,
    .intr_flags = 0,
};

#define W16(a) ((a) >> 8), ((a) & 0xff)

const uint8_t kInitCmds[] = {
    0xEF, 3, 0x03, 0x80, 0x02,
    0xCF, 3, 0x00, 0xC1, 0x30,
    0xED, 4, 0x64, 0x03, 0x12, 0x81,
    0xE8, 3, 0x85, 0x00, 0x78,
    0xCB, 5, 0x39, 0x2C, 0x00, 0x34, 0x02,
    0xF7, 1, 0x20,
    0xEA, 2, 0x00, 0x00,
    0xC0, 1, 0x23,
    0xC1, 1, 0x10,
    0xC5, 2, 0x3e, 0x28,
    0xC7, 1, 0x86,
    0x36, 1, static_cast<uint8_t>(kTftMac ^ 0xc0),
    0x37, 1, 0x00,
    0x3A, 1, 0x55,
    0xB1, 2, 0x00, 0x18,
    0xB6, 3, 0x08, 0x82, 0x27,
    0xF2, 1, 0x00,
    0x26, 1, 0x01,
    0xE0, 15, 0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1, 0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00,
    0xE1, 15, 0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1, 0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F,
    0x11, 0,
    0xff, 150,
    0x29, 0,
    0xff, 150,
    0x00,
};

void write8(uint8_t value) {
    g_transaction.flags = SPI_TRANS_USE_TXDATA;
    g_transaction.length = 8;
    g_transaction.rxlength = 0;
    g_transaction.tx_data[0] = value;
    spi_device_transmit(g_tft_handle, &g_transaction);
}

void write16(uint16_t value) {
    g_transaction.flags = SPI_TRANS_USE_TXDATA;
    g_transaction.length = 16;
    g_transaction.rxlength = 0;
    g_transaction.tx_data[0] = static_cast<uint8_t>((value >> 8) & 0xff);
    g_transaction.tx_data[1] = static_cast<uint8_t>(value & 0xff);
    spi_device_transmit(g_tft_handle, &g_transaction);
}

void write_command(uint8_t command) {
    write_gpio(K10_TFT_DC, 0);
    write8(command);
    write_gpio(K10_TFT_DC, 1);
}

void send_command(uint8_t command, const uint8_t* data, uint8_t len) {
    write_gpio(K10_TFT_CS, 0);
    write_command(command);
    for (uint8_t index = 0; index < len; ++index) {
        write8(data[index]);
    }
    write_gpio(K10_TFT_CS, 1);
}

void set_addr_window(uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
    write_command(0x2A);
    write16(x);
    write16(x + width - 1);

    write_command(0x2B);
    write16(y);
    write16(y + height - 1);

    write_command(0x2C);
}

void flush_dma() {
    if (!g_dma_active) {
        return;
    }

    spi_transaction_t* completed = nullptr;
    spi_device_get_trans_result(g_tft_handle, &completed, portMAX_DELAY);
    g_dma_active = false;
}

uint16_t greyscale(uint16_t input) {
    const uint16_t red = (input >> 3) & 31;
    const uint16_t green = ((input << 3) & 0x38) | ((input >> 13) & 0x07);
    const uint16_t blue = (input >> 8) & 31;
    const uint16_t average = static_cast<uint16_t>((2 * red + green + 2 * blue) / 4);

    return static_cast<uint16_t>(((average << 13) & 0xe000) | ((average << 7) & 0x1f00) |
                                 ((average << 2) & 0x00f8) | ((average >> 3) & 0x0007));
}

int normalize_selection(int selection) {
    if (selection < 1 || selection > static_cast<int>(kMenuEntryCount)) {
        return 1;
    }
    return selection;
}

const MenuEntry& menu_entry_for(int selection) {
    return g_menu_entries[normalize_selection(selection) - 1];
}

uint16_t machine_accent_color(int machine) {
    switch (normalize_selection(machine)) {
        case 1:
            return 0x07e0;
        case 2:
            return 0xf800;
        case 3:
            return 0xfd20;
        case 4:
            return 0x07ff;
        case 5:
            return 0xf81f;
        default:
            return 0xffe0;
    }
}

void render_logo(int16_t row, const uint16_t* logo, bool active, uint16_t* buffer) {
    const uint16_t marker = logo[0];
    const uint16_t* data = logo + 1;

    uint16_t pixel_index = 0;
    const uint16_t pixels_to_draw = static_cast<uint16_t>((row <= 96 - 8) ? (K10_TFT_ACTIVE_WIDTH * 8)
                                                                            : ((96 - row) * K10_TFT_ACTIVE_WIDTH));

    if (row >= 0) {
        uint16_t color = 0;
        uint32_t pixel = 0;
        while (pixel < static_cast<uint32_t>(K10_TFT_ACTIVE_WIDTH * row)) {
            if (data[0] != marker) {
                pixel++;
                data++;
            } else {
                pixel += data[1] + 1;
                color = data[2];
                data += 3;
            }
        }

        if (!active) {
            color = greyscale(color);
        }
        while (pixel_index < ((pixel - K10_TFT_ACTIVE_WIDTH * row < pixels_to_draw)
                                  ? (pixel - K10_TFT_ACTIVE_WIDTH * row)
                                  : pixels_to_draw)) {
            buffer[pixel_index++] = color;
        }
    } else {
        pixel_index = static_cast<uint16_t>(pixel_index - row * K10_TFT_ACTIVE_WIDTH);
    }

    while (pixel_index < pixels_to_draw) {
        if (data[0] != marker) {
            buffer[pixel_index++] = active ? *data++ : greyscale(*data++);
        } else {
            uint16_t color = data[2];
            if (!active) {
                color = greyscale(color);
            }
            for (uint16_t count = 0; count < data[1] + 1 && pixel_index < pixels_to_draw; ++count) {
                buffer[pixel_index++] = color;
            }
            data += 3;
        }
    }
}

void fill_row(uint16_t* buffer, uint16_t color) {
    for (uint32_t index = 0; index < K10_TFT_ACTIVE_WIDTH * 8; ++index) {
        buffer[index] = color;
    }
}

void render_menu_row(uint16_t* buffer, uint16_t tile_row, int selection) {
    memset(buffer, 0, K10_TFT_ACTIVE_WIDTH * 8 * sizeof(uint16_t));

    const int normalized_selection = normalize_selection(selection);
    const int offset = 96 * ((normalized_selection + static_cast<int>(kMenuEntryCount) - 2) % static_cast<int>(kMenuEntryCount));
    int logo_index = ((static_cast<int>(tile_row) + offset / 8) / 12) % static_cast<int>(kMenuEntryCount);
    if (logo_index < 0) {
        logo_index += static_cast<int>(kMenuEntryCount);
    }

    int logo_y = (static_cast<int>(tile_row) * 8 + offset) % 96;
    if (g_menu_entries[logo_index].logo != nullptr) {
        render_logo(static_cast<int16_t>(logo_y), g_menu_entries[logo_index].logo,
                    normalized_selection == logo_index + 1, buffer);
    }

    if (logo_y > (96 - 8)) {
        logo_index = (logo_index + 1) % static_cast<int>(kMenuEntryCount);
        logo_y -= 96;
        if (g_menu_entries[logo_index].logo != nullptr) {
            render_logo(static_cast<int16_t>(logo_y), g_menu_entries[logo_index].logo,
                        normalized_selection == logo_index + 1, buffer);
        }
    }
}

void render_machine_row(uint16_t* buffer, uint16_t tile_row, int machine) {
    fill_row(buffer, kMenuBackground);

    const uint16_t accent = machine_accent_color(machine);
    if (tile_row < 2 || tile_row >= (K10_TFT_ACTIVE_HEIGHT / 8) - 2) {
        fill_row(buffer, accent);
        return;
    }

    const uint32_t middle_start = (K10_TFT_ACTIVE_WIDTH / 2) - 48;
    const uint32_t middle_end = (K10_TFT_ACTIVE_WIDTH / 2) + 48;
    if (tile_row == 4 || tile_row == (K10_TFT_ACTIVE_HEIGHT / 8) - 5) {
        for (uint32_t index = 0; index < K10_TFT_ACTIVE_WIDTH * 8; ++index) {
            const uint32_t x = index % K10_TFT_ACTIVE_WIDTH;
            if (x >= middle_start && x < middle_end) {
                buffer[index] = accent;
            }
        }
    }

    const int16_t logo_row = static_cast<int16_t>(tile_row * 8) - kLogoTop;
    const uint16_t* logo = menu_entry_for(machine).logo;
    if (logo != nullptr && logo_row > -8 && logo_row < 96) {
        render_logo(logo_row, logo, true, buffer);
    }
}

}  // namespace

bool k10_video_begin() {
    if (g_video_ready) {
        return true;
    }

    configure_output_gpio(K10_TFT_CS, 1);
    configure_output_gpio(K10_TFT_DC, 1);
    configure_output_gpio(40, 1);

    esp_err_t result = spi_bus_initialize(SPI3_HOST, &g_bus_cfg, SPI_DMA_CH_AUTO);
    if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) {
        printf("Video: SPI bus init failed: 0x%x\n", result);
        return false;
    }

    result = spi_bus_add_device(SPI3_HOST, &g_if_cfg, &g_tft_handle);
    if (result == ESP_ERR_INVALID_STATE && g_tft_handle != nullptr) {
        result = ESP_OK;
    }
    if (result != ESP_OK) {
        printf("Video: SPI device add failed: 0x%x\n", result);
        return false;
    }

    send_command(0x01, nullptr, 0);
    k10_delay_ms(150);

    const uint8_t* cursor = kInitCmds;
    while (*cursor != 0x00) {
        const uint8_t command = *cursor++;
        const uint8_t len = *cursor++;
        if (command == 0xff) {
            k10_delay_ms(len);
            continue;
        }
        send_command(command, cursor, len);
        cursor += len;
    }

    if (!k10_prepare_expander()) {
        printf("Video: expander prepare failed\n");
    }

    g_frame_buffers[0] = static_cast<uint16_t*>(heap_caps_malloc(K10_TFT_ACTIVE_WIDTH * 8 * sizeof(uint16_t), MALLOC_CAP_DMA));
    g_frame_buffers[1] = static_cast<uint16_t*>(heap_caps_malloc(K10_TFT_ACTIVE_WIDTH * 8 * sizeof(uint16_t), MALLOC_CAP_DMA));
    if (g_frame_buffers[0] == nullptr || g_frame_buffers[1] == nullptr) {
        printf("Video: frame buffer allocation failed\n");
        return false;
    }

    g_video_ready = true;
    printf("Video: ILI9341 initialized\n");
    return true;
}

void k10_video_begin_frame() {
    if (!g_video_ready && !k10_video_begin()) {
        return;
    }

    write_gpio(K10_TFT_CS, 0);
    set_addr_window(K10_TFT_X_OFFSET, K10_TFT_Y_OFFSET, K10_TFT_ACTIVE_WIDTH, K10_TFT_ACTIVE_HEIGHT);
}

void k10_video_write(const uint16_t* colors, uint32_t len) {
    if (!g_video_ready) {
        return;
    }

    flush_dma();

    g_transaction.flags = 0;
    g_transaction.length = len * 16;
    g_transaction.rxlength = 0;
    g_transaction.tx_buffer = colors;
    spi_device_queue_trans(g_tft_handle, &g_transaction, portMAX_DELAY);
    g_dma_active = true;
}

void k10_video_end_frame() {
    if (!g_video_ready) {
        return;
    }

    flush_dma();
    write_gpio(K10_TFT_CS, 1);
}

void k10_video_draw_menu_frame(int selection) {
    if (!g_video_ready && !k10_video_begin()) {
        return;
    }

    k10_video_begin_frame();

    for (uint16_t tile_row = 0; tile_row < (K10_TFT_ACTIVE_HEIGHT / 8); ++tile_row) {
        uint16_t* buffer = g_frame_buffers[g_buffer_index];
        render_menu_row(buffer, tile_row, selection);
        k10_video_write(buffer, K10_TFT_ACTIVE_WIDTH * 8);
        g_buffer_index = static_cast<uint8_t>(1 - g_buffer_index);
    }

    k10_video_end_frame();
}

void k10_video_draw_machine_frame(int machine) {
    if (!g_video_ready && !k10_video_begin()) {
        return;
    }

    k10_video_begin_frame();

    for (uint16_t tile_row = 0; tile_row < (K10_TFT_ACTIVE_HEIGHT / 8); ++tile_row) {
        uint16_t* buffer = g_frame_buffers[g_buffer_index];
        render_machine_row(buffer, tile_row, machine);
        k10_video_write(buffer, K10_TFT_ACTIVE_WIDTH * 8);
        g_buffer_index = static_cast<uint8_t>(1 - g_buffer_index);
    }

    k10_video_end_frame();
}

int k10_video_wrap_menu_selection(int selection, int delta) {
    const int count = static_cast<int>(kMenuEntryCount);
    int wrapped = normalize_selection(selection) + delta;
    while (wrapped < 1) {
        wrapped += count;
    }
    while (wrapped > count) {
        wrapped -= count;
    }
    return wrapped;
}

const char* k10_video_menu_name(int selection) {
    return menu_entry_for(selection).name;
}