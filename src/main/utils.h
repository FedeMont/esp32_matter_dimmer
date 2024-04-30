#ifndef __UTILS_H__
#define __UTILS_H__

#include <Arduino.h>

#define SHOULD_LOG_DIMMER
// #define SHOULD_LOG_ESP
#define SHOULD_LED_EXAMPLE

#define MIN_MATTER_LEVEL 3
#define MAX_MATTER_LEVEL 254

// Integrated switchDIM function allows a direct connection of a pushbutton for dimming and switching.
#define EPSILON_MS 10 // ms to account for the time it takes to execute the code
// Brief push (< 0.6 s) switches LED control gear ON and OFF. The dimm level is saved at power-down and restored at power-up.
#define ON_OFF_PUSH_MS 600 // < 0.6s is a push, > 0.6s is a hold
#define LITTLE_PUSH_MS 100
// When the pushbutton is held, LED modules are dimmed. After repush the LED modules are dimmed in the opposite direction.
#define FULL_DIMMING_HOLD_MS 6000 // 6s is full dimming (maybe is not linear)
#define LITTLE_HOLD_MS ON_OFF_PUSH_MS + EPSILON_MS 
#define HOLD_MS_FOR_EACH_PERCENTAGE (float)(FULL_DIMMING_HOLD_MS / (float)(MAX_MATTER_LEVEL - MIN_MATTER_LEVEL)) // time in ms to hold for each 1% (1/255) percentage


enum ButtonState {
    NONE,
    RELEASED,
    PRESSED
};

enum LEDState {
    UP,
    DOWN
};

struct Input {
    const uint8_t PIN;
    ButtonState state;
};

struct Output {
    const uint8_t PIN;
    LEDState state;
};

char *ButtonState_String[] = {
    "NONE",
    "PRESSED"
};

char *LEDState_String[] = {
    "UP",
    "DOWN"
};


#endif // __UTILS_H__