#ifndef K10_BT_GAMEPAD_H
#define K10_BT_GAMEPAD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct uni_platform;

void k10_bt_gamepad_setup();
uint8_t k10_bt_gamepad_read();
bool k10_bt_gamepad_connected();
struct uni_platform* k10_get_bluepad_platform(void);

#ifdef __cplusplus
}
#endif

#endif