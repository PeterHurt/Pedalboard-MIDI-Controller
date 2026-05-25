#include <MIDIUSB.h>

// adjust these to suit your button matrix
const byte ROWS = 8;
const byte COLS = 4;

// adjust these to match the wiring to your Arduino
const byte rowPins[ROWS] = {6, 7, 8, 9, 10, 11, 12, 13};
const byte colPins[COLS] = {2, 3, 4, 5};

// mapping the standard MIDI note values to each point on the button matrix
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

bool keyState[ROWS][COLS]; // current state of each key

bool lastReading[ROWS][COLS]; // last raw reading

unsigned long lastDebounceTime[ROWS][COLS]; // debounce timer

const unsigned long debounceDelay = 15;

void setup() {
  Serial.begin(115200);

  // this logic seems inverse to the physical wiring

  // rows = inputs with the inbuilt Arduino pullup resistors
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

    digitalWrite(colPins[c], LOW); // activate one column

    delayMicroseconds(5);

    for (byte r = 0; r < ROWS; r++) {

      bool reading = (digitalRead(rowPins[r]) == LOW); // LOW when pressed

      // test if the reading has changed
      if (reading != lastReading[r][c]) {
        lastDebounceTime[r][c] = millis();
        lastReading[r][c] = reading;
      }

      // once stable
      if ((millis() - lastDebounceTime[r][c]) > debounceDelay) {

        // if the state change remains, trigger the appropriate MIDI function
        if (reading != keyState[r][c]) {

          keyState[r][c] = reading;

          if (reading) {
            noteOn(notes[r][c], 127);

            //Serial.print("ON  "); // uncomment these four lines for serial debugging
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