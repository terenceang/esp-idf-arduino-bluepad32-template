# Unihiker K10 Arcade

This folder is the ESP-IDF + Arduino migration target for UniHiker K10 Bluetooth controller support.

What is already ported:
- ESP32-S3 PlatformIO environment in [platformio.ini](platformio.ini)
- K10 flash and serial defaults in [sdkconfig.defaults](sdkconfig.defaults)
- K10 board manifest aligned with the vendor 16 MB partition map, USB IDs, and PSRAM flag in [boards/unihiker_k10_arcade.json](boards/unihiker_k10_arcade.json)
- K10 input bitmask definitions in [main/k10_input.h](main/k10_input.h)
- K10 BLE controller mapper in [main/k10_bt_gamepad.cpp](main/k10_bt_gamepad.cpp)
- K10 expander and I2S audio setup in [main/k10_hardware.cpp](main/k10_hardware.cpp)
- K10 TFT SPI bring-up and an original-style tiled `224x8` DMA row upload path in [main/k10_video.cpp](main/k10_video.cpp)
- A basic boot and input test sketch in [main/sketch.cpp](main/sketch.cpp)
- A machine/menu state handoff layer with original-style `menu -> machine -> coin back to menu` transitions in [main/k10_app.cpp](main/k10_app.cpp)
- A first Galaga-only emulator bridge that reuses the original Z80/emulation core and renders through the arcade DMA upload path in [main/k10_emulator.cpp](main/k10_emulator.cpp)
- The arcade shell now boots in single-game mode straight into Galaga, skipping the menu and using `Start + Coin` as a restart chord

Board-profile note:
- The original DFRobot board uses the `unihiker_k10` Arduino variant, but in the current PlatformIO ESP-IDF + Arduino flow the builder ignores the Arduino `variant` field and falls back to the default `esp32` variant. Direct GPIO assignments in the K10 code remain authoritative until a variant-aware build path is introduced.

What is not ported yet:
- TFT frame content generation from the original project beyond the Galaga runtime path and menu shell
- Emulator/game sources and ROM assets for machines other than Galaga
- LED effects layer
- Exact K10 board definition beyond the generic ESP32-S3 starting point

Recommended next port order:
1. Validate the single-game Galaga runtime on-device and harden restart/reset behavior.
2. Split the Galaga adapter into reusable pieces for the next machine instead of cloning another monolithic port.
3. Bring over LED handling only after the emulator/frame path is stable enough to justify it.
4. Keep the DKong-specific audio-rate switch aligned with the launched machine once that emulator path lands.