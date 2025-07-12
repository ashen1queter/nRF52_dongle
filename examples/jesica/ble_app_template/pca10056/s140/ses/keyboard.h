/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __KEYBOARD_H
#define __KEYBOARD_H

#include "config.h"
#include <stdint.h>
#include "app_usbd_hid_kbd.h"

#define CALIBRATION_CYCLES 20

#define IDLE_VALUE_APPROX 10
#define MAX_DISTANCE_APPROX 100
#define IDLE_VALUE_OFFSET 10
#define MAX_DISTANCE_OFFSET 60
#define IDLE_CYCLES_UNTIL_SLEEP 15

#define XXXX 0xff
#define ____ 0x00

//#define SPECIAL(X) (0b1000000000000000 | X)

struct __attribute__((__packed__)) calibration {
  uint16_t cycles_count;
  uint16_t idle_value;
  uint16_t max_distance;
};

enum direction {
  GOING_UP,
  GOING_DOWN,
};

struct __attribute__((__packed__)) state {
  uint16_t value;
  uint16_t distance;
  uint8_t distance_8bits;
  float filtered_distance;
  int8_t velocity;
  uint8_t filtered_distance_8bits;
};

enum actuation_status {
  STATUS_MIGHT_BE_TAP,
  STATUS_TAP,
  STATUS_TRIGGERED,
  STATUS_RESET,
  STATUS_RAPID_TRIGGER_RESET
};

enum socd {
  SOCD_KEY,
};

struct __attribute__((__packed__)) actuation {
  enum direction direction;
  uint8_t direction_changed_point;
  enum actuation_status status;
  uint8_t reset_offset;
  uint8_t trigger_offset;
  uint8_t rapid_trigger_offset;
  uint8_t is_continuous_rapid_trigger_enabled;
  uint32_t triggered_at;
  uint8_t is_triggered;
  uint8_t old_trigger;
  enum socd socd;
  uint8_t end_thres;
};

enum key_type {
  KEY_TYPE_EMPTY,
  KEY_TYPE_NORMAL,
  KEY_TYPE_MODIFIER,
  KEY_TYPE_CONSUMER_CONTROL,
};

struct __attribute__((__packed__)) layer {
  enum key_type type;
  uint16_t value;
};

enum {
  _BASE_LAYER,
  _TAP_LAYER,
  LAYERS_COUNT
};

struct __attribute__((__packed__)) key {
  uint8_t is_enabled;
  uint8_t row;
  uint8_t column;
  uint8_t idle_counter;
  uint8_t is_idle;
  struct layer layers[LAYERS_COUNT];
  struct calibration calibration;
  struct state state;
  struct actuation actuation;
  struct actuation *socd_key_pair;
};

struct user_config {
  uint8_t reverse_magnet_pole;
  uint8_t trigger_offset[4];
  uint8_t reset_threshold;
  uint8_t rapid_trigger_offset[4];
  uint8_t screaming_velocity_trigger;
  uint16_t tap_timeout;
  uint8_t socd_[LAYERS_COUNT][MATRIX_COLS];
  uint16_t keymaps[1][3][3];
  bool SOCD;
};

void keyboard_task();
void keyboard_init_keys();

extern struct user_config keyboard_user_config = {
    //.reverse_magnet_pole = DEFAULT_REVERSE_MAGNET_POLE,
    .trigger_offset = {0},
    .reset_threshold = DEFAULT_RESET_THRESHOLD,
    .rapid_trigger_offset = {0},
    .screaming_velocity_trigger = DEFAULT_SCREAMING_VELOCITY_TRIGGER,
    .tap_timeout = DEFAULT_TAP_TIMEOUT,
    .keymaps = {
            {APP_USBD_HID_KBD_A,1,0},{APP_USBD_HID_KBD_W,0,1},{APP_USBD_HID_KBD_D,1,0},{APP_USBD_HID_KBD_S, 0,1}}
};

extern struct key keyboard_keys[3];
extern struct state flex_keys[3];

extern uint8_t socd_pairs[3];

#endif /* __KEYBOARD_H */
