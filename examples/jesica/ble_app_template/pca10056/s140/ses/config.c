#include "config.h"
#include "DRV2605L.h"
#include "keyboard.h"
#include "stdlib.h"
//#include <class/hid/hid.h>

/**
const struct user_config keyboard_default_user_config = {
    //.reverse_magnet_pole = DEFAULT_REVERSE_MAGNET_POLE,
    .trigger_offset = DEFAULT_TRIGGER_OFFSET,
    .reset_threshold = DEFAULT_RESET_THRESHOLD,
    .rapid_trigger_offset = {0}, //DEFAULT_OFFSET or offset set by the user (custom actuation point)
    .screaming_velocity_trigger = DEFAULT_SCREAMING_VELOCITY_TRIGGER,
    .tap_timeout = DEFAULT_TAP_TIMEOUT,
    .keymaps = {
        // clang-format off
        [_BASE_LAYER] = {
            {HID_KEY_A, HID_KEY_W, HID_KEY_D}
        },
        [_TAP_LAYER] = {
            {____, HID_KEY_S, ____}
        },
        // clang-format on
    }};
*/

/** {adc_channel, amux_channel}
*/
const uint8_t channels_by_row_col[MATRIX_ROWS][MATRIX_COLS][1] = {
    {{0}, {1}, {2}}
};
