#include <MIDIUSB.h>

const byte ROWS = 8;
const byte COLS = 4;

const byte rowPins[ROWS] = {6, 7, 8, 9, 10, 11, 12, 13};
const byte colPins[COLS] = {2, 3, 4, 5};

const byte notes[ROWS][COLS] = {
  {36, 44, 52, 60},
  {37, 45, 53, 61},
  {38, 46, 54, 62},
  {39, 47, 55, 63},
  {40, 48, 56, 64},
  {41, 49, 57, 65},
  {42, 50, 58, 66},
  {43, 51, 59, 67}
};

// current stable state of each key
bool keyState[ROWS][COLS];

// last raw reading
bool lastReading[ROWS][COLS];

// debounce timer
unsigned long lastDebounceTime[ROWS][COLS];

const unsigned long debounceDelay = 15;

void setup() {
  Serial.begin(115200);

  // rows = inputs with pullups
  for (byte r = 0; r < ROWS; r++) {
    pinMode(rowPins[r], INPUT_PULLUP);
  }

  // cols = outputs
  for (byte c = 0; c < COLS; c++) {
    pinMode(colPins[c], OUTPUT);
    digitalWrite(colPins[c], HIGH);
  }
}

void loop() {
  scanMatrix();
}

void scanMatrix() {

  for (byte c = 0; c < COLS; c++) {

    // activate one column
    digitalWrite(colPins[c], LOW);

    delayMicroseconds(5);

    for (byte r = 0; r < ROWS; r++) {

      // LOW means pressed
      bool reading = (digitalRead(rowPins[r]) == LOW);

      // reading changed?
      if (reading != lastReading[r][c]) {
        lastDebounceTime[r][c] = millis();
        lastReading[r][c] = reading;
      }

      // stable long enough?
      if ((millis() - lastDebounceTime[r][c]) > debounceDelay) {

        // actual state changed?
        if (reading != keyState[r][c]) {

          keyState[r][c] = reading;

          if (reading) {
            noteOn(notes[r][c], 127);

            //Serial.print("ON  ");
            //Serial.println(notes[r][c]);

          } else {
            noteOff(notes[r][c], 0);

            //Serial.print("OFF ");
            //Serial.println(notes[r][c]);
          }
        }
      }
    }

    // deactivate column
    digitalWrite(colPins[c], HIGH);
  }
}

void noteOn(byte pitch, byte velocity) {
  midiEventPacket_t event = {
    0x09,
    0x90,
    pitch,
    velocity
  };

  MidiUSB.sendMIDI(event);
  MidiUSB.flush();
}

void noteOff(byte pitch, byte velocity) {
  midiEventPacket_t event = {
    0x08,
    0x80,
    pitch,
    velocity
  };

  MidiUSB.sendMIDI(event);
  MidiUSB.flush();
}