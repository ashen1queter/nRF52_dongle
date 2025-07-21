const int flexPin = A0;
long flexValue = 0;
uint8_t cust_activate = 220;

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
  STATUS_TRIGGERED,
  STATUS_RESET,
  STATUS_RAPID_TRIGGER_RESET
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
  uint8_t end_thres;
};

struct __attribute__((__packed__)) key {
  uint8_t is_enabled;
  uint8_t idle_counter;
  uint8_t is_idle;
  struct state state;
  struct calibration calibration;
  struct actuation actuation;
};

struct key keyboard_keys[1] = {0};
struct state flex_keys[1] = {0};

void init_key() {
  struct key *key = &keyboard_keys[0];

  key->is_enabled = 1;
  key->is_idle = 0;

  key->calibration.cycles_count = 0;
  key->calibration.idle_value = 304; //IDLE_VALUE_APPROX
  key->calibration.max_distance = 200; //MAX_DISTANCE_APPROX

  key->actuation.status = STATUS_RESET;
  cust_activate = cust_activate-key->calibration.max_distance;
  key->actuation.trigger_offset = cust_activate;
  key->actuation.reset_offset = cust_activate+20;
  key->actuation.rapid_trigger_offset = cust_activate;

  Serial.println(key->actuation.trigger_offset = cust_activate);
  //key->actuation.is_continuous_rapid_trigger_enabled = 0;
}


uint8_t update_key_state(struct key *key, struct state *state) {
  if (key->calibration.cycles_count <= 20) {
    float delta = 0.6f;
    key->calibration.idle_value = (1 - delta) * state->value + delta * key->calibration.idle_value;
    key->calibration.cycles_count++;
    Serial.print("Idle value: ");
    Serial.println(key->calibration.idle_value);
    return 0;
  }

  // Ongoing idle value adjustment (slow drift)
  if (state->value > key->calibration.idle_value) {
    float delta = 0.8f;
    key->calibration.idle_value = (1 - delta) * state->value + delta * key->calibration.idle_value;
    state->value = key->calibration.idle_value;
    //Serial.print("Adjusted idle value: ");
    //Serial.println(key->calibration.idle_value);
  }

    // Do nothing if key is idle
  if (key->state.distance == 0 && state->value <= key->calibration.idle_value - 5) {
    if (key->idle_counter >= 15) {
      key->is_idle = 1;
      return 0;
    }
    key->idle_counter++;
  }

  // Get distance from top
  if (state->value >= key->calibration.idle_value - 5 && state->value <= key->calibration.idle_value) {
    state->distance = 0;
    key->actuation.direction_changed_point = 0;
  } else if (state->value <= key->calibration.idle_value && state->value >= key->calibration.max_distance){
    state->distance = key->calibration.idle_value - state->value;
    key->is_idle = 0;
    key->idle_counter = 0;
  }

  /* Calibrate max distance value
  if (state->distance < key->calibration.max_distance) {
    key->calibration.max_distance = state->distance;
  }
  */

  // Limit max distance
  if (state->distance <= key->calibration.max_distance && state->distance >= key->calibration.max_distance + 3) {
    state->distance = key->calibration.max_distance;
  }

  // Map distance in percentages
  state->distance_8bits = state->distance;

  float delta = 0.8;
  state->filtered_distance = (1 - delta) * state->distance_8bits + delta * key->state.filtered_distance;
  state->filtered_distance_8bits = state->filtered_distance;

  // Update velocity
  state->velocity = state->filtered_distance_8bits - key->state.filtered_distance_8bits;

  // Update direction
  if (key->state.velocity > 0 && state->velocity > 0 && key->actuation.direction != GOING_DOWN) {
    key->actuation.direction = GOING_DOWN;
    if (key->actuation.direction_changed_point != 100) {
      key->actuation.direction_changed_point = key->state.filtered_distance_8bits;
    }
  } else if (key->state.velocity < 0 && state->velocity < 0 && key->actuation.direction != GOING_UP) {
    key->actuation.direction = GOING_UP;
    if (key->actuation.direction_changed_point != 305) {
      key->actuation.direction_changed_point = key->state.filtered_distance_8bits;
    }
  }

  key->state = *state;
  /*
  Serial.print("Value: ");
  Serial.print(key->calibration.max_distance);
  Serial.print(" | Distance: ");
  Serial.print(key->state.distance);
  Serial.print(" | Distance_8: ");
  Serial.print(key->state.distance_8bits);
  Serial.print(" | Filtered_8: ");
  Serial.print(key->state.filtered_distance_8bits);
  Serial.print(" | Velocity: ");
  Serial.print(key->state.velocity);
  Serial.print(" | Direction: ");
  Serial.println(key->actuation.direction == GOING_DOWN ? "DOWN" : "UP");
  */
  return 1;
}

void cus_act(){
  long start_time = millis(); 
  long timeout = 60000;
  String value = "";
  Serial.println("Enter custom activation for W:");
  while(millis() - start_time < timeout){
    if(Serial.available()){
      value = Serial.readStringUntil('\n');
      value.trim();
      break;
    }
  } 
  if(value.length() != 0){
    cust_activate = value.toInt();
  } 
  /*
  Serial.print("Value entered: ");
  Serial.println(cust_activate);
  */
  Serial.println("Begin testing");
  Serial.println(".................");
  Serial.println();
}

void update_key_actuation(struct key *key) {
  
  //Serial.println("--- Key State Update ---");
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
  uint8_t is_after_rapid_trigger_offset = key->state.distance_8bits > key->actuation.direction_changed_point - key->actuation.rapid_trigger_offset + 10;
  uint8_t is_before_rapid_reset_offset = key->state.distance_8bits < key->actuation.direction_changed_point - key->actuation.rapid_trigger_offset;
  /*
  Serial.print("is_after_trigger_offset: "); Serial.println(is_after_trigger_offset);
  Serial.print("is_before_reset_offset: "); Serial.println(is_before_reset_offset);
  Serial.print("has_rapid_trigger: "); Serial.println(has_rapid_trigger);
  Serial.print("is_after_rapid_trigger_offset: "); Serial.println(is_after_rapid_trigger_offset);
  Serial.print("is_before_rapid_reset_offset: "); Serial.println(is_before_rapid_reset_offset);
  */
  delay(100);
  switch (key->actuation.status) {

  case STATUS_RESET:
    // if reset, can be triggered or tap
    if (is_after_trigger_offset) {
        key->actuation.status = STATUS_TRIGGERED;
        key->actuation.is_triggered = 1;
        Serial.print("Value:");
        Serial.println(key->state.value);
        Serial.print("Distance 8 bits:");
        Serial.println(key->state.distance_8bits);
        Serial.print("Direction:");
        Serial.println(key->actuation.direction == GOING_UP? "UP":"DOWN");
        Serial.print("Distance where direction changed:");
        Serial.println(key->actuation.direction_changed_point);
        Serial.println("W");
        Serial.println();
      }
    break;

  case STATUS_RAPID_TRIGGER_RESET:
    if (!has_rapid_trigger) {
      key->actuation.status = STATUS_RESET;
      break;
    }
    // if reset, can be triggered or tap
    if (is_after_trigger_offset && key->actuation.direction == GOING_DOWN && is_after_rapid_trigger_offset) {
        key->actuation.status = STATUS_TRIGGERED;
        key->actuation.is_triggered = 1;
        Serial.print("Value:");
        Serial.println(key->state.value);
        Serial.print("Distance 8 bits:");
        Serial.println(key->state.distance_8bits);
        Serial.print("Direction:");
        Serial.println(key->actuation.direction == GOING_UP? "UP":"DOWN");
        Serial.print("Distance where direction changed:");
        Serial.println(key->actuation.direction_changed_point);
        Serial.println("Released");
        Serial.println();
      }
    else if (is_before_reset_offset) {
      key->actuation.status = STATUS_RESET;
    }
    break;

  case STATUS_TRIGGERED:
    // if triggered, can be reset
    if (is_before_reset_offset) {
      key->actuation.status = STATUS_RESET;
      key->actuation.is_triggered = 0;
      Serial.print("Value:");
      Serial.println(key->state.value);
      Serial.print("Distance 8 bits:");
      Serial.println(key->state.distance_8bits);
      Serial.print("Direction:");
      Serial.println(key->actuation.direction == GOING_UP? "UP":"DOWN");
      Serial.print("Distance where direction changed:");
      Serial.println(key->actuation.direction_changed_point);
      Serial.println("Released");
      Serial.println();
    } else if (has_rapid_trigger && key->actuation.direction == GOING_UP && is_before_rapid_reset_offset) {
      key->actuation.status = STATUS_RAPID_TRIGGER_RESET;
      key->actuation.is_triggered = 0;
      Serial.print("Value:");
      Serial.println(key->state.value);
      Serial.print("Distance 8 bits:");
      Serial.println(key->state.distance_8bits);
      Serial.print("Direction:");
      Serial.println(key->actuation.direction == GOING_UP? "UP":"DOWN");
      Serial.print("Distance where direction changed:");
      Serial.println(key->actuation.direction_changed_point);
      Serial.println("Released");
      Serial.println();
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
  /*
  Serial.println("--- Key State Update ---");
  Serial.println(key->state.value);
  Serial.println("--- Actuation Conditions ---");
  //Serial.print("distance_8bits: "); Serial.println(key->state.distance_8bits);
  Serial.print("trigger_offset: "); Serial.println(key->actuation.trigger_offset);
  Serial.print("reset_offset: "); Serial.println(key->actuation.reset_offset);   
  Serial.print("direction_changed_point: "); Serial.println(key->actuation.direction_changed_point);
  Serial.print("rapid_trigger_offset: "); Serial.println(key->actuation.rapid_trigger_offset);
  Serial.println("-----------------------------");
  */
  /*
  if(key->calibration.cycles_count==21){
      while (Serial.available()) Serial.read();
      Serial.println("Enter custom activation for W:");
      while(millis() - start_time < timeout){
        if(Serial.available()){
        value = Serial.readStringUntil('\n');
        value.trim();
        break;
        }
      } 
      if(value.length() != 0){
        cust_activate = value.toInt();
      } 
      Serial.print("Value entered: ");
      Serial.println(cust_activate);
      Serial.println("Begin testing");
      Serial.println(".................");
      Serial.println();

      key->calibration.cycles_count++;    
  }
  */
  update_key_actuation(key);
  //Serial.println("Yo");
}


void keyboard_task(void) {
  struct key *key = &keyboard_keys[0];
  struct state *state = &flex_keys[0];
  state->value = flexValue; //map(flexValue, 0, 1005, 0, 255);
  //Serial.println(state->value);
  //delay(1000);
  if (!key->is_enabled){
  return;}
  update_key(key, state); 
}


void setup() {
  Serial.begin(38400);
  cus_act();
  init_key();
}

void loop() {
  flexValue = analogRead(flexPin);
  keyboard_task();
}