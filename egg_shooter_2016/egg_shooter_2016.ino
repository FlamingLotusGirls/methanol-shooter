/*
* Egg Shooter control box
 */

const int NUM_SHOOTERS = 6;
const unsigned long MIN_POT_VALUE = 250; // milliseconds
const unsigned long MAX_POT_VALUE = 1500; // milliseconds
const unsigned long SIGNAL_INTERVAL = 20; //milliseconds
const unsigned long ACTIVITY_BLINK_TIME = 60; //milliseconds.  how long to modulate the activity indicator when serial messages are sent.
const int TIMER_INCREMENT = 750; // milliseconds
const int PURGE_TIME = 500; //milliseconds

#define BUTTON_PRESSED_VALUE LOW

// Pins for LEDs
const int LED_1 = 13;
const int LED_2 = 12;
const int LED_3 = 11;

// LED State
//boolean led_1;
boolean led_2;
boolean led_3;
unsigned long activity_until = millis(); // If this is greater than the current time, serial activity is happening.

// Addresses for Relay Boxes
const String BOX1 = "0e";
const String BOX2 = "0f";

// Addresses for individual switched outlets
const String FUEL1 = BOX1 + "1";
const String FUEL2 = BOX1 + "2";
const String FUEL3 = BOX1 + "3";
const String PURGE1 = BOX1 + "4";
const String PURGE2 = BOX1 + "5";
const String PURGE3 = BOX1 + "6";

const String FUEL4 = BOX2 + "1";
const String FUEL5 = BOX2 + "2";
const String FUEL6 = BOX2 + "3";
const String PURGE4 = BOX2 + "4";
const String PURGE5 = BOX2 + "5";
const String PURGE6 = BOX2 + "6";

typedef struct Shooter{
  int button_pin;
  String fuel_address;
  String purge_address;
};

Shooter shooters[] = {
  {2, FUEL1, PURGE1},
  {3, FUEL2, PURGE2},
  {4, FUEL3, PURGE3},
  {5, FUEL4, PURGE4},
  {6, FUEL5, PURGE5},
  {7, FUEL6, PURGE6},
};

const int PURGE_BUTTON_PIN = 8;
const int POT_PIN = A0; // Analog pin for the knob

boolean last_button_state[NUM_SHOOTERS];
boolean last_purge_button_state;

//unsigned long purge_off_time[NUM_SHOOTERS];
unsigned long button_off_time[NUM_SHOOTERS];
unsigned long last_signal_sent[NUM_SHOOTERS];
unsigned long last_purge_all_signal = 0;

unsigned long fuel_off_time[NUM_SHOOTERS];
unsigned long purge_off_time[NUM_SHOOTERS];

boolean last_fuel_valve_state[NUM_SHOOTERS];
boolean last_purge_valve_state[NUM_SHOOTERS];
unsigned long debounce[NUM_SHOOTERS];

int delay_knob_min_value, delay_knob_max_value;
unsigned long pot_value = MIN_POT_VALUE; // start it at the minimum value.

void setup() {
  Serial.begin(19200);

  pinMode(LED_1, OUTPUT);
  pinMode(LED_2, OUTPUT);
  pinMode(LED_3, OUTPUT);
  // digitalWrite(LED_1, HIGH); // Power light!

  pinMode(POT_PIN, INPUT);
  pinMode(PURGE_BUTTON_PIN, INPUT_PULLUP);

  for (int i=0; i<NUM_SHOOTERS; i++) {
    pinMode(shooters[i].button_pin, INPUT_PULLUP);
  }
}


void loop() {
  getPotValue();
  handleButtons();
  handleExpireTimers();
  setStatusLights();
}

void setStatusLights() {
  bool powerLight = false;
  bool fuelLight = false;
  bool purgeLight = false;

  // digitalWrite(LED_1, (millis() < activity_until)? LOW : HIGH);

  for (int i=0; i<NUM_SHOOTERS; i++) {
    if (fuel_off_time[i] > 0) { fuelLight = true; }
    if (purge_off_time[i] > 0) { purgeLight = true; }
    if (button_pressed(i)) { powerLight = true; }
  }
  if (all_purge_button_pressed()) { powerLight = true;}

  digitalWrite(LED_1, powerLight ? HIGH : LOW);
  digitalWrite(LED_2, fuelLight ? HIGH : LOW);
  digitalWrite(LED_3, purgeLight ? HIGH : LOW);

}

void handleExpireTimers(){
  for (int i=0; i<NUM_SHOOTERS; i++) {
    if ( fuel_off_time[i] && fuel_off_time[i] <= millis() ) {
        Serial.println("fuel timer expired.");
        turnOffFuel(i);
        turnOnPurge(i);
    }
  }

  for (int i=0; i<NUM_SHOOTERS; i++) {
    if (purge_off_time[i] > 0) {
      if ( purge_off_time[i] < millis() ) {
          turnOffPurge(i);
      } else {
        // send purge command
        purgeSignal(i, 1, false);
      }
    }
  }

}

/*****/
void turnOnFuel(int i) {
  turnOffPurge(i);
  Serial.print("Fuel on for ");
  Serial.println(pot_value);
  fuel_off_time[i] = millis() + pot_value;
  //send on packet
  Serial.println("turnOnFuel()");
  fuelSignal(i, 1, true);
}

void turnOffFuel(int i) {
  fuel_off_time[i] = 0; // clear timer
  //send off packet
  Serial.println("turnOffFuel()");
  fuelSignal(i, 0, true);
}

void turnOnPurge(int i) {
  purge_off_time[i] = millis() + PURGE_TIME;
  //send on packet
  Serial.println("turnOnPurge()");
  purgeSignal(i, 1, true);
}

void turnOffPurge(int i) {
  purge_off_time[i] = 0; //clear timer
  //send off packet
  Serial.println("turnOffPurge()");
  purgeSignal(i, 0, true);
}

/***/

void sendSignal(int shooter_idx, String address, int value, unsigned long last_signal_time, boolean now) {
  // Unless now is true, only send the signal if we've exceeded the signal interval.
  if ( now || ( (last_signal_time + SIGNAL_INTERVAL) <= millis() ) ) {
    Serial.print("!" + address);
    Serial.print(value);
    Serial.print(".");
    Serial.print("\n");
    last_signal_sent[shooter_idx] = millis();
    activity_until = millis() + ACTIVITY_BLINK_TIME;
  }
}

void fuelSignal(int shooter_idx, int value, boolean now) {
  if ( value != 0 || last_fuel_valve_state[shooter_idx] ) { // "off" signal only sent once"
    //Serial.println("FIRE");
    String address = shooters[shooter_idx].fuel_address;
    unsigned long last_signal_time = last_signal_sent[shooter_idx];
    sendSignal( shooter_idx, address, value, last_signal_time, now);
    last_fuel_valve_state[shooter_idx] = (value > 0);
  }
}

void purgeSignal(int shooter_idx, int value, boolean now) {
  if ( true || value != 0 || last_purge_valve_state[shooter_idx] ) { // "off" signal only sent once"
    //Serial.println("PURGE");
    String address = shooters[shooter_idx].purge_address;
    unsigned long last_signal_time = last_signal_sent[shooter_idx];
    sendSignal( shooter_idx, address, value, last_signal_time, now);
    last_purge_valve_state[shooter_idx] = (value ==  0);
  }
}

void purgeAll() {
  for ( int i=0; i<NUM_SHOOTERS; i++) {
    // purgeSignal(i, 1, true);
    turnOnPurge(i);
  }
}
/***/

bool button_pressed( int button_idx) {
  int val = digitalRead(shooters[button_idx].button_pin);
  return (val == BUTTON_PRESSED_VALUE ? true : false);
}

bool all_purge_button_pressed() {
  return digitalRead(PURGE_BUTTON_PIN) == BUTTON_PRESSED_VALUE;
}

void handleButtons(){
  for (int i=0; i<NUM_SHOOTERS; i++) {
    if (button_pressed(i)) {
      turnOnFuel(i);
    }
  }

  if (all_purge_button_pressed()) {
    purgeAll();
  }

}

void getPotValue() {
  long old_pot_val = pot_value;
  int knob_value = 1023 - analogRead(POT_PIN); // invert
  pot_value = (int)( (knob_value / 1023.0) * (MAX_POT_VALUE - MIN_POT_VALUE) + MIN_POT_VALUE );
  if ( pot_value != old_pot_val ) { Serial.println(pot_value); }
}
