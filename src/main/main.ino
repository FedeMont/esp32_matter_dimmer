#include "utils.h"
#include "Matter.h"
#include <app/server/OnboardingCodesUtil.h>
#include <credentials/examples/DeviceAttestationCredsExample.h>
using namespace chip;
using namespace chip::app::Clusters;
using namespace esp_matter;
using namespace esp_matter::endpoint;
using namespace esp_matter::attribute;
typedef void *app_driver_handle_t;

// Cluster and attribute ID used by Matter light device
const cluster_t LEVEL_CLUSTER_ID = LevelControl::Id;
const attribute_t LEVEL_ATTRIBUTE_ID = LevelControl::Attributes::CurrentLevel::Id;

const cluster_t POWER_CLUSTER_ID = OnOff::Id;
const attribute_t POWER_ATTRIBUTE_ID = OnOff::Attributes::OnOff::Id;

// Constants
const bool DEFAULT_POWER = false;
const uint8_t DEFAULT_BRIGHTNESS = 64;

// Endpoint and attribute ref that will be assigned to Matter device
uint16_t dimmer_endpoint_id = 0;
attribute_t *level_attribute_ref;
attribute_t *power_attribute_ref;

#ifdef SHOULD_LED_EXAMPLE
    int led_pin = 16;
    const int freq = 5000;
    const int ledChannel = 0;
    const int resolution = 8;
#endif

Input input = {
    .PIN = 27,
    .state = ButtonState::NONE
};

Output output = {
    .PIN = 4,
    .state = LEDState::DOWN
};

bool is_setup_done = false;

unsigned long button_start_millis = 0; // button start timer
unsigned long button_hold_time = 0; // button hold timer (in ms)
uint32_t time_to_send_signal = 0; // time to send signal to LED
unsigned long last_time_output_high = 0; // last time the output was high

bool is_set_defaults = false;
bool is_dimmer_updating_matter = false;
bool is_power_to_signal = false;
bool should_reset_brightness_to_signal = false;
bool is_brightness_to_signal = false;

bool power_state = DEFAULT_POWER;
int16_t brightness = DEFAULT_BRIGHTNESS;
bool last_power_state = DEFAULT_POWER;
uint8_t last_brightness = 0;

void toggle_output_state() {
    if (output.state == LEDState::UP) {
        output.state = LEDState::DOWN;
    } else {
        output.state = LEDState::UP;
    }
}

void power_to_signal(bool new_power_state) {
#ifdef SHOULD_LOG_DIMMER
    Serial.println("Power to signal");
#endif
    is_power_to_signal = true;

    last_power_state = power_state;
    power_state = new_power_state;
    time_to_send_signal = (uint32_t)(LITTLE_PUSH_MS);
}

void brightness_to_signal(uint8_t new_brightness) {
#ifdef SHOULD_LOG_DIMMER
    Serial.println("Brightness to signal");
#endif
    is_brightness_to_signal = true;

    last_brightness = brightness;
    brightness = (int16_t)new_brightness;
}

void update_power(bool new_power_state) {
    is_dimmer_updating_matter = true;

    void *priv_data = endpoint::get_priv_data(dimmer_endpoint_id);
    app_driver_handle_t handle = (app_driver_handle_t)priv_data;
    node_t *node = node::get();
    endpoint_t *endpoint = endpoint::get(node, dimmer_endpoint_id);
    cluster_t *cluster = NULL;
    attribute_t *attribute = NULL;
    esp_matter_attr_val_t value = esp_matter_invalid(NULL);

    /* Setting brightness */
    cluster = cluster::get(endpoint, POWER_CLUSTER_ID);
    attribute = attribute::get(cluster, POWER_ATTRIBUTE_ID);
    attribute::get_val(attribute, &value);
#ifdef SHOULD_LOG_DIMMER
    Serial.print("update_power old power value: ");
    Serial.println(value.val.b);
#endif
    value.val.b = new_power_state;
    attribute::update(dimmer_endpoint_id, POWER_CLUSTER_ID, POWER_ATTRIBUTE_ID, &value);
}

void update_brightness(uint8_t new_brightness) {
    is_dimmer_updating_matter = true;

    void *priv_data = endpoint::get_priv_data(dimmer_endpoint_id);
    app_driver_handle_t handle = (app_driver_handle_t)priv_data;
    node_t *node = node::get();
    endpoint_t *endpoint = endpoint::get(node, dimmer_endpoint_id);
    cluster_t *cluster = NULL;
    attribute_t *attribute = NULL;
    esp_matter_attr_val_t value = esp_matter_invalid(NULL);

    /* Setting brightness */
    cluster = cluster::get(endpoint, LEVEL_CLUSTER_ID);
    attribute = attribute::get(cluster, LEVEL_ATTRIBUTE_ID);
    attribute::get_val(attribute, &value);
#ifdef SHOULD_LOG_DIMMER
    Serial.print("update_brightness old brightness value: ");
    Serial.println(value.val.u8);
#endif
    value.val.u8 = new_brightness;
    attribute::update(dimmer_endpoint_id, LEVEL_CLUSTER_ID, LEVEL_ATTRIBUTE_ID, &value);
}

/* Do any conversions/remapping for the actual value here */
static esp_err_t app_driver_light_set_power(
    app_driver_handle_t handle, 
    esp_matter_attr_val_t *val
){
    is_dimmer_updating_matter = false;
#ifdef SHOULD_LOG_ESP
    Serial.print("app_driver_light_set_power val: ");
    Serial.println(val->val.b);
#endif

    power_to_signal(val->val.b);

    return ESP_OK;
}

static esp_err_t app_driver_light_set_brightness(
    app_driver_handle_t handle, 
    esp_matter_attr_val_t *val
) {
    is_dimmer_updating_matter = false;

#ifdef SHOULD_LOG_ESP
    Serial.print("app_driver_light_set_brightness val: ");
    Serial.println(val->val.u8);
#endif

    brightness_to_signal(val->val.u8);

    return ESP_OK;
}

esp_err_t app_driver_attribute_update(
    app_driver_handle_t handle, 
    uint16_t endpoint_id, 
    cluster_t cluster_id,
    attribute_t attribute_id, 
    esp_matter_attr_val_t *val
) {
    esp_err_t err = ESP_OK;

    if (endpoint_id == dimmer_endpoint_id) {
        // led_indicator_handle_t handle = (led_indicator_handle_t)driver_handle;
        if (cluster_id == POWER_CLUSTER_ID) {
            if (attribute_id == POWER_ATTRIBUTE_ID) {
                err = app_driver_light_set_power(handle, val);
            }
        } else if (cluster_id == LEVEL_CLUSTER_ID) {
            if (attribute_id == LEVEL_ATTRIBUTE_ID) {
                err = app_driver_light_set_brightness(handle, val);
            }
        }
    }
    
    return err;
}

esp_err_t app_driver_light_set_defaults(uint16_t endpoint_id) {
#ifdef SHOULD_LOG_DIMMER
    Serial.println("app_driver_light_set_defaults");
#endif
    is_set_defaults = true;

    esp_err_t err = ESP_OK;

    void *priv_data = endpoint::get_priv_data(endpoint_id);
    app_driver_handle_t handle = (app_driver_handle_t)priv_data;
    node_t *node = node::get();
    endpoint_t *endpoint = endpoint::get(node, endpoint_id);
    cluster_t *cluster = NULL;
    attribute_t *attribute = NULL;
    esp_matter_attr_val_t val = esp_matter_invalid(NULL);

    /* Setting brightness */
    cluster = cluster::get(endpoint, LEVEL_CLUSTER_ID);
    attribute = attribute::get(cluster, LEVEL_ATTRIBUTE_ID);
    attribute::get_val(attribute, &val);
    err |= app_driver_light_set_brightness(handle, &val);

    /* Setting power */
    cluster = cluster::get(endpoint, POWER_CLUSTER_ID);
    attribute = attribute::get(cluster, POWER_ATTRIBUTE_ID);
    attribute::get_val(attribute, &val);
    err |= app_driver_light_set_power(handle, &val);

    is_set_defaults = false;

    return err;
}

// There is possibility to listen for various device events, related for example to setup process
// Leaved as empty for simplicity
static void on_device_event(const ChipDeviceEvent *event, intptr_t arg) {

}

static esp_err_t on_identification(
    identification::callback_type_t type, 
    uint16_t endpoint_id,
    uint8_t effect_id, 
    uint8_t effect_variant, 
    void *priv_data
) {
    return ESP_OK;
}

static esp_err_t on_attribute_update(
    attribute::callback_type_t type, 
    uint16_t endpoint_id, 
    cluster_t cluster_id,
    attribute_t attribute_id, 
    esp_matter_attr_val_t *val, 
    void *priv_data
) {
    esp_err_t err = ESP_OK;
    if (is_setup_done) {
#ifdef SHOULD_LOG_DIMMER
        Serial.println("||||||||||||||||||||||||||||||||");
        Serial.println("on_attribute_update");
#endif

        if (!is_dimmer_updating_matter) {
            if (type == attribute::PRE_UPDATE && endpoint_id == dimmer_endpoint_id) {
#ifdef SHOULD_LOG_DIMMER
                Serial.println("PRE_UPDATE");
#endif
                app_driver_handle_t driver_handle = (app_driver_handle_t)priv_data;
                err = app_driver_attribute_update(driver_handle, endpoint_id, cluster_id, attribute_id, val);
            }
        } else {
            is_dimmer_updating_matter = false;
        }
    }

    return err;
}


void ARDUINO_ISR_ATTR isr() {
    if (digitalRead(input.PIN) == HIGH) {
        isr_RISING();
    } else {
        isr_FALLING();
    }
}

void ARDUINO_ISR_ATTR isr_RISING() {
    input.state = ButtonState::PRESSED;
}

void ARDUINO_ISR_ATTR isr_FALLING() {
    input.state = ButtonState::RELEASED;
}


void reset() {
#ifdef SHOULD_LOG_DIMMER
    Serial.println("Resetting");
#endif
    button_hold_time = 0;
    button_start_millis = 0;
    input.state = ButtonState::NONE;
}


void button_pressed_loop() {
#ifdef SHOULD_LOG_DIMMER
    Serial.println("Button PRESSED");
#endif
    digitalWrite(output.PIN, HIGH);
    button_start_millis = millis();

    input.state = ButtonState::NONE;
}


void button_pushed() {
#ifdef SHOULD_LOG_DIMMER
    Serial.println("Brief push");
#endif
    power_state = !power_state;

#ifdef SHOULD_LED_EXAMPLE
    ledcWrite(ledChannel, power_state ? brightness : 0);
#endif
    update_power(power_state);
}

void button_held() {
#ifdef SHOULD_LOG_DIMMER
    Serial.println("Button held");
#endif
    if (power_state == false) { // if the light is off, turn it on from 0 brightness
        brightness = 0;
        power_state = true;
        update_power(power_state);
        output.state = LEDState::UP;
#ifdef SHOULD_LOG_DIMMER
        Serial.print("Power state off, resetting brightness: ");
        Serial.print(brightness);
        Serial.print(", and current LED state: ");
        Serial.println(LEDState_String[output.state]);
#endif
    }

    long adjust_percentage = (
        (button_hold_time / HOLD_MS_FOR_EACH_PERCENTAGE) *
        (output.state == LEDState::UP ? 1 : -1)
    );

#ifdef SHOULD_LOG_DIMMER
    Serial.print("Adjust percentage: ");
    Serial.println(adjust_percentage);
    Serial.print("HOLD_MS_FOR_EACH_PERCENTAGE: ");
    Serial.println(HOLD_MS_FOR_EACH_PERCENTAGE);
    Serial.print("(output.state == LEDState::UP ? 1 : -1): ");
    Serial.println((output.state == LEDState::UP ? 1 : -1));
#endif

    brightness += adjust_percentage;

    if (brightness > MAX_MATTER_LEVEL) {
        brightness = MAX_MATTER_LEVEL;
    } else if (brightness < MIN_MATTER_LEVEL) {
        brightness = MIN_MATTER_LEVEL;
    }

#ifdef SHOULD_LED_EXAMPLE
    ledcWrite(ledChannel, brightness);
#endif

    toggle_output_state();
    update_brightness((uint8_t)brightness);

#ifdef SHOULD_LOG_DIMMER
    Serial.print("Brightness after: ");
    Serial.println(brightness);
    Serial.print("Power state after: ");
    Serial.println(power_state);
    Serial.print("Output state after: ");
    Serial.println(LEDState_String[output.state]);
#endif
}

void button_released_loop() {
#ifdef SHOULD_LOG_DIMMER
    Serial.println("Button RELEASED");
#endif
    digitalWrite(output.PIN, LOW);

    button_hold_time = millis() - button_start_millis;
#ifdef SHOULD_LOG_DIMMER
    Serial.print("Hold time: ");
    Serial.println(button_hold_time);
    Serial.print("Power state before: ");
    Serial.println(power_state);
    Serial.print("Brightness before: ");
    Serial.println(brightness);
    Serial.print("Output state before: ");
    Serial.println(LEDState_String[output.state]);
#endif

    if (button_hold_time < ON_OFF_PUSH_MS) { // brief push (< 0.6 s)
        button_pushed();
    } else {
        button_held();
    }
    reset();
}


void matter_control_power() {
// #ifdef SHOULD_LOG_DIMMER
//     Serial.println("Matter control power");
// #endif

    if (last_time_output_high == 0) {
        digitalWrite(output.PIN, HIGH); 
        last_time_output_high = millis();
    } else {
        if (millis() - last_time_output_high >= time_to_send_signal) {
            digitalWrite(output.PIN, LOW);
#ifdef SHOULD_LOG_DIMMER
            Serial.print("Power signal sent for (ms): ");
            Serial.println(time_to_send_signal);
            Serial.print("Power state after: ");
            Serial.println(power_state);
#endif
            last_time_output_high = 0;
            is_power_to_signal = false;

#ifdef SHOULD_LED_EXAMPLE
            ledcWrite(ledChannel, power_state ? brightness : 0);
#endif
        }
    }
}

void matter_control_brightness() {
// #ifdef SHOULD_LOG_DIMMER
//     Serial.println("Matter control brightness");
// #endif

    if (last_time_output_high == 0) { 
        if (power_state == false) { // from 0 to brightness 
        // (should never happen, matter first turns on the power and then sets the brightness)
#ifdef SHOULD_LOG_DIMMER
            Serial.println("Power state was off");
#endif

            // power_state = true;
            time_to_send_signal = (uint32_t)(HOLD_MS_FOR_EACH_PERCENTAGE * brightness);
            output.state = LEDState::UP;

            digitalWrite(output.PIN, HIGH); 
            last_time_output_high = millis();

            should_reset_brightness_to_signal = true;

        } else { // from last_brightness to brightness
#ifdef SHOULD_LOG_DIMMER
            Serial.println("Power state is on");
#endif
            LEDState wantedState = brightness > last_brightness ? LEDState::UP : LEDState::DOWN;

            if (wantedState == output.state) { // go to same state as before -> double hold
#ifdef SHOULD_LOG_DIMMER
                Serial.println("First of Double hold");
#endif
                time_to_send_signal = LITTLE_HOLD_MS;
                uint8_t little_hold_percentage = (uint8_t)(time_to_send_signal / HOLD_MS_FOR_EACH_PERCENTAGE);
                digitalWrite(output.PIN, HIGH); 
                last_time_output_high = millis();

                if (output.state == LEDState::UP) {
                    last_brightness += little_hold_percentage;
                } else {
                    last_brightness -= little_hold_percentage;
                }

                should_reset_brightness_to_signal = false;
                toggle_output_state();
            } else {
#ifdef SHOULD_LOG_DIMMER
                Serial.println("Hold");
#endif
                time_to_send_signal = (uint32_t)(
                    HOLD_MS_FOR_EACH_PERCENTAGE * 
                    abs(brightness - last_brightness)
                );
                digitalWrite(output.PIN, HIGH); 
                last_time_output_high = millis();

                should_reset_brightness_to_signal = true;
                toggle_output_state();
            }
        }
    } else {
        if (millis() - last_time_output_high >= time_to_send_signal) {
            digitalWrite(output.PIN, LOW);

#ifdef SHOULD_LOG_DIMMER
            Serial.print("Brightness signal sent for (ms): ");
            Serial.println(time_to_send_signal);
            Serial.print("Brightness state after: ");
            Serial.println(brightness);
#endif
            last_time_output_high = 0;
            is_brightness_to_signal = should_reset_brightness_to_signal ? false : true;

#ifdef SHOULD_LED_EXAMPLE
            ledcWrite(ledChannel, power_state ? brightness : 0);
#endif
        }
    }

}

void matter_control_loop() {
    if (is_power_to_signal) {
        matter_control_power();
    } else if (is_brightness_to_signal) {
        matter_control_brightness();
    }
}

void setup() {
    Serial.begin(115200);

    pinMode(input.PIN, INPUT_PULLDOWN);
    attachInterrupt(digitalPinToInterrupt(input.PIN), isr, CHANGE);

    pinMode(output.PIN, OUTPUT);
    digitalWrite(output.PIN, LOW);

#ifdef SHOULD_LED_EXAMPLE
    ledcSetup(ledChannel, freq, resolution);
    ledcAttachPin(led_pin, ledChannel);
    ledcWrite(ledChannel, 0);
#endif

    // Enable debug logging
#ifdef SHOULD_LOG_ESP
    esp_log_level_set("*", ESP_LOG_DEBUG);
#endif

    // Setup Matter node
    node::config_t node_config;
    node_t *node = node::create(&node_config, on_attribute_update, on_identification);

    // Setup Dimmable light endpoint / cluster / attributes with default values
    dimmable_light::config_t dimmable_light_config;
    dimmable_light_config.on_off.on_off = DEFAULT_POWER;
    dimmable_light_config.on_off.lighting.start_up_on_off = nullptr;
    dimmable_light_config.level_control.current_level = DEFAULT_BRIGHTNESS;
    dimmable_light_config.level_control.lighting.start_up_current_level = DEFAULT_BRIGHTNESS;
    endpoint_t *endpoint = dimmable_light::create(node, &dimmable_light_config, ENDPOINT_FLAG_NONE, NULL);

    // Save on/off attribute reference. It will be used to read attribute value later.
    level_attribute_ref = attribute::get(cluster::get(endpoint, LEVEL_CLUSTER_ID), LEVEL_ATTRIBUTE_ID);
    power_attribute_ref = attribute::get(cluster::get(endpoint, POWER_CLUSTER_ID), POWER_ATTRIBUTE_ID);

    // Save generated endpoint id
    dimmer_endpoint_id = endpoint::get_id(endpoint);

    // Setup DAC (this is good place to also set custom commission data, passcodes etc.)
    esp_matter::set_custom_dac_provider(chip::Credentials::Examples::GetExampleDACProvider());

    // Start Matter device
    esp_matter::start(on_device_event);

    // app_driver_light_set_defaults(dimmer_endpoint_id);

    // Print codes needed to setup Matter device
    PrintOnboardingCodes(chip::RendezvousInformationFlags(chip::RendezvousInformationFlag::kBLE));
}

void loop() {
    if (!is_setup_done) is_setup_done = true;

    if (input.state == ButtonState::PRESSED) { // button is pressed -> send signal and start timer 
        button_pressed_loop();
    } else if (input.state == ButtonState::RELEASED) { // button is released -> stop signal, calc hold time, reset 
        button_released_loop();
    } else if (input.state == ButtonState::NONE) {
        matter_control_loop();
    }
}