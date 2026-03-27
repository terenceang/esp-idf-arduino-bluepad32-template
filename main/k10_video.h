#ifndef K10_VIDEO_H
#define K10_VIDEO_H

#include <stdint.h>

bool k10_video_begin();
void k10_video_begin_frame();
void k10_video_write(const uint16_t* colors, uint32_t len);
void k10_video_end_frame();
void k10_video_draw_menu_frame(int selection);
void k10_video_draw_machine_frame(int machine);
int k10_video_wrap_menu_selection(int selection, int delta);
const char* k10_video_menu_name(int selection);

#endif