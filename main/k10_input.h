#ifndef K10_INPUT_H
#define K10_INPUT_H

#include <stdint.h>

enum : uint8_t {
    K10_BUTTON_LEFT = 0x01,
    K10_BUTTON_RIGHT = 0x02,
    K10_BUTTON_UP = 0x04,
    K10_BUTTON_DOWN = 0x08,
    K10_BUTTON_FIRE = 0x10,
    K10_BUTTON_START = 0x20,
    K10_BUTTON_COIN = 0x40,
    K10_BUTTON_EXTRA = 0x80,
};

#endif