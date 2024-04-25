#define CONFIG_BSP_LEDS_NUM  1
#define BSP_LED_ON  1
#define BSP_LED_OFF 0

#include "dimmer.h"

// DIMMER_CPP
// PUBLIC
Dimmer::Dimmer(bool power_state, uint8_t brightness) {
    this->power_state = power_state;
    this->brightness = brightness;

    this->input_pin = 27;
    this->output_pin = 4;

    // this->current_LED_state = LEDState::UP;
    this->previous_LED_state = LEDState::UP;
    this->button_state = ButtonState::NONE;
}

Dimmer::Dimmer(bool power_state, uint8_t brightness, int input_pin, int output_pin) {
    this->power_state = power_state;
    this->brightness = brightness;

    this->input_pin = input_pin;
    this->output_pin = output_pin;

    // this->current_LED_state = LEDState::UP;
    this->previous_LED_state = LEDState::UP;
    this->button_state = ButtonState::NONE;
}

Dimmer::~Dimmer() {
}

void Dimmer::toggle_power(bool state) {
    this->power_state = state;

    esp_matter_attr_val_t power_value = esp_matter_invalid(NULL);
    attribute::get_val(this->power_attribute_ref, &power_value);

    power_value.val.b = this->power_state;

    attribute::update(
        *this->endpoint_id, 
        *this->power_cluster_id, 
        *this->power_attribute_id, 
        &power_value
    );

#ifdef SHOULD_LED_EXAMPLE
    ledcWrite(this->ledChannel, this->power_state ? int(this->brightness * 2.55) : 0);
#endif
#ifdef SHOULD_LOG_DIMMER
    Serial.print("Power state: ");
    Serial.println(ButtonState_String[this->power_state]);
    // Serial.print("Current LED state: ");
    // Serial.println(LEDState_String[this->current_LED_state]);
    Serial.print("Previous LED state: ");
    Serial.println(LEDState_String[this->previous_LED_state]);
    Serial.print("Brigthness: ");
    Serial.println(this->brightness);
#endif
}

void Dimmer::set_brightness(int held_time) {
#ifdef SHOULD_LOG_DIMMER
    Serial.print("Power state: ");
    Serial.println(ButtonState_String[this->power_state]);
    Serial.print("Before brightness: ");
    Serial.println(this->brightness);
    // Serial.print("Current LED state: ");
    // Serial.println(LEDState_String[this->current_LED_state]);
    Serial.print("Previous LED state: ");
    Serial.println(LEDState_String[this->previous_LED_state]);
#endif

    if (this->power_state == false) {
        this->brightness = 0;
        this->previous_LED_state = LEDState::UP;
        // this->current_LED_state = LEDState::UP;
#ifdef SHOULD_LOG_DIMMER
        Serial.print("Power state off, resetting brightness: ");
        Serial.print(this->brightness);
        Serial.print(", and current LED state: ");
        Serial.println(LEDState_String[this->previous_LED_state]);
        // Serial.println(LEDState_String[this->current_LED_state]);
#endif
    }
    
    int adjust_percentage = (
        int(held_time / this->HOLD_MS_FOR_EACH_PERCENTAGE) *
        (this->previous_LED_state == LEDState::UP ? 1 : -1)
        // (this->current_LED_state == LEDState::UP ? 1 : -1)
    );

#ifdef SHOULD_LOG_DIMMER
    Serial.print("Adjust percentage: ");
    Serial.println(adjust_percentage);
#endif

    this->brightness += adjust_percentage;

#ifdef SHOULD_LOG_DIMMER
    Serial.print("brightness middle: ");
    Serial.println(this->brightness);
#endif

    if (this->brightness > 100) {
        this->brightness = 100;
    } else if (this->brightness < 1) {
        this->brightness = 1;
    }
#ifdef SHOULD_LOG_DIMMER
    Serial.print("Brightness end: ");
    Serial.println(this->brightness);
#endif

    this->power_state = true;

    esp_matter_attr_val_t power_value = esp_matter_invalid(NULL);
    attribute::get_val(this->power_attribute_ref, &power_value);

    power_value.val.b = this->power_state;

    attribute::update(
        *this->endpoint_id, 
        *this->power_cluster_id, 
        *this->power_attribute_id, 
        &power_value
    );

    esp_matter_attr_val_t level_value = esp_matter_invalid(NULL);
    attribute::get_val(this->level_attribute_ref, &level_value);

    level_value.val.u8 = (uint8_t) (this->brightness * 2.55);

    attribute::update(
        *this->endpoint_id, 
        *this->level_cluster_id, 
        *this->level_attribute_id, 
        &level_value
    );

#ifdef SHOULD_LED_EXAMPLE
    ledcWrite(this->ledChannel, int(this->brightness * 2.55));
#endif
}

void Dimmer::power_to_signal(bool new_power_state) {
#ifdef SHOULD_LOG_DIMMER
    Serial.print("Power to signal, before: ");
    Serial.print(this->power_state);
#endif

    this->power_state = new_power_state;

#ifdef SHOULD_LOG_DIMMER
    Serial.print(", after: ");
    Serial.println(this->power_state);
#endif

    digitalWrite(this->output_pin, HIGH);
    delay((this->ON_OFF_PUSH_MS - this->EPSILON_MS));
    digitalWrite(this->output_pin, LOW);

#ifdef SHOULD_LED_EXAMPLE
    ledcWrite(this->ledChannel, this->power_state ? int(this->brightness * 2.55) : 0);
#endif

    // esp_matter_attr_val_t power_value = esp_matter_invalid(NULL);
    // attribute::get_val(this->power_attribute_ref, &power_value);

    // power_value.val.b = this->power_state;

    // attribute::update(
    //     *this->endpoint_id, 
    //     *this->power_cluster_id, 
    //     *this->power_attribute_id, 
    //     &power_value
    // );
}

void Dimmer::brightness_to_signal(uint8_t new_brightness) {
    new_brightness = (uint8_t) (new_brightness / 2.55);
#ifdef SHOULD_LOG_DIMMER
    Serial.print("Brightness to signal, before: ");
    Serial.print(this->brightness);
#endif

    if (this->power_state == false) { // from off to new_brightness
       this->power_state = true;
       this->brightness = new_brightness;
       int timeToHold = this->HOLD_MS_FOR_EACH_PERCENTAGE * new_brightness;
       digitalWrite(this->output_pin, HIGH);
       delay(timeToHold);
       digitalWrite(this->output_pin, LOW);
       this->previous_LED_state = LEDState::UP;

#ifdef SHOULD_LED_EXAMPLE
        ledcWrite(this->ledChannel, int(this->brightness * 2.55));
#endif
    } else { // already on
        int actualBrightness = this->brightness;
        LEDState wantedState = new_brightness > actualBrightness ? LEDState::UP : LEDState::DOWN;
        
        if (wantedState == this->previous_LED_state) { // to same state as before -> double hold
            int timeLittleHold = ON_OFF_PUSH_MS + EPSILON_MS;
            int littleHoldPercentage = int(timeLittleHold / this->HOLD_MS_FOR_EACH_PERCENTAGE);

            digitalWrite(this->output_pin, HIGH);
            delay(timeLittleHold);
            digitalWrite(this->output_pin, LOW);

            if (this->previous_LED_state == LEDState::UP) {
                actualBrightness -= littleHoldPercentage;
            } else {
                actualBrightness += littleHoldPercentage;
            }
            // this->toggle_LED_state();
            // int timeToHold = this->HOLD_MS_FOR_EACH_PERCENTAGE * abs(new_brightness - actualBrightness);
            // digitalWrite(this->output_pin, HIGH);
            // delay(timeToHold);
            // digitalWrite(this->output_pin, LOW);
            // this->toggle_LED_state();
        }
         
        // to opposite state as before -> single hold
        this->toggle_LED_state();
        int timeToHold = (
            this->HOLD_MS_FOR_EACH_PERCENTAGE * 
            abs(new_brightness - actualBrightness)
        );
        digitalWrite(this->output_pin, HIGH);
        delay(timeToHold);
        digitalWrite(this->output_pin, LOW);
        this->toggle_LED_state();
        // this->previous_LED_state = wantedState;
    }
    
    this->brightness = new_brightness;

#ifdef SHOULD_LOG_DIMMER
    Serial.print(", after: ");
    Serial.println(this->brightness);
#endif

#ifdef SHOULD_LED_EXAMPLE
    ledcWrite(this->ledChannel, int(this->brightness * 2.55));
#endif

    // ??

    // digitalWrite(this->output_pin, HIGH);
    // // delay((this->ON_OFF_PUSH_MS - this->EPSILON_MS));
    // digitalWrite(this->output_pin, LOW);

    // esp_matter_attr_val_t level_value = esp_matter_invalid(NULL);
    // attribute::get_val(this->level_attribute_ref, &level_value);

    // level_value.val.u8 = (uint8_t) this->brightness;

    // attribute::update(
    //     *this->endpoint_id, 
    //     *this->level_cluster_id, 
    //     *this->level_attribute_id, 
    //     &level_value
    // );
}

void Dimmer::setup(
    const uint16_t *endpoint_id, 
    const cluster_t *level_cluster_id, 
    const attribute_t *level_attribute_id, 
    attribute_t *level_attribute_ref,
    const cluster_t *power_cluster_id, 
    const attribute_t *power_attribute_id,
    attribute_t *power_attribute_ref
) {
    this->endpoint_id = endpoint_id;
    this->level_cluster_id = level_cluster_id;
    this->level_attribute_id = level_attribute_id;
    this->level_attribute_ref = level_attribute_ref;
    this->power_cluster_id = power_cluster_id;
    this->power_attribute_id = power_attribute_id;
    this->power_attribute_ref = power_attribute_ref;

    pinMode(this->input_pin, INPUT_PULLDOWN);
    pinMode(this->output_pin, OUTPUT);

    digitalWrite(this->output_pin, LOW);

#ifdef SHOULD_LED_EXAMPLE
    ledcSetup(this->ledChannel, this->freq, this->resolution);
    ledcAttachPin(this->led_pin, this->ledChannel);
    ledcWrite(this->ledChannel, 0);
#endif
}

void Dimmer::loop() {
    int reading = digitalRead(this->input_pin);
    // send signal to the LED AC-DC converter
    digitalWrite(this->output_pin, reading);

    this->change_state(reading);
}

// PRIVATE

void Dimmer::change_state(int reading) {
// #ifdef SHOULD_LOG
//     Serial.print("Reading: ");
//     Serial.println(reading);
// #endif

    // button is clicked
    if (reading == HIGH) {
        
        if (this->button_state == NONE) { // button is being clicked for the first time
            this->button_start_millis = millis();
            this->button_state = ButtonState::PRESSED;
#ifdef SHOULD_LOG_DIMMER
            Serial.println("////////////////////////////////");
            Serial.println("Button begin to be pressed time");
#endif
        }

        if (this->button_state == PRESSED) {
            this->button_hold_time = millis() - this->button_start_millis; // time the button has been held
// #ifdef SHOULD_LOG
//             Serial.print("Button hold time: ");
//             Serial.println(button_hold_time);
// #endif
        }

    } else { // button is released
        if (this->button_state == PRESSED) {
            // button has been PUSHED, toggle the power, and update the state
            if (this->button_hold_time < (this->ON_OFF_PUSH_MS - this->EPSILON_MS)) {
#ifdef SHOULD_LOG_DIMMER
                Serial.print("Button has been pushed for: ");
                Serial.println(this->button_hold_time);
#endif
                this->toggle_power(!this->power_state);

            } else { // button has been HELD, update the state
#ifdef SHOULD_LOG_DIMMER
                Serial.print("Button has been held for: ");
                Serial.println(this->button_hold_time);
#endif
                this->set_brightness(this->button_hold_time);
                this->toggle_LED_state();

                // this->previous_LED_state = this->current_LED_state;
            }

            this->button_state = ButtonState::NONE; // reset the button state
            this->button_start_millis = 0; // reset the timer every time the button is released
            this->button_hold_time = 0; // reset the timer every time the button is released
        }

    }
}

void Dimmer::toggle_LED_state() {
    if (
        this->previous_LED_state == LEDState::DOWN
    ) {
        this->previous_LED_state = LEDState::UP;
        // this->current_LED_state = LEDState::UP;
    } else {
        this->previous_LED_state = LEDState::DOWN;
        // this->current_LED_state = LEDState::DOWN;
    }
}

// END DIMMER_CPP

// #include "Matter.h"
// #include <app/server/OnboardingCodesUtil.h>
// #include <credentials/examples/DeviceAttestationCredsExample.h>
// using namespace chip;
// using namespace chip::app::Clusters;
// using namespace esp_matter;
// using namespace esp_matter::endpoint;
// using namespace esp_matter::attribute;
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

Dimmer dimmer = Dimmer(DEFAULT_POWER, DEFAULT_BRIGHTNESS);

/* Do any conversions/remapping for the actual value here */
static esp_err_t app_driver_light_set_power(
    app_driver_handle_t handle, 
    esp_matter_attr_val_t *val
){
#ifdef SHOULD_LOG_DIMMER
    Serial.print("app_driver_light_set_power val: ");
    Serial.println(val->val.b);
#endif

    dimmer.power_to_signal(val->val.b);

    return ESP_OK;
}

static esp_err_t app_driver_light_set_brightness(
    app_driver_handle_t handle, 
    esp_matter_attr_val_t *val
) {
#ifdef SHOULD_LOG_DIMMER
    Serial.print("app_driver_light_set_brightness val: ");
    Serial.println(val->val.u8);
#endif

    dimmer.brightness_to_signal(val->val.u8);

    return ESP_OK;
}

esp_err_t app_driver_attribute_update(
    app_driver_handle_t handle, 
    uint16_t endpoint_id, 
    uint32_t cluster_id,
    uint32_t attribute_id, 
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
    uint32_t cluster_id,
    uint32_t attribute_id, 
    esp_matter_attr_val_t *val, 
    void *priv_data
) {
#ifdef SHOULD_LOG_DIMMER
    Serial.println("||||||||||||||||||||||||||||||||");
    Serial.println("on_attribute_update");
#endif
    esp_err_t err = ESP_OK;

    if (type == attribute::PRE_UPDATE && endpoint_id == dimmer_endpoint_id) {
#ifdef SHOULD_LOG_ESP
        Serial.println("PRE_UPDATE");
#endif
        app_driver_handle_t driver_handle = (app_driver_handle_t)priv_data;
        err = app_driver_attribute_update(driver_handle, endpoint_id, cluster_id, attribute_id, val);
    }
    return err;
}

// // Reads light attribute value
// esp_matter_attr_val_t get_attribute_value(attribute_t *attribute_id) {
//     esp_matter_attr_val_t value = esp_matter_invalid(NULL);
//     attribute::get_val(attribute_id, &value);
// #ifdef SHOULD_LOG_ESP
//     Serial.print("get_attribute_value");
//     Serial.println(value.val.u8);
// #endif
//     return value;
// }

// // Sets light attribute value
// void set_attribute_value(const cluster_t cluster_id, const attribute_t attribute_id, esp_matter_attr_val_t *value) {
//     attribute::update(dimmer_endpoint_id, cluster_id, attribute_id, value);
// }

void setup() {
    Serial.begin(115200);
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

    app_driver_light_set_defaults(dimmer_endpoint_id);

    // Print codes needed to setup Matter device
    PrintOnboardingCodes(chip::RendezvousInformationFlags(chip::RendezvousInformationFlag::kBLE));

    dimmer.setup(
        &dimmer_endpoint_id, 
        &LEVEL_CLUSTER_ID, 
        &LEVEL_ATTRIBUTE_ID, 
        level_attribute_ref,
        &POWER_CLUSTER_ID, 
        &POWER_ATTRIBUTE_ID,
        power_attribute_ref
    );
}

// When toggle light button is pressed (with debouncing),
// light attribute value is changed
void loop() {
    dimmer.loop();
}
