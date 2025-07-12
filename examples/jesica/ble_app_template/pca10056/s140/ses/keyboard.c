#include "keyboard.h"
//#include "DRV2605L.h"
#include "hid.h"
//#include <class/hid/hid.h>
#include <stdlib.h>
#include "main.h"
#include "config.h"


struct key keyboard_keys[] = {0};
struct state flex_keys[] = {0};


uint8_t socd_pairs[] = { [0 ... 2] = -1 };
uint8_t thres_pairs[] = { [0 ... 2] = -1 };


static void store_socd_pairs(struct key *a, struct key *b)
{
    a->socd_key_pair = &b->actuation;
    b->socd_key_pair = &a->actuation;
}


static void store_thres_pairs(struct key *a[3][3], struct key *b[3][3])
{
    a->actuation.end_thres = b->actuation.rapid_trigger_offset;
    b->actuation.end_thres = MAX_DISTANCE_OFFSET;
}


uint32_t keyboard_last_cycle_duration = 0;

static uint8_t key_triggered = 0;


void init_key(uint8_t i, uint8_t *socd_, uint8_t *pair_thres_) {
  struct key *key = &keyboard_keys[i];
  uint8_t socd_key_counter = 0;
  struct actuation *socd_key1 = {0};
  uint8_t j = 0;

  keyboard_user_config.rapid_trigger_offset[i] = cus_actuation[i];
  keyboard_user_config.trigger_offset[i] = cus_actuation[i];
  
  if (socd_ > 0 && socd_ <= 2) {
        if (socd_pairs[socd_] == -1) {
            socd_pairs[socd_] = i;
        } else {
            store_socd_pairs(&keyboard_keys[socd_pairs[socd_]], &keyboard_keys[i]);
        }
  if (pair_thres_ > 0 && pair_thres_ <= 2) {
        if (thres_pairs[socd_] == -1) {
            thres_pairs[socd_] = i;
        } else {
            store_thres_pairs(&keyboard_keys[thres_pairs[pair_thres_]], &keyboard_keys[i]);
        }
      

      /**
      if(socd_key_counter == 0){
          key->actuation.socd = SOCD_KEY;
          socd_key1 = &key->actuation;
          ++socd_key_counter;
          j = i;
      }
      else if(socd_key_counter == 1){
          key->actuation.socd = SOCD_KEY;
          key->socd_key_pair = socd_key1;
          keyboard_keys[j].socd_key_pair = &key->actuation;
      }
      */
  }

  key->is_enabled = 1;
  key->is_idle = 0;

  key->calibration.cycles_count = 0;
  key->calibration.idle_value = IDLE_VALUE_APPROX;
  key->calibration.max_distance = MAX_DISTANCE_APPROX;

  key->actuation.status = STATUS_RESET;
  key->actuation.trigger_offset = keyboard_user_config.trigger_offset[i];
  key->actuation.reset_offset = keyboard_user_config.trigger_offset[i] - keyboard_user_config.reset_threshold;
  key->actuation.rapid_trigger_offset = keyboard_user_config.rapid_trigger_offset[i];
  key->actuation.is_continuous_rapid_trigger_enabled = 0;


    if (keyboard_user_config.keymaps[i][row][column] != ____) {
          key->layers[i].type = KEY_TYPE_NORMAL;
          key->layers[i].value = keyboard_user_config.keymaps[i][row][column][0];
        }
}
}

uint8_t update_key_state(struct key *key, struct state *state) {

  // Get a reading
  //state.value = keyboard_user_config.reverse_magnet_pole ? 4500 - keyboard_read_adc() : keyboard_read_adc();

  if (key->calibration.cycles_count < CALIBRATION_CYCLES) {
    // Calibrate idle value
    float delta = 0.6;
    key->calibration.idle_value = (1 - delta) * state->value + delta * key->calibration.idle_value;
    key->calibration.cycles_count++;

    return 0;
  }

  // Calibrate idle value
  if (state->value < key->calibration.idle_value) {
    // opti possible sur float
    float delta = 0.8;
    key->calibration.idle_value = (1 - delta) * state->value + delta * key->calibration.idle_value;
    state->value = key->calibration.idle_value;
  }

  // Do nothing if key is idle
  if (key->state.distance == 0 && state->value <= key->calibration.idle_value + IDLE_VALUE_OFFSET) {
    if (key->idle_counter >= IDLE_CYCLES_UNTIL_SLEEP) {
      key->is_idle = 1;
      return 0;
    }
    key->idle_counter++;
  }

  // Get distance from top
  if (state->value <= key->calibration.idle_value + IDLE_VALUE_OFFSET) {
    state->distance = 0;
    key->actuation.direction_changed_point = 0;
  } else {
    state->distance = state->value - key->calibration.idle_value + IDLE_VALUE_OFFSET;
    key->is_idle = 0;
    key->idle_counter = 0;
  }

  // Calibrate max distance value
  if (state->distance > key->calibration.max_distance) {
    key->calibration.max_distance = state->distance;
  }

  // Limit max distance
  if (state->distance >= key->calibration.max_distance - MAX_DISTANCE_OFFSET) {
    state->distance = key->calibration.max_distance;
  }

  // Map distance in percentages
  state->distance_8bits = (state->distance * 0xff) / key->calibration.max_distance;

  float delta = 0.8;
  state->filtered_distance = (1 - delta) * state->distance_8bits + delta * key->state.filtered_distance;
  state->filtered_distance_8bits = state->filtered_distance;

  // Update velocity
  state->velocity = state->filtered_distance_8bits - key->state.filtered_distance_8bits;

  // Update direction
  if (key->state.velocity > 0 && state->velocity > 0 && key->actuation.direction != GOING_DOWN) {
    key->actuation.direction = GOING_DOWN;
    if (key->actuation.direction_changed_point != 0) {
      key->actuation.direction_changed_point = key->state.filtered_distance_8bits;
    }
  } else if (key->state.velocity < 0 && state->velocity < 0 && key->actuation.direction != GOING_UP) {
    key->actuation.direction = GOING_UP;
    if (key->actuation.direction_changed_point != 255) {
      key->actuation.direction_changed_point = key->state.filtered_distance_8bits;
    }
  }

  key->state = *state;
  return 1;
}

void update_key_actuation(struct key *key) {
  /**
   * https://www.youtube.com/watch?v=_Sl-T6iQr8U&t
   *
   *                          -----   |--------|                           -
   *                            |     |        |                           |
   *    is_before_reset_offset  |     |        |                           |
   *                            |     |        |                           | Continuous rapid trigger domain (deactivated when full_reset)
   *                          -----   | ------ | <- reset_offset           |
   *                            |     |        |                           |
   *                          -----   | ------ | <- trigger_offset         -
   *                            |     |        |                           |
   *                            |     |        |                           |
   *   is_after_trigger_offset  |     |        |                           | Rapid trigger domain
   *                            |     |        |                           |
   *                            |     |        |                           |
   *                          -----   |--------|                           -
   *
   */

  // if rapid trigger enable, move trigger and reset offsets according to the distance taht began the trigger

  //uint32_t now = keyboard_get_time();
  uint8_t is_after_trigger_offset = key->state.distance_8bits > key->actuation.trigger_offset;
  uint8_t is_before_reset_offset = key->state.distance_8bits < key->actuation.reset_offset;
  uint8_t has_rapid_trigger = key->actuation.rapid_trigger_offset != 0;
  uint8_t is_after_rapid_trigger_offset = key->state.distance_8bits > key->actuation.direction_changed_point - key->actuation.rapid_trigger_offset + keyboard_user_config.reset_threshold;
  uint8_t is_before_rapid_reset_offset = key->state.distance_8bits < key->actuation.direction_changed_point - key->actuation.rapid_trigger_offset;
  
  if(key->actuation.socd == SOCD_KEY && key->socd_key_pair->socd == SOCD_KEY){
       if (key->actuation.is_triggered == 1 && key->socd_key_pair->is_triggered == 0) {
              key->actuation.status = STATUS_RESET;
              key->socd_key_pair->status = STATUS_TRIGGERED;
      }
      if (key->actuation.is_triggered == 0 && key->socd_key_pair->is_triggered == 1) {
              key->actuation.status = STATUS_TRIGGERED;
              key->socd_key_pair->status = STATUS_RESET;
      }

      if (key->actuation.is_triggered == 0 && key->socd_key_pair->is_triggered == 0) {
              key->actuation.status = STATUS_TRIGGERED;
              key->socd_key_pair->status = STATUS_TRIGGERED;
      }

      if (key->actuation.is_triggered == 1 && key->socd_key_pair->is_triggered == 1) {
        if (key->actuation.old_trigger == 1) {
            key->actuation.status = STATUS_TRIGGERED;
            key->socd_key_pair->status = STATUS_RESET;
        }
      if (key->socd_key_pair->old_trigger == 1) {
              key->actuation.status = STATUS_RESET;
              key->socd_key_pair->status = STATUS_TRIGGERED;
        }
      } else {
            key->actuation.old_trigger = key->actuation.is_triggered;
            key->socd_key_pair->old_trigger = key->socd_key_pair->is_triggered;
            }
  }
  

  switch (key->actuation.status) {

  case STATUS_RESET:
    // if reset, can be triggered or tap
    if (is_after_trigger_offset) {
      if (key->layers[_TAP_LAYER].value) {
        key->actuation.status = STATUS_MIGHT_BE_TAP;
        // key_triggered = 1;
      } else {
        key->actuation.status = STATUS_TRIGGERED;
        key->actuation.is_triggered = 1;
        hid_press_key(key, _BASE_LAYER);
      }
    }
    break;

  case STATUS_RAPID_TRIGGER_RESET:
    if (!has_rapid_trigger) {
      key->actuation.status = STATUS_RESET;
      break;
    }
    // if reset, can be triggered or tap
    if (is_after_trigger_offset && key->actuation.direction == GOING_DOWN && is_after_rapid_trigger_offset) {
      if (key->layers[_TAP_LAYER].value) {
        key->actuation.status = STATUS_MIGHT_BE_TAP;
        key->actuation.is_triggered = 1;
      } else {
        key->actuation.status = STATUS_TRIGGERED;
        key->actuation.is_triggered = 1;
        hid_press_key(key, _BASE_LAYER);
      }
    } else if (is_before_reset_offset) {
      key->actuation.status = STATUS_RESET;
    }
    break;

  case STATUS_TAP:
    // if tap, can be reset
    key->actuation.status = STATUS_RESET;
    key->actuation.is_triggered = 0;
    hid_release_key(key, _TAP_LAYER);
    break;

  case STATUS_TRIGGERED:
    // if triggered, can be reset
    if (is_before_reset_offset) {
      key->actuation.status = STATUS_RESET;
      key->actuation.is_triggered = 0;
      hid_release_key(key, _BASE_LAYER);
    } else if (has_rapid_trigger && key->actuation.direction == GOING_UP && is_before_rapid_reset_offset) {
      key->actuation.status = STATUS_RAPID_TRIGGER_RESET;
      key->actuation.is_triggered = 0;
      hid_release_key(key, _BASE_LAYER);
    }
    break;

  default:
    break;
  }
}

void update_key(struct key *key, struct state *state) {
  if (!update_key_state(key, state)) {
    return;
  }

  update_key_actuation(key);
}

void keyboard_init_keys(void) {

  for (uint8_t i = 0; i < 4; i++) {
                  init_key(i, keyboard_user_config.keymaps[i][1], keyboard_user_config.keymaps[i][2]);
              }
          }

void keyboard_task(void) {
  
  for (uint8_t i = 0; i < 3; i++) {
    struct key *key = &keyboard_keys[i];
    struct state *state = &flex_keys[i];
    state->value = adc_values[i];

    if (!key->is_enabled)
      continue;

    update_key(key, state); 
  }

  for (uint8_t i = 0; i < 3; i++) {
    struct key *key = &keyboard_keys[i];

    if (!key->is_enabled || key->actuation.status != STATUS_MIGHT_BE_TAP)
      continue;

    uint8_t is_before_reset_offset = key->state.distance_8bits < key->actuation.reset_offset;
   
    if (is_before_reset_offset) {
      key->actuation.status = STATUS_TAP;
      hid_press_key(key, _TAP_LAYER);  // tap behavior
    } else if (key_triggered) {
      key->actuation.status = STATUS_TRIGGERED;
      hid_press_key(key, _BASE_LAYER);  // normal key
    }
}
}