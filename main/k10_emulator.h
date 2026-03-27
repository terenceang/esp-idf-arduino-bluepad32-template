#ifndef K10_EMULATOR_H
#define K10_EMULATOR_H

#include <stdint.h>

#include "k10_app.h"

bool k10_emulator_supports_machine(K10Machine machine);
bool k10_emulator_start(K10Machine machine);
void k10_emulator_stop();
bool k10_emulator_is_running();
bool k10_emulator_run_frame(uint8_t input_state);

#endif