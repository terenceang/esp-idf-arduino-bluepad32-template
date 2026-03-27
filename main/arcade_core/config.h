#ifndef _CONFIG_H_
#define _CONFIG_H_

// disable e.g. if roms are missing
// #define ENABLE_PACMAN
#define ENABLE_GALAGA
// #define ENABLE_DKONG
// #define ENABLE_FROGGER
// #define ENABLE_DIGDUG
// #define ENABLE_1942

#if !defined(ENABLE_PACMAN) && !defined(ENABLE_GALAGA) && !defined(ENABLE_DKONG) && !defined(ENABLE_FROGGER) && !defined(ENABLE_DIGDUG) && !defined(ENABLE_1942)
#error "At least one machine has to be enabled!"
#endif

// check if only one machine is enabled
#if (( defined(ENABLE_PACMAN) && !defined(ENABLE_GALAGA) && !defined(ENABLE_DKONG) && !defined(ENABLE_FROGGER) && !defined(ENABLE_DIGDUG) && !defined(ENABLE_1942)) || \
     (!defined(ENABLE_PACMAN) &&  defined(ENABLE_GALAGA) && !defined(ENABLE_DKONG) && !defined(ENABLE_FROGGER) && !defined(ENABLE_DIGDUG) && !defined(ENABLE_1942)) || \
     (!defined(ENABLE_PACMAN) && !defined(ENABLE_GALAGA) &&  defined(ENABLE_DKONG) && !defined(ENABLE_FROGGER) && !defined(ENABLE_DIGDUG) && !defined(ENABLE_1942)) || \
     (!defined(ENABLE_PACMAN) && !defined(ENABLE_GALAGA) && !defined(ENABLE_DKONG) &&  defined(ENABLE_FROGGER) && !defined(ENABLE_DIGDUG) && !defined(ENABLE_1942)) || \
     (!defined(ENABLE_PACMAN) && !defined(ENABLE_GALAGA) && !defined(ENABLE_DKONG) && !defined(ENABLE_FROGGER) &&  defined(ENABLE_DIGDUG) && !defined(ENABLE_1942)) || \
     (!defined(ENABLE_PACMAN) && !defined(ENABLE_GALAGA) && !defined(ENABLE_DKONG) && !defined(ENABLE_FROGGER) && !defined(ENABLE_DIGDUG) &&  defined(ENABLE_1942)))
  #define SINGLE_MACHINE
#endif

// game config

#define MASTER_ATTRACT_MENU_TIMEOUT  20000   // start games randomly while sitting idle in menu for 20 seconds, undefine to disable

#include "dip_switches.h"

// video config
#define TFT_SPICLK  80000000

#define TFT_MISO -1
#define TFT_MOSI 21
#define TFT_SCLK 12

#define TFT_CS   14
#define TFT_DC   13
#define TFT_RST  -1  // RST is not directly connected or managed via GPIO
#define TFT_BL_EXPANDER // Backlight controlled via XL9535 Expander P0
#define TFT_ILI9341
#define TFT_VFLIP

#define K10_I2C_SDA 47
#define K10_I2C_SCL 48
#define K10_EXPANDER_ADDR 0x20

#define GALAGA_AUDIO_SAMPLE_RATE         24000
#define DKONG_AUDIO_SAMPLE_RATE          11765

#define K10_I2S_BCK   0
#define K10_I2S_WS   38
#define K10_I2S_DOUT 45
#define K10_I2S_MCLK  3

// XL9535 Register Addresses
#define XL9535_INPUT_REG_0  0x00
#define XL9535_INPUT_REG_1  0x01
#define XL9535_OUTPUT_REG_0 0x02
#define XL9535_OUTPUT_REG_1 0x03
#define XL9535_CONFIG_REG_0 0x06
#define XL9535_CONFIG_REG_1 0x07

// K10 IO Expander Pin Mapping (Port 0/1)
// P00: LCD_BLK (Port 0 Bit 0)
// P01: Camera hold/reset (Port 0 Bit 1)
// P02: Key B (Port 0 Bit 2)
// P14: Key A (Port 1 Bit 4)
#define K10_BIT_BACKLIGHT  0  // P00
#define K10_BIT_CAMERA_HOLD 1 // P01
#define K10_BIT_KEY_B  2  // P02
#define K10_BIT_KEY_A  4  // P14 (Bit 4 of Port 1)
#define K10_MASK_BACKLIGHT   (1U << K10_BIT_BACKLIGHT)
#define K10_MASK_CAMERA_HOLD (1U << K10_BIT_CAMERA_HOLD)
#define K10_MASK_KEY_B (1U << K10_BIT_KEY_B)
#define K10_MASK_KEY_A (1U << K10_BIT_KEY_A)

#define K10_EXPANDER_OUTPUT_PORT0 (K10_MASK_BACKLIGHT | K10_MASK_CAMERA_HOLD)
#define K10_EXPANDER_CONFIG_PORT0 ((uint8_t)~K10_EXPANDER_OUTPUT_PORT0)
#define K10_EXPANDER_CONFIG_PORT1 0xFF

#define TFT_X_OFFSET  8
#define TFT_Y_OFFSET 16

#define AUDIO_VOLUME   50   // 0 (mute) to 100 (full)

// Bluetooth HID gamepad support needs a Bluepad32-enabled ESP32 toolchain.
// This DFRobot PlatformIO Arduino target does not provide Bluepad32 as a normal lib_deps package.
// Leave this disabled unless the project is migrated to a Bluepad32-capable board package or ESP-IDF setup.
#define BT_GAMEPAD

#define DISABLE_ONBOARD_BUTTONS  // physical Key A / Key B not used

#define LED_PIN        7
#define LED_BRIGHTNESS 50

// K10 buttons are read from the IO expander in hardware.cpp.

#endif // _CONFIG_H_
