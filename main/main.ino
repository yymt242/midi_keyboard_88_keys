#include <MIDI.h>
MIDI_CREATE_DEFAULT_INSTANCE();

// Define constants for the number of rows, columns, keys, potentiometers, buttons, and switches.
#define NUM_ROWS 8
#define NUM_COLS 24
#define NUM_KEYS 88
#define NUM_POTS 4
#define NUM_BUTTONS 2
#define NUM_SWITCHES 2

// Pin assignments for rows, columns, potentiometers, buttons, and switches.
const byte rowPins[NUM_ROWS] = {26, 27, 28, 29, 30, 31, 32, 33};
const byte colPins[NUM_COLS] = {34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, A4, A5, A6, A7};
const byte potentiometerPins[NUM_POTS] = {A0, A1, A2, A3};
const byte buttonPins[NUM_BUTTONS] = {22, 23};
const byte switchPins[NUM_SWITCHES] = {24, 25};

// Global variables for current MIDI channel and key states.
byte currentChannel = 1;
byte keyStates[NUM_KEYS] = {0};
byte prevKeyStates[NUM_KEYS] = {0};
byte velKeyStates[NUM_KEYS] = {0};
byte prevVelKeyStates[NUM_KEYS] = {0};
unsigned long keyPressTime[NUM_KEYS] = {0};
unsigned long debounceKey[NUM_KEYS] = {0};
unsigned long debounceVelKey[NUM_KEYS] = {0};
const byte debounceThreshold = 2;

bool noteOnSent[NUM_KEYS] = {false};

struct sustainingNotes {
        byte note;
        byte channel;
        unsigned long startTime;
};

sustainingNotes sustainNotes[100] = {0};

// Variables for potentiometer readings and smoothing.
int potReadings[NUM_POTS][6] = {0};
int potPrevReadings[NUM_POTS] = {0};
byte potIndex[NUM_POTS] = {0};

// Variables for sustain and potentiometer enable states.
bool sustainEnabled = false;
bool potsEnabled = true;

// Variables for velocity settings.
int velocityMax = 127;
bool velocityMaxEnabled = false;
int releaseTime = 8000;

// Variables for button states.
bool buttonUp = false;
bool buttonDown = false;
bool buttonPrevUp = false;
bool buttonPrevDown = false;

/**
 * @brief Arduino setup function.
 *
 * This function initializes MIDI communication, serial communication,
 * and configures the pin modes for rows, columns, buttons, switches, and potentiometers.
 */
void setup() {
        MIDI.begin(MIDI_CHANNEL_OMNI);
        Serial.begin(9600);

        for (byte i = 0; i < NUM_ROWS; i++)
                pinMode(rowPins[i], INPUT);
        for (byte i = 0; i < NUM_COLS; i++)
                pinMode(colPins[i], INPUT);
        for (byte i = 0; i < NUM_BUTTONS; i++)
                pinMode(buttonPins[i], INPUT);
        for (byte i = 0; i < NUM_SWITCHES; i++)
                pinMode(switchPins[i], INPUT);
        for (byte i = 0; i < NUM_SWITCHES; i++)
                pinMode(potentiometerPins[i], INPUT);
}

/**
 * @brief Scan the keys on the keyboard.
 *
 * This function iterates over each column and row, setting the appropriate
 * pin modes and reading the state of each key. It handles both velocity sensitive
 * keys and standard keys based on the row.
 */
void scanKeys() {
        for (byte col = 0; col < NUM_COLS; col++) {
                pinMode(colPins[col], OUTPUT);
                digitalWrite(colPins[col], LOW);

                for (byte row = 0; row < NUM_ROWS; row++) {
                        pinMode(rowPins[row], INPUT_PULLUP);
                        // Calculate the key index = MIDI key number - 21
                        byte keyIndex = keyMap(row % 4, col) - 21;

                        // Skip if keyIndex is out of bounds.
                        if (keyIndex < 0 || keyIndex >= NUM_KEYS)
                                continue;

                        // Handle velocity-sensitive keys if in the first 4 rows, else normal key press.
                        if (row < 4) {
                                // Velocity-sensitive key
                                velKeyStates[keyIndex] = !digitalRead(rowPins[row]);
                                if (velKeyStates[keyIndex] != prevVelKeyStates[keyIndex]) {
                                        prevVelKeyStates[keyIndex] = velKeyStates[keyIndex];
                                        if (velKeyStates[keyIndex] == 1) {
                                                unsigned long pressDuration = millis() - keyPressTime[keyIndex];
                                                byte velocity = calculateVelocity(pressDuration);
                                                int note = keyIndex + 21;
                                                Serial.println("Velo pressed - Sending note on");
                                                // MIDI.sendNoteOn(note, velocity, currentChannel);
                                        } else {
                                                Serial.println("Velocity key released");
                                        }
                                }
                        }

                        else {
                                // Normal key press
                                keyStates[keyIndex] = !digitalRead(rowPins[row]);
                                if (keyStates[keyIndex] != prevKeyStates[keyIndex]) {
                                        prevKeyStates[keyIndex] = keyStates[keyIndex];
                                        if (keyStates[keyIndex] == 1) {
                                                int note = keyIndex + 21;
                                                keyPressTime[keyIndex] = millis();
                                                Serial.println("--------------------------");
                                                Serial.println("Normal key pressed - Recorded key press TIME");
                                        } else {
                                                int note = keyIndex + 21;
                                                Serial.println("Normal key released - Sending note off");
                                                // MIDI.sendNoteOff(note, 0, currentChannel);
                                        }
                                }
                        }
                }
                digitalWrite(colPins[col], HIGH);
        }
}

/**
 * @brief Handle a normal key press.
 *
 * This function updates the state of a key and sets a debounce timer.
 * It records the time of key press and resets the noteOnSent flag.
 *
 * @param keyIndex Index of the key.
 * @param keyState Current state of the key (pressed or not).
 */
void handleKeyPress(byte keyIndex, byte keyState) {
        if (keyStates[keyIndex] != keyState && millis() - debounceKey[keyIndex] >= debounceThreshold) {
                keyStates[keyIndex] = keyState;
                debounceKey[keyIndex] = millis();
                if (keyState == 1) {
                        /*
                        keyPressTime[keyIndex] = millis();
                        noteOnSent[keyIndex] = false;
                        */
                } else {
                }
        }
}

/**
 * @brief Handle a velocity-sensitive key press.
 *
 * This function calculates the velocity of the key press based on the press duration
 * and sends a MIDI Note On message if the note hasn't already been sent.
 *
 * @param keyIndex Index of the key.
 * @param keyState Current state of the key (pressed or not).
 */
void handleVelocityKeyPress(byte keyIndex, byte keyState) {
        if (velKeyStates[keyIndex] != keyState && millis() - debounceVelKey[keyIndex] >= debounceThreshold) {
                velKeyStates[keyIndex] = keyState;
                debounceVelKey[keyIndex] = millis();
                Serial.println(keyState);
                if (keyState == 1) {

                        /*
                        unsigned long pressDuration = millis() - keyPressTime[keyIndex];
                        int note = keyIndex + 21;

                        // Stop all same sustaining notes when new note is played.
                        for (byte i = 0; i < 100; i++) {
                                if (sustainNotes[i].note == note) {
                                        MIDI.sendNoteOff(note, 0, sustainNotes[i].channel);
                                        sustainNotes[i].channel = 0;
                                        sustainNotes[i].note = 0;
                                        sustainNotes[i].startTime = 0;
                                }
                        }

                        byte velocity = calculateVelocity(pressDuration);
                        if (!noteOnSent[keyIndex]) {
                                MIDI.sendNoteOn(note, velocity, currentChannel);
                                noteOnSent[keyIndex] = true;

                                if (sustainEnabled) {
                                        for (byte i = 0; i < 100; i++) {
                                                if (sustainNotes[i].channel == 0) {
                                                        sustainNotes[i].note = note;
                                                        sustainNotes[i].channel = currentChannel;
                                                        sustainNotes[i].startTime = millis();
                                                        break;
                                                }
                                        }
                                }
                        }
                        */
                } else {
                }
        }
}

/**
 * @brief Send MIDI messages for key releases and handle sustain.
 *
 * This function sends a MIDI Note Off message when a key is released,
 * and it also automatically releases notes based on the sustain state
 * and the release time.
 */
void sendMIDIMessages() {
        /*
        for (byte i = 0; i < NUM_KEYS; i++) {
                int note = i + 21;

                // Check if key state has changed.
                if (keyStates[i] != prevKeyStates[i]) {
                        // If key is released and note was on, send a Note Off message.
                        if (keyStates[i] == 0 && noteOnSent[i]) {
                                if (!sustainEnabled) {
                                        MIDI.sendNoteOff(note, 0, currentChannel);
                                        noteOnSent[i] = false;
                                }
                        }
                        prevKeyStates[i] = keyStates[i];
                }

                // Check if there are any note still somehow sustaining after released
        }

        // Automatically send Note Off if the key has been held for longer than releaseTime.
        for (byte j = 0; j < 100; j++) {
                if ((sustainNotes[j].channel != 0) && (millis() - sustainNotes[j].startTime >= releaseTime)) {
                        MIDI.sendNoteOff(sustainNotes[j].note, 0, sustainNotes[j].channel);
                        sustainNotes[j].channel = 0;
                        sustainNotes[j].note = 0;
                        sustainNotes[j].startTime = 0;
                }
        }
        */
}

/**
 * @brief Map the row and column to MIDI key number
 *
 * This function calculates a key number based on the row and column,
 * applying offsets and handling special cases.
 *
 * @param row The row number.
 * @param col The column number.
 * @return The MIDI key number or -1 if invalid.
 */
byte keyMap(byte row, byte col) {
        if ((row == 0 && col == 0) || (row == 3 && (col > 20 || (col > 3 && col < 8))))
                return -1;
        return col < 8 ? row * 8 + col + 20 : (col < 16 ? row * 8 + col - 8 + 48 : row * 8 + col - 16 + 80);
}

/**
 * @brief Calculate the velocity of a key press.
 *
 * This function computes the velocity based on how fast the key was pressed,
 * using logarithmic scaling and a power function. It also applies a maximum velocity
 * if enabled.
 *
 * @param pressSpeed Duration of the key press. (0 to 400+ miliseconds)
 * @return Calculated velocity (1-127).
 */
byte calculateVelocity(unsigned long pressSpeed) {
        if (velocityMaxEnabled && potsEnabled)
                return velocityMax;

        const unsigned long minTime = 4;
        const unsigned long maxTime = 432;

        if (pressSpeed <= minTime)
                return 127;
        if (pressSpeed >= maxTime)
                return 1;

        float normalizedLog = log(pressSpeed - 3) / log(250);
        float scaledValue = pow(normalizedLog, 1.75);
        byte velocity = 127 - (107 * scaledValue);

        return velocity;
}

/**
 * @brief Read and process potentiometer values.
 *
 * This function reads the potentiometers, calculates an average reading,
 * maps it to a 0-127 range, and sends MIDI Control Change messages when the
 * reading changes.
 */
void readPotentiometers() {
        if (!potsEnabled)
                return;
        for (byte i = 0; i < NUM_POTS; i++) {
                potReadings[i][potIndex[i]] = analogRead(potentiometerPins[i]);
                potIndex[i] = (potIndex[i] + 1) % 6;
                int avg = 0;
                for (byte j = 0; j < 6; j++)
                        avg += potReadings[i][j];
                avg /= 6;
                avg = map(avg, 0, 1023, 0, 127);

                if (avg != potPrevReadings[i]) {
                        potPrevReadings[i] = avg;

                        if (i < 2) {
                                avg = 127 - avg;
                                MIDI.sendControlChange(10 + i, avg, currentChannel);
                        } else if (i == 3)
                                MIDI.sendControlChange(10 + i, avg, currentChannel);
                        else {
                                if (avg == 0) {
                                        velocityMaxEnabled = false;
                                } else {
                                        velocityMax = avg;
                                        velocityMaxEnabled = true;
                                }
                        }
                }
        }
}

/**
 * @brief Check the state of the switches.
 *
 * This function updates the sustain and potentiometer enable states based on
 * the readings from the switches.
 */
void checkSwitches() {
        sustainEnabled = !digitalRead(switchPins[0]);
        potsEnabled = !digitalRead(switchPins[1]);
}

/**
 * @brief Check the state of the buttons and update the MIDI channel.
 *
 * This function reads the buttons to increment or decrement the current MIDI channel.
 * It also handles debouncing for the buttons.
 */
void checkButtons() {
        buttonUp = !digitalRead(buttonPins[1]);
        buttonDown = !digitalRead(buttonPins[0]);

        if (buttonUp && !buttonPrevUp) {
                currentChannel++;
                if (currentChannel > 16)
                        currentChannel = 1;
        }

        if (buttonDown && !buttonPrevDown) {
                currentChannel--;
                if (currentChannel < 1)
                        currentChannel = 16;
        }

        buttonPrevUp = buttonUp;
        buttonPrevDown = buttonDown;
}

/**
 * @brief Scan keys to select the MIDI channel.
 *
 * This function scans the keyboard for keys that indicate a channel change.
 * If a key corresponding to a channel between 27 and 42 is pressed, it sets the
 * current channel accordingly.
 *
 * @return The selected MIDI channel.
 */
byte scanSelectChannel() {
        byte channel = currentChannel;
        for (byte col = 0; col < NUM_COLS; col++) {
                pinMode(colPins[col], OUTPUT);
                digitalWrite(colPins[col], LOW);

                for (byte row = 0; row < NUM_ROWS; row++) {
                        pinMode(rowPins[row], INPUT_PULLUP);

                        byte keyState = !digitalRead(rowPins[row]);
                        byte keyIndex = keyMap(row % 4, col) - 21;

                        // Check if key index corresponds to a channel selection key.
                        if (keyIndex >= 27 && keyIndex < (27 + 16))
                                if (keyState == 1) {
                                        channel = keyIndex - 26;
                                }
                }
                digitalWrite(colPins[col], HIGH);
        }

        return channel;
}

/**
 * @brief Main Arduino loop.
 *
 * This function continuously checks for button presses, scans keys,
 * sends MIDI messages, reads potentiometers, and checks switches.
 * If both channel selection buttons are pressed, it scans for channel selection.
 */
void loop() {
        checkButtons();
        if (buttonUp && buttonDown) {
                currentChannel = scanSelectChannel();
        } else {
                scanKeys();
                sendMIDIMessages();
                readPotentiometers();
                checkSwitches();
        }
}
