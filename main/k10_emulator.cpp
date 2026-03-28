#include "k10_emulator.h"

#include <esp_heap_caps.h>
#include <esp_system.h>
#include <stdlib.h>
#include <string.h>

#include "driver/i2s.h"
#include "freertos/idf_additions.h"

#include "k10_hardware.h"
#include "k10_idf.h"
#include "k10_video.h"

extern "C" {
#include "arcade_core/config.h"

struct Z80;

void prepare_emulation(void);
void emulate_frame(void);
extern unsigned char* memory;
extern char game_started;
extern unsigned char soundregs[32];
extern unsigned char starcontrol;
}

struct sprite_S {
    unsigned char code;
    unsigned char color;
    unsigned char flags;
    short x;
    short y;
};

void leds_state_reset() {}
void leds_check_galaga_sprite(struct sprite_S* spr) {
    (void)spr;
}

unsigned char active_sprites = 0;
struct sprite_S* sprite = nullptr;
unsigned short* frame_buffer = nullptr;

#define IO_EMULATION
#include "arcade_core/tileaddr.h"
#include "arcade_core/galaga.h"

namespace {

constexpr uint32_t kFramePixels = 224 * 8;
constexpr uint32_t kFrameBytes = kFramePixels * sizeof(uint16_t);
constexpr uint64_t kFramePeriodUs = 16667;
constexpr uint32_t kEmulationTaskStackWords = 4096;
constexpr UBaseType_t kEmulationTaskPriority = 2;
constexpr BaseType_t kEmulationTaskCore = 0;

TaskHandle_t g_emulation_task = nullptr;
TaskHandle_t g_present_task = nullptr;
uint16_t* g_frame_buffers[2] = {nullptr, nullptr};
uint8_t g_buffer_index = 0;
bool g_runtime_ready = false;
bool g_runtime_running = false;
uint8_t g_cached_buttons = 0;

unsigned short snd_boom_cnt = 0;
const signed char* snd_boom_ptr = nullptr;

unsigned long snd_cnt[3] = {0, 0, 0};
unsigned long snd_freq[3] = {0, 0, 0};
const signed char* snd_wave[3] = {nullptr, nullptr, nullptr};
unsigned char snd_volume[3] = {0, 0, 0};

int16_t snd_buffer[128];

int16_t clamp_pcm16(int sample) {
    if (sample > 32767) {
        return 32767;
    }
    if (sample < -32768) {
        return -32768;
    }
    return static_cast<int16_t>(sample);
}

void render_line(short row) {
    memset(frame_buffer, 0, kFrameBytes);
    galaga_render_row(row);
}

void audio_namco_waveregs_parse_cpp() {
    for (char channel = 0; channel < 3; ++channel) {
        snd_volume[channel] = soundregs[channel * 5 + 0x15];

        if (!snd_volume[channel]) {
            continue;
        }

        snd_freq[channel] = (channel == 0) ? soundregs[0x10] : 0;
        snd_freq[channel] += soundregs[channel * 5 + 0x11] << 4;
        snd_freq[channel] += soundregs[channel * 5 + 0x12] << 8;
        snd_freq[channel] += soundregs[channel * 5 + 0x13] << 12;
        snd_freq[channel] += soundregs[channel * 5 + 0x14] << 16;
        snd_wave[channel] = galaga_wavetable[soundregs[channel * 5 + 0x05] & 0x07];
    }
}

void snd_render_buffer_cpp() {
    for (int index = 0; index < 64; ++index) {
        short value = 0;

        if (snd_volume[0]) {
            value += snd_volume[0] * snd_wave[0][(snd_cnt[0] >> 13) & 0x1f];
        }
        if (snd_volume[1]) {
            value += snd_volume[1] * snd_wave[1][(snd_cnt[1] >> 13) & 0x1f];
        }
        if (snd_volume[2]) {
            value += snd_volume[2] * snd_wave[2][(snd_cnt[2] >> 13) & 0x1f];
        }

        if (snd_boom_cnt) {
            value += *snd_boom_ptr;
            if (snd_boom_cnt & 1) {
                snd_boom_ptr++;
            }
            snd_boom_cnt--;
        }

        value = value * (64 * AUDIO_VOLUME / 100);

        const int16_t sample = clamp_pcm16(value);
        snd_buffer[2 * index] = sample;
        snd_buffer[2 * index + 1] = sample;

        snd_cnt[0] += snd_freq[0];
        snd_cnt[1] += snd_freq[1];
        snd_cnt[2] += snd_freq[2];
    }
}

void snd_transmit_cpp() {
    size_t bytes_out = 0;
    do {
        i2s_write(I2S_NUM_0, snd_buffer, sizeof(snd_buffer), &bytes_out, 0);

        if (bytes_out) {
            audio_namco_waveregs_parse_cpp();
            snd_render_buffer_cpp();
        }
    } while (bytes_out);
}

void update_screen_cpp() {
    static const signed char star_speeds[8] = {-1, -2, -3, 0, 3, 2, 1, 0};
    static uint32_t frame_count = 0;
    static uint32_t last_fps_time = 0;
    static uint64_t next_frame_deadline_us = 0;

    const uint32_t frame_start = static_cast<uint32_t>(k10_micros());
    frame_count++;

    if (g_emulation_task != nullptr) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    }

    const uint32_t now = k10_millis();
    if (now - last_fps_time >= 5000) {
        const float fps = last_fps_time == 0 ? 0.0f : (frame_count * 1000.0f) / (now - last_fps_time);
        printf("FPS: %.2f, Free Heap: %u\n", fps, esp_get_free_heap_size());
        frame_count = 0;
        last_fps_time = now;
    }

    galaga_prepare_frame();

    k10_video_begin_frame();
    for (int tile_row = 0; tile_row < 36; ++tile_row) {
        frame_buffer = g_frame_buffers[g_buffer_index];
        render_line(static_cast<short>(tile_row));
        k10_video_write(frame_buffer, kFramePixels);
        g_buffer_index = static_cast<uint8_t>(1 - g_buffer_index);
    }
    k10_video_end_frame();

    snd_transmit_cpp();

    if (g_emulation_task != nullptr) {
        xTaskNotifyGive(g_emulation_task);
    }

    const uint64_t now_us = k10_micros();
    if (next_frame_deadline_us == 0 || now_us > next_frame_deadline_us + kFramePeriodUs) {
        next_frame_deadline_us = now_us + kFramePeriodUs;
    } else {
        while (true) {
            const uint64_t current_us = k10_micros();
            if (current_us >= next_frame_deadline_us) {
                break;
            }

            const uint64_t remaining_us = next_frame_deadline_us - current_us;
            if (remaining_us > 2000) {
                vTaskDelay(pdMS_TO_TICKS((remaining_us - 1000) / 1000));
            } else {
                taskYIELD();
            }
        }
        next_frame_deadline_us += kFramePeriodUs;
    }

    stars_scroll_y += 2 * star_speeds[starcontrol & 7];
}

void emulation_task(void* parameter) {
    (void)parameter;
    while (true) {
        emulate_frame();
        if (g_present_task != nullptr) {
            xTaskNotifyGive(g_present_task);
        }
    }
}

bool ensure_runtime_allocations() {
    if (g_frame_buffers[0] == nullptr) {
        g_frame_buffers[0] = static_cast<uint16_t*>(heap_caps_malloc(kFrameBytes, MALLOC_CAP_DMA));
    }
    if (g_frame_buffers[1] == nullptr) {
        g_frame_buffers[1] = static_cast<uint16_t*>(heap_caps_malloc(kFrameBytes, MALLOC_CAP_DMA));
    }
    if (sprite == nullptr) {
        sprite = static_cast<sprite_S*>(malloc(128 * sizeof(sprite_S)));
    }

    g_runtime_ready = g_frame_buffers[0] != nullptr && g_frame_buffers[1] != nullptr && sprite != nullptr;
    return g_runtime_ready;
}

void reset_audio_state() {
    memset(snd_cnt, 0, sizeof(snd_cnt));
    memset(snd_freq, 0, sizeof(snd_freq));
    memset(snd_volume, 0, sizeof(snd_volume));
    memset(snd_buffer, 0, sizeof(snd_buffer));
    snd_boom_cnt = 0;
    snd_boom_ptr = nullptr;
}

void teardown_emulation_task() {
    if (g_emulation_task != nullptr) {
        vTaskDelete(g_emulation_task);
        g_emulation_task = nullptr;
    }

    g_present_task = nullptr;

    if (memory != nullptr) {
        free(memory);
        memory = nullptr;
    }

    g_runtime_running = false;
}

}  // namespace

extern "C" void galaga_trigger_sound_explosion(void) {
    if (game_started) {
        snd_boom_cnt = 2 * sizeof(galaga_sample_boom);
        snd_boom_ptr = reinterpret_cast<const signed char*>(galaga_sample_boom);
    }
}

extern "C" unsigned char buttons_get(void) {
    return g_cached_buttons;
}

extern "C" void audio_dkong_bitrate(char is_dkong) {
    k10_audio_set_dkong_rate(is_dkong != 0);
}

extern "C" unsigned short LoopZ80(Z80* cpu_state) {
    (void)cpu_state;
    return 0xFFFF;
}

bool k10_emulator_supports_machine(K10Machine machine) {
    return machine == K10_MACHINE_GALAGA;
}

bool k10_emulator_start(K10Machine machine) {
    if (!k10_emulator_supports_machine(machine)) {
        return false;
    }

    if (!k10_video_begin() || !ensure_runtime_allocations()) {
        return false;
    }

    teardown_emulation_task();
    reset_audio_state();
    game_started = 0;
    g_cached_buttons = 0;
    g_buffer_index = 0;
    g_present_task = xTaskGetCurrentTaskHandle();
    ulTaskNotifyTake(pdTRUE, 0);

    prepare_emulation();

    const BaseType_t task_ok = xTaskCreatePinnedToCore(emulation_task, "emulation task", kEmulationTaskStackWords,
                                                       nullptr, kEmulationTaskPriority, &g_emulation_task,
                                                       kEmulationTaskCore);
    if (task_ok != pdPASS) {
        teardown_emulation_task();
        return false;
    }

    g_runtime_running = true;
    return true;
}

void k10_emulator_stop() {
    teardown_emulation_task();
    reset_audio_state();
    i2s_zero_dma_buffer(I2S_NUM_0);
}

bool k10_emulator_is_running() {
    return g_runtime_running;
}

bool k10_emulator_run_frame(uint8_t input_state) {
    if (!g_runtime_running) {
        return false;
    }

    g_cached_buttons = input_state;
    update_screen_cpp();
    return true;
}