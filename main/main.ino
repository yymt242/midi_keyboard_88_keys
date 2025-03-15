#include <MIDI.h>
MIDI_CREATE_DEFAULT_INSTANCE();

#define NUM_ROWS 8
#define NUM_COLS 24
#define NUM_KEYS 88
#define NUM_POTS 4
#define NUM_BUTTONS 2
#define NUM_SWITCHES 2
#define SUS_MAX 88

const byte rowPins[NUM_ROWS] = {26, 27, 28, 29, 30, 31, 32, 33};
const byte colPins[NUM_COLS] = {34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45,
                                46, 47, 48, 49, 50, 51, 52, 53, A4, A5, A6, A7};
const byte potentiometerPins[NUM_POTS] = {A0, A1, A2, A3};
const byte buttonPins[NUM_BUTTONS] = {22, 23};
const byte switchPins[NUM_SWITCHES] = {24, 25};

// Channel selection
bool buttonUp = false;
bool buttonDown = false;
bool buttonPrevUp = false;
bool buttonPrevDown = false;
byte currentChannel = 1;

// Key settings
byte firstKey[NUM_KEYS] = {0};
byte secondKey[NUM_KEYS] = {0};
unsigned long timestamp[NUM_KEYS] = {1};
bool timeRead[NUM_KEYS] = {false};

// Potentiometer settings
bool potsEnabled = true;
int potReadings[NUM_POTS][6] = {0};
int potPrevReadings[NUM_POTS] = {0};
byte potIndex[NUM_POTS] = {0};

// Sustain settings
bool sustainEnabled = false;
int sustainTime = 8000;

struct sustainingNotes {
    byte channel;
    unsigned long startTime;
};

sustainingNotes sustainNotes[SUS_MAX] = {0};

// Velocity settings
int velocityMax = 127;
bool velocityMaxEnabled = false;

void setup()
{
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
    for (byte i = 0; i < NUM_POTS; i++)
        pinMode(potentiometerPins[i], INPUT);
}

void scanKeys()
{
    for (byte col = 0; col < NUM_COLS; col++) {
        pinMode(colPins[col], OUTPUT);
        digitalWrite(colPins[col], LOW);

        for (byte row = 0; row < NUM_ROWS; row++) {
            pinMode(rowPins[row], INPUT_PULLUP);

            /* START KEY READING */
            int note = keyMap(row % 4, col);
            byte keyIndex = note - 21; // Array index = MIDI note - 21
            if (keyIndex < 0 || keyIndex >= NUM_KEYS)
                continue;

            byte reading = !digitalRead(rowPins[row]);
            // Second button: control Note ON/OFF
            if (row < 4) {
                if (reading != secondKey[keyIndex]) {
                    secondKey[keyIndex] = reading;
                    if (reading == 1) {
                        // Send Note On
                        if (sustainNotes[keyIndex].startTime != 0) {
                            MIDI.sendNoteOff(note, 0,
                                             sustainNotes[keyIndex].channel);
                        }
                        if (sustainEnabled) {
                            sustainNotes[keyIndex].channel = currentChannel;
                            sustainNotes[keyIndex].startTime = millis();
                        }
                        if (timeRead[keyIndex]) {
                            unsigned long pressDuration =
                                millis() - timestamp[keyIndex];
                            byte velocity = calculateVelocity(pressDuration);
                            MIDI.sendNoteOn(note, velocity, currentChannel);
                            timeRead[keyIndex] = false;
                        } else {
                            MIDI.sendNoteOn(note, 60, currentChannel);
                            timestamp[keyIndex] = millis();
                            timeRead[keyIndex] = true;
                        }
                    } else {
                        // Send Note Off
                        if (!sustainEnabled) {
                            MIDI.sendNoteOff(note, 0, currentChannel);
                        }
                    }
                }
            }

            // First button: set timestamp
            else {
                if (reading != firstKey[keyIndex]) {
                    firstKey[keyIndex] = reading;
                    if (reading == 1) {
                        timestamp[keyIndex] = millis();
                        timeRead[keyIndex] = true;
                    }
                }
            }
            /* END KEY READING */
        }

        digitalWrite(colPins[col], HIGH);
    }
}

void sustainNoteOff()
{
    for (int i = 0; i < SUS_MAX; i++) {
        if (sustainNotes[i].startTime != 0) {
            unsigned long elapsedTime = millis() - sustainNotes[i].startTime;
            if (elapsedTime >= sustainTime) {
                MIDI.sendNoteOff(i + 21, 0, sustainNotes[i].channel);
                sustainNotes[i].startTime = 0;
            }
        }
    }
}

byte keyMap(byte row, byte col)
{
    if ((row == 0 && col == 0) ||
        (row == 3 && (col > 20 || (col > 3 && col < 8))))
        return -1;
    return col < 8
               ? row * 8 + col + 20
               : (col < 16 ? row * 8 + col - 8 + 48 : row * 8 + col - 16 + 80);
}

byte calculateVelocity(unsigned long pressSpeed)
{
    if (velocityMaxEnabled && potsEnabled)
        return velocityMax;

    const unsigned long minTime = 4;
    const unsigned long maxTime = 432;

    if (pressSpeed <= minTime)
        return 127;
    if (pressSpeed >= maxTime)
        return 1;

    float b = 2;
    float a = 700;
    float normalizedLog = log(pressSpeed - 2) / log(a - 200);
    float scaledValue = pow(normalizedLog, b);
    scaledValue = 129 - (107 * scaledValue);
    scaledValue *= 1.08;
    scaledValue *= (log(400 - pressSpeed) / log(a));
    byte velocity = (byte)scaledValue;
    velocity = constrain(velocity, 1, 127);

    return velocity;
}

void readPotentiometers()
{
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

void checkSwitches()
{
    sustainEnabled = !digitalRead(switchPins[0]);
    potsEnabled = !digitalRead(switchPins[1]);
}

void checkButtons()
{
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

byte scanSelectChannel()
{
    byte channel = currentChannel;
    for (byte col = 0; col < NUM_COLS; col++) {
        pinMode(colPins[col], OUTPUT);
        digitalWrite(colPins[col], LOW);

        for (byte row = 0; row < NUM_ROWS; row++) {
            pinMode(rowPins[row], INPUT_PULLUP);

            byte keyState = !digitalRead(rowPins[row]);
            byte keyIndex = keyMap(row % 4, col) - 21;

            if (keyIndex >= 27 && keyIndex < (27 + 16))
                if (keyState == 1) {
                    channel = keyIndex - 26;
                }
        }
        digitalWrite(colPins[col], HIGH);
    }

    return channel;
}

void loop()
{
    checkButtons();
    if (buttonUp && buttonDown) {
        currentChannel = scanSelectChannel();
    } else {
        scanKeys();
        sustainNoteOff();
        readPotentiometers();
        checkSwitches();
    }
}
