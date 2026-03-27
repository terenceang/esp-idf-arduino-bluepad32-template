#include <stdio.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "k10_app.h"
#include "k10_bt_gamepad.h"
#include "k10_emulator.h"
#include "k10_hardware.h"
#include "k10_idf.h"
#include "k10_input.h"
#include "k10_video.h"

static uint8_t last_input_state = 0;
static TaskHandle_t g_app_task = nullptr;

static void start_current_machine() {
    if (!k10_emulator_supports_machine(k10_app_current_machine())) {
        printf("Machine not supported yet: %s\n", k10_app_current_name());
        return;
    }

    if (!k10_emulator_start(k10_app_current_machine())) {
        printf("Emulator start failed: %s\n", k10_app_current_name());
        return;
    }

    printf("Machine active: %s\n", k10_app_current_name());
}

static void k10_app_task(void* parameter) {
    (void)parameter;
    k10_delay_ms(250);

    printf("\n");
    printf("Unihiker K10 Arcade\n");
    printf("This project is the pure ESP-IDF target for BLE controller support.\n");

    k10_hw_init();

    if (k10_app_boot()) {
        if (k10_app_single_game_mode()) {
            printf("Single game boot: %s\n", k10_app_current_name());
            start_current_machine();
        } else {
            printf("Video menu active: %s\n", k10_app_current_name());
        }
    } else {
        printf("Video init failed.\n");
    }

    while (true) {
        const uint8_t input_state = k10_read_inputs();

        switch (k10_app_handle_input(input_state, last_input_state)) {
            case K10_APP_EVENT_MENU_CHANGED:
                printf("Menu: %s\n", k10_app_current_name());
                break;
            case K10_APP_EVENT_LAUNCHED:
                start_current_machine();
                break;
            case K10_APP_EVENT_RETURNED_TO_MENU:
                if (k10_emulator_is_running()) {
                    k10_emulator_stop();
                }
                printf("Return to menu: %s\n", k10_app_current_name());
                break;
            case K10_APP_EVENT_RESTART_REQUESTED:
                if (k10_emulator_is_running()) {
                    k10_emulator_stop();
                }
                printf("Restart game: %s\n", k10_app_current_name());
                start_current_machine();
                break;
            case K10_APP_EVENT_NONE:
            default:
                break;
        }

        if (input_state != last_input_state) {
            last_input_state = input_state;
        }

        if (!k10_app_in_menu() && k10_emulator_is_running()) {
            k10_emulator_run_frame(input_state);
            continue;
        }

        k10_delay_ms(5);
    }
}

extern "C" void k10_start_app_task(void) {
    if (g_app_task != nullptr) {
        return;
    }

    xTaskCreatePinnedToCore(k10_app_task, "k10_app", 8192, nullptr, 2, &g_app_task, 1);
}
