#include <FastLED.h>

// Define number of keys and LEDs
const int NUM_KEYS = 36; // Change according to your keyboard
const int NUM_LEDS = 25; // Match to the available number of LEDs

#define LED_PIN 2  // Pin connected to WS2812 strip

// Define base color and velocity offset color
const CRGB baseColor = CRGB(255, 255, 0);  // Yellow
const CRGB velocityOffsetColor = CRGB(255, 100, 0); // Reddish orange
const float baseBrightnessFactor = 0.05; // 25% brightness for default state

// LED strip setup
CRGB leds[NUM_LEDS];

//key pins
const int keyPins[12] = {14, 27, 26, 25, 23, 22, 21, 19, 18, 17, 16, 4};  // Key input pins array (12 keys)
const int lowerRailPin[3] = {33, 13, 5};  // Lower rail output pins (3 rails for 12 keys)
const int topRailPin[3] = {32, 12, 15};    // Top rail output pins (3 rails for 12 keys)
const int firstKeyCorrection = 36; //change this number to get the correct pitch note for your desired keyboard this changes by matrix

bool keyPressed[3][12] = {false};       // Track if each key is pressed
unsigned long pressStartTime[3][12];    // Store the time when each key first touches the lower rail
unsigned long pressTime[3][12];         // Store the time difference from lower to top rail for each key


// Time range for velocity mapping (microseconds)
const unsigned long minPressTime = 1990;  // Minimum time for max velocity (127)
const unsigned long maxPressTime = 329000;  // Maximum time for min velocity (0)

void setup() {
    Serial.begin(115200);
    FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = baseColor;
        leds[i].nscale8_video(255 * baseBrightnessFactor); // Dim to 25%
    }
    FastLED.show();

    // Initialize lower and top rail pins as outputs
    for (int i = 0; i < 3; i++) {
        pinMode(lowerRailPin[i], OUTPUT);
        pinMode(topRailPin[i], OUTPUT);
        digitalWrite(lowerRailPin[i], LOW);
        digitalWrite(topRailPin[i], LOW);
    }

    // Initialize key input pins
    for (int i = 0; i < 12; i++) {
        pinMode(keyPins[i], INPUT_PULLDOWN);  // Use pull-down for stable readings on the key input pins
    }
}

void loop() {
    // Iterate through the rails (3 rails for 12 keys)
    for (int rail = 0; rail < 3
    ; rail++) {
        // Step 1: Activate the current lower rail
        digitalWrite(lowerRailPin[rail], HIGH);
        delayMicroseconds(10);  // Stabilizing delay

        // Step 2: Check all keys connected to this rail
        for (int key = 0; key < 12; key++) {
            int keyIndex = rail * 12 + key;  // Calculate the key index (0-11)
            int ledIndex = map(keyIndex, 0, NUM_KEYS - 1, 0, NUM_LEDS - 1); // Map keys to LEDs

            // Check if the key is on the lower rail
            bool isOnLowerRail = digitalRead(keyPins[key]);

            // If the key is on the lower rail and not already pressed, record the start time
            if (isOnLowerRail && !keyPressed[rail][key]) {
                pressStartTime[rail][key] = micros();  // Store the time when the key first touches the lower rail
            }
        }

        // Step 3: Deactivate the lower rail
        digitalWrite(lowerRailPin[rail], LOW);

        // Step 4: Activate the current top rail
        digitalWrite(topRailPin[rail], HIGH);
        delayMicroseconds(10);  // Stabilizing delay

        // Step 5: Check all keys connected to this rail
        for (int key = 0; key < 12; key++) {
            int keyIndex = rail * 12 + key;  // Calculate the key index (0-11)
            int ledIndex = map(keyIndex, 0, NUM_KEYS - 1, 0, NUM_LEDS - 1); // Map keys to LEDs

            // Check if the key is on the top rail
            bool isOnTopRail = digitalRead(keyPins[key]);

            // If the key is on the top rail and was not pressed before, calculate the press time and velocity
            if (isOnTopRail && !keyPressed[rail][key]) {
                pressTime[rail][key] = micros() - pressStartTime[rail][key];  // Calculate how long it took to press

                // Map the press time to velocity range 0-127
                int velocity = map(pressTime[rail][key], minPressTime, maxPressTime, 127, 0);
                velocity = constrain(velocity, 0, 127);  // Ensure the velocity stays within the range

                // Print the key number and calculated velocity
                // Serial.print("Key ");
                // Serial.print(12*rail + key + 1);  // Key number (1-indexed)
                // Serial.print(" Pressed, Velocity: ");
                // Serial.println(velocity);

                sendMidiNoteOn(12*rail + key + 1, velocity);
                keyPressed[rail][key] = true;  // Set the flag to true so it won't print again until released

                // Update the LED color and brightness based on the velocity
                setKeyColor(ledIndex, velocity);
                FastLED.show(); // Ensure update
            }

            // If the key is not on the top rail and was previously pressed, reset the flag
            if (!isOnTopRail && keyPressed[rail][key]) {
                keyPressed[rail][key] = false;  // Reset the flag once the key is released
                sendMidiNoteOff(12*rail + key + 1, 0);

                resetKeyColor(ledIndex);
                FastLED.show(); // Ensure update
            }
        }

        // Step 6: Deactivate the top rail
        digitalWrite(topRailPin[rail], LOW);
    }

    // Small delay to prevent flooding
    delay(10);

}

void setKeyColor(int ledIndex, int velocity) {
    if (ledIndex < NUM_LEDS) {
        CRGB newColor = baseColor;
        nblend(newColor, velocityOffsetColor, velocity * 2);
        leds[ledIndex] = newColor; // Keep full brightness for key press
    }
}

void resetKeyColor(int ledIndex) {
    if (ledIndex < NUM_LEDS) {
        leds[ledIndex] = baseColor;
        leds[ledIndex].nscale8_video(255 * baseBrightnessFactor); // Return to dimmed state
    }
}

void sendMidiNoteOn(int note, int velocity) {
  byte midiMessage[3];
  midiMessage[0] = 0x90;  // Note On message (Channel 1)
  midiMessage[1] = note + firstKeyCorrection ;   // Note number (0-127)
  midiMessage[2] = velocity; // Velocity (0-127)
  
  // Send MIDI Note On message via Serial
  Serial.write(midiMessage, 3);
}

void sendMidiNoteOff(int note, int velocity) {
  byte midiMessage[3];
  midiMessage[0] = 0x80;  // Note Off message (Channel 1)
  midiMessage[1] = note + firstKeyCorrection;   // Note number (0-127)
  midiMessage[2] = velocity; // Velocity (0-127)
  
  // Send MIDI Note Off message via Serial
  Serial.write(midiMessage, 3);
}
