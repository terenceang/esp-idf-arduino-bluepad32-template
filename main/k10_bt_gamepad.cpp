#include "k10_bt_gamepad.h"

#include <stdio.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

extern "C" {
#include <uni.h>
}

#include "k10_idf.h"
#include "k10_input.h"

namespace {

struct CachedGamepad {
    bool connected;
    uni_gamepad_t gamepad;
};

constexpr uint16_t kFireMask = BUTTON_A | BUTTON_B | BUTTON_X | BUTTON_Y | BUTTON_SHOULDER_L | BUTTON_SHOULDER_R;

SemaphoreHandle_t g_bt_mutex = nullptr;
CachedGamepad g_btPads[CONFIG_BLUEPAD32_MAX_DEVICES];

void platform_init(int argc, const char** argv) {
    (void)argc;
    (void)argv;

    if (g_bt_mutex == nullptr) {
        g_bt_mutex = xSemaphoreCreateMutex();
    }
    memset(g_btPads, 0, sizeof(g_btPads));
}

uni_error_t on_device_discovered(bd_addr_t addr, const char* name, uint16_t cod, uint8_t rssi) {
    (void)addr;
    (void)name;
    (void)cod;
    (void)rssi;
    return UNI_ERROR_SUCCESS;
}

void on_init_complete(void) {
    k10_start_app_task();
}

void on_device_connected(uni_hid_device_t* device) {
    const int index = uni_hid_device_get_idx_for_instance(device);
    if (index < 0 || index >= CONFIG_BLUEPAD32_MAX_DEVICES) {
        return;
    }

    if (g_bt_mutex != nullptr) {
        xSemaphoreTake(g_bt_mutex, portMAX_DELAY);
        g_btPads[index].connected = false;
        memset(&g_btPads[index].gamepad, 0, sizeof(g_btPads[index].gamepad));
        xSemaphoreGive(g_bt_mutex);
    }
}

void on_device_disconnected(uni_hid_device_t* device) {
    const int index = uni_hid_device_get_idx_for_instance(device);
    if (index < 0 || index >= CONFIG_BLUEPAD32_MAX_DEVICES) {
        return;
    }

    if (g_bt_mutex != nullptr) {
        xSemaphoreTake(g_bt_mutex, portMAX_DELAY);
        g_btPads[index].connected = false;
        memset(&g_btPads[index].gamepad, 0, sizeof(g_btPads[index].gamepad));
        xSemaphoreGive(g_bt_mutex);
    }

    printf("BT gamepad disconnected\n");
}

uni_error_t on_device_ready(uni_hid_device_t* device) {
    const int index = uni_hid_device_get_idx_for_instance(device);
    if (index < 0 || index >= CONFIG_BLUEPAD32_MAX_DEVICES) {
        return UNI_ERROR_IGNORE_DEVICE;
    }

    if (!uni_hid_device_is_gamepad(device)) {
        return UNI_ERROR_SUCCESS;
    }

    if (g_bt_mutex != nullptr) {
        xSemaphoreTake(g_bt_mutex, portMAX_DELAY);
        g_btPads[index].connected = true;
        memset(&g_btPads[index].gamepad, 0, sizeof(g_btPads[index].gamepad));
        xSemaphoreGive(g_bt_mutex);
    }

    printf("BT gamepad connected\n");
    return UNI_ERROR_SUCCESS;
}

void on_controller_data(uni_hid_device_t* device, uni_controller_t* controller) {
    const int index = uni_hid_device_get_idx_for_instance(device);
    if (index < 0 || index >= CONFIG_BLUEPAD32_MAX_DEVICES) {
        return;
    }

    if (controller->klass != UNI_CONTROLLER_CLASS_GAMEPAD || g_bt_mutex == nullptr) {
        return;
    }

    xSemaphoreTake(g_bt_mutex, portMAX_DELAY);
    g_btPads[index].connected = true;
    g_btPads[index].gamepad = controller->gamepad;
    xSemaphoreGive(g_bt_mutex);
}

const uni_property_t* get_property(uni_property_idx_t index) {
    (void)index;
    return nullptr;
}

void on_oob_event(uni_platform_oob_event_t event, void* data) {
    (void)event;
    (void)data;
}

struct uni_platform g_platform = {
    "K10 Arcade",
    &platform_init,
    &on_init_complete,
    &on_device_discovered,
    &on_device_connected,
    &on_device_disconnected,
    &on_device_ready,
    nullptr,
    &on_controller_data,
    &get_property,
    &on_oob_event,
    nullptr,
    nullptr,
};

uint8_t map_gamepad_to_inputs(const uni_gamepad_t& gamepad) {
    uint8_t result = 0;

    if ((gamepad.dpad & DPAD_UP) || gamepad.axis_y < -200) {
        result |= K10_BUTTON_UP;
    }
    if ((gamepad.dpad & DPAD_DOWN) || gamepad.axis_y > 200) {
        result |= K10_BUTTON_DOWN;
    }
    if ((gamepad.dpad & DPAD_LEFT) || gamepad.axis_x < -200) {
        result |= K10_BUTTON_LEFT;
    }
    if ((gamepad.dpad & DPAD_RIGHT) || gamepad.axis_x > 200) {
        result |= K10_BUTTON_RIGHT;
    }
    if (gamepad.buttons & kFireMask) {
        result |= K10_BUTTON_FIRE;
    }
    if (gamepad.misc_buttons & MISC_BUTTON_START) {
        result |= K10_BUTTON_START;
    }
    if (gamepad.misc_buttons & MISC_BUTTON_SELECT) {
        result |= K10_BUTTON_COIN;
    }

    return result;
}

}  // namespace

extern "C" struct uni_platform* k10_get_bluepad_platform(void) {
    return &g_platform;
}

void k10_bt_gamepad_setup() {
    if (g_bt_mutex == nullptr) {
        memset(g_btPads, 0, sizeof(g_btPads));
        return;
    }

    xSemaphoreTake(g_bt_mutex, portMAX_DELAY);
    memset(g_btPads, 0, sizeof(g_btPads));
    xSemaphoreGive(g_bt_mutex);
}

uint8_t k10_bt_gamepad_read() {
    uint8_t result = 0;
    if (g_bt_mutex == nullptr) {
        return 0;
    }

    xSemaphoreTake(g_bt_mutex, portMAX_DELAY);
    for (int index = 0; index < CONFIG_BLUEPAD32_MAX_DEVICES; ++index) {
        if (g_btPads[index].connected) {
            result |= map_gamepad_to_inputs(g_btPads[index].gamepad);
        }
    }
    xSemaphoreGive(g_bt_mutex);

    return result;
}

bool k10_bt_gamepad_connected() {
    if (g_bt_mutex == nullptr) {
        return false;
    }

    bool connected = false;
    xSemaphoreTake(g_bt_mutex, portMAX_DELAY);
    for (int index = 0; index < CONFIG_BLUEPAD32_MAX_DEVICES; ++index) {
        if (g_btPads[index].connected) {
            connected = true;
            break;
        }
    }
    xSemaphoreGive(g_bt_mutex);

    return connected;
}
