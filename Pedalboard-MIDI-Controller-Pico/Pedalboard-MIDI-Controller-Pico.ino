#include <Adafruit_TinyUSB.h>
#include "pico/multicore.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"

// ============================================================
// CONFIG
// ============================================================

constexpr uint8_t ROWS = 8;
constexpr uint8_t COLS = 4;
constexpr uint8_t KEYS = 30;

constexpr uint8_t rowPins[ROWS] = {6,7,8,9,10,11,12,13};
constexpr uint8_t colPins[COLS] = {2,3,4,5};
constexpr uint8_t keysPerCol[COLS] = {8,8,8,6};

constexpr uint8_t BASE_NOTE = 36;

// ============================================================
// MIDI
// ============================================================

Adafruit_USBD_MIDI usb_midi;

struct MidiEvent {
  uint8_t status;
  uint8_t note;
  uint8_t velocity;
};

// ============================================================
// LOCK-FREE RING BUFFER
// ============================================================

constexpr uint16_t QSIZE = 128;

MidiEvent queue[QSIZE];

volatile uint16_t qHead = 0;
volatile uint16_t qTail = 0;

inline bool push(const MidiEvent &e)
{
  uint16_t n = (qHead + 1) & (QSIZE - 1);
  if (n == qTail) return false;

  queue[qHead] = e;
  qHead = n;
  return true;
}

inline bool pop(MidiEvent &e)
{
  if (qHead == qTail) return false;

  e = queue[qTail];
  qTail = (qTail + 1) & (QSIZE - 1);
  return true;
}

// ============================================================
// STATE
// ============================================================

uint32_t stable = 0;
uint8_t debounce[KEYS] = {0};

volatile bool scanFlag = false;
volatile uint32_t frameTick = 0;

// ============================================================
// TIMERS
// ============================================================

bool scan_timer_cb(repeating_timer_t*)
{
  scanFlag = true;
  return true;
}

bool usb_frame_cb(repeating_timer_t*)
{
  frameTick++;
  return true;
}

// ============================================================
// SCAN MATRIX (FIXED)
// ============================================================

void scanMatrix()
{
  uint8_t key = 0;

  for (uint8_t c = 0; c < COLS; c++)
  {
    gpio_put(colPins[c], 0);

    // IMPORTANT: long-wire + diode settling time
    busy_wait_us_32(8);

    for (uint8_t r = 0; r < keysPerCol[c]; r++)
    {
      if (key >= KEYS)
      {
        key++; // still advance to preserve mapping safety
        continue;
      }

      bool rawPressed = !gpio_get(rowPins[r]);

      // =====================================================
      // DEBOUNCE (HYSTERESIS FILTER)
      // =====================================================

      if (rawPressed)
      {
        if (debounce[key] < 3) debounce[key]++;
      }
      else
      {
        if (debounce[key] > 0) debounce[key]--;
      }

      bool pressed = debounce[key] >= 2;

      uint32_t mask = (1u << key);
      bool was = stable & mask;

      if (pressed && !was)
      {
        stable |= mask;
        push({0x90, (uint8_t)(BASE_NOTE + key), 127});
      }
      else if (!pressed && was)
      {
        stable &= ~mask;
        push({0x80, (uint8_t)(BASE_NOTE + key), 0});
      }

      key++;
    }

    gpio_put(colPins[c], 1);
  }
}

// ============================================================
// CORE 1 (scanner)
// ============================================================

void core1_main()
{
  while (true)
  {
    if (!scanFlag)
    {
      tight_loop_contents();
      continue;
    }

    scanFlag = false;
    scanMatrix();
  }
}

// ============================================================
// SETUP
// ============================================================

repeating_timer_t scanTimer;
repeating_timer_t usbFrameTimer;

void setup()
{
  TinyUSBDevice.begin();
  usb_midi.begin();

  // GPIO rows
  for (auto p : rowPins)
  {
    gpio_init(p);
    gpio_set_dir(p, GPIO_IN);
    gpio_pull_up(p);
  }

  // GPIO columns
  for (auto p : colPins)
  {
    gpio_init(p);
    gpio_set_dir(p, GPIO_OUT);
    gpio_put(p, 1);
  }

  // 2kHz scan
  add_repeating_timer_us(-500, scan_timer_cb, nullptr, &scanTimer);

  // 1kHz USB frame tick
  add_repeating_timer_us(-1000, usb_frame_cb, nullptr, &usbFrameTimer);

  multicore_launch_core1(core1_main);
}

// ============================================================
// CORE 0 (USB MIDI FLUSH)
// ============================================================

void loop()
{
  static uint32_t lastFrame = 0;

  if (frameTick != lastFrame)
  {
    lastFrame = frameTick;

    MidiEvent e;
    while (pop(e))
    {
      uint8_t msg[3] = { e.status, e.note, e.velocity };
      usb_midi.write(msg, 3);
    }

    usb_midi.flush();
  }

  tud_task();
}