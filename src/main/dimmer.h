#ifndef __DIMMER_H__
#define __DIMMER_H__

#define SHOULD_LOG_DIMMER
// #define SHOULD_LOG_ESP
#define SHOULD_LED_EXAMPLE

// TODO RIMUOVERRE DELAY

#include <Arduino.h>
#include "Matter.h"
#include <app/server/OnboardingCodesUtil.h>
#include <credentials/examples/DeviceAttestationCredsExample.h>

using namespace chip;
using namespace chip::app::Clusters;
using namespace esp_matter;
using namespace esp_matter::endpoint;
using namespace esp_matter::attribute;

enum ButtonState {
    NONE,
    PRESSED
};

static char *ButtonState_String[] = {
    "NONE",
    "PRESSED"
};

enum LEDState {
    UP,
    DOWN
};

static char *LEDState_String[] = {
    "UP",
    "DOWN"
};

class Dimmer {
public:
    Dimmer(bool power_state, uint8_t brightness);
    Dimmer(bool power_state, uint8_t brightness, int input_pin, int output_pin);
    ~Dimmer();

    bool power_state; // true = on, false = off
    int brightness; // 1-100

    const uint16_t *endpoint_id;
    const cluster_t *level_cluster_id;
    const attribute_t *level_attribute_id;
    attribute_t *level_attribute_ref;
    const cluster_t *power_cluster_id;
    const attribute_t *power_attribute_id;
    attribute_t *power_attribute_ref;

    void toggle_power(bool state);
    void set_brightness(int held_time);

    void power_to_signal(bool new_power_state);
    void brightness_to_signal(uint8_t new_brightness);

    void setup(
        const uint16_t *endpoint_id, 
        const cluster_t *level_cluster_id, 
        const attribute_t *level_attribute_id, 
        attribute_t *level_attribute_ref,
        const cluster_t *power_cluster_id, 
        const attribute_t *power_attribute_id,
        attribute_t *power_attribute_ref
    );
    void loop();

private:
    int input_pin; // GPIO27 to read the button state (0 or 1)
    int output_pin; // GPIO4 to control the LED indicator

#ifdef SHOULD_LED_EXAMPLE
    int led_pin = 16;
    const int freq = 5000;
    const int ledChannel = 0;
    const int resolution = 8;
#endif

    // LEDState current_LED_state;
    LEDState previous_LED_state;

    ButtonState button_state;

    const int EPSILON_MS = 10; // ms to account for the time it takes to execute the code

    // Integrated switchDIM function allows a direct connection of a pushbutton for dimming and switching.
    int button_start_millis = 0; // button start timer
    int button_hold_time = 0; // button hold timer (in ms)

    // Brief push (< 0.6 s) switches LED control gear ON and OFF. The dimm level is saved at power-down and restored at power-up.
    const int ON_OFF_PUSH_MS = 600; // < 0.6s is a push, > 0.6s is a hold  
    // When the pushbutton is held, LED modules are dimmed. After repush the LED modules are dimmed in the opposite direction.
    const int FULL_DIMMING_HOLD_MS = 6000; // 6s is full dimming (maybe is not linear)
    const int HOLD_MS_FOR_EACH_PERCENTAGE = FULL_DIMMING_HOLD_MS / (100 - 1); // time in ms to hold for each 1% percentage

    void change_state(int reading);
    void toggle_LED_state();

};

#endif // __DIMMER_H__