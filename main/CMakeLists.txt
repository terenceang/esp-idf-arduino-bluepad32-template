set(srcs
        "main.c"
        "sketch.cpp"
        "k10_app.cpp"
        "k10_bt_gamepad.cpp"
        "k10_emulator.cpp"
        "k10_emulator_core.c"
        "k10_hardware.cpp"
        "k10_video.cpp")

set(requires "bluepad32" "btstack")

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS "." "arcade_core"
                    REQUIRES "${requires}")
