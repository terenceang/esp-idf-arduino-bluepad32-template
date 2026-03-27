#ifndef K10_APP_H
#define K10_APP_H

#include <stdint.h>

enum K10Machine {
    K10_MACHINE_MENU = 0,
    K10_MACHINE_PACMAN,
    K10_MACHINE_GALAGA,
    K10_MACHINE_DKONG,
    K10_MACHINE_FROGGER,
    K10_MACHINE_DIGDUG,
    K10_MACHINE_1942,
    K10_MACHINE_LAST,
};

enum K10AppEvent {
    K10_APP_EVENT_NONE = 0,
    K10_APP_EVENT_MENU_CHANGED,
    K10_APP_EVENT_LAUNCHED,
    K10_APP_EVENT_RETURNED_TO_MENU,
    K10_APP_EVENT_RESTART_REQUESTED,
};

bool k10_app_boot();
K10AppEvent k10_app_handle_input(uint8_t input_state, uint8_t last_input_state);
bool k10_app_single_game_mode();
bool k10_app_in_menu();
K10Machine k10_app_current_machine();
K10Machine k10_app_menu_selection();
const char* k10_app_current_name();

#endif