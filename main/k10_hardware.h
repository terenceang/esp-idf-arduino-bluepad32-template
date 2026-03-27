#ifndef K10_HARDWARE_H
#define K10_HARDWARE_H

#include <stdint.h>

bool k10_prepare_expander();
void k10_audio_init();
void k10_audio_set_dkong_rate(bool is_dkong);
void k10_hw_init();
uint8_t k10_read_inputs();

#endif