/*
  ============================================================
  MULTI-FIRMWARE CLOCK MODULE
  ============================================================

  Firmware switching:
  - Hold RESET + CLOCK together for 5 seconds
  - Active firmware is saved to EEPROM and restored on power-up

  ------------------------------------------------------------
  Firmware 0: Ring Counter
  ------------------------------------------------------------
  - One active output at a time
  - Advances to the next output on each clock pulse
  - RESET returns to output 0

  ------------------------------------------------------------
  Firmware 1: Divider / Polymeter
  ------------------------------------------------------------
  - Independent clock divisions per output
  - RESET clears all counters

  ------------------------------------------------------------
  Firmware 2: 24 PPQN Polyrhythm Divider
  ------------------------------------------------------------
  - Designed for 24 PPQN clock input
  - RESET realigns all phases

  ------------------------------------------------------------
  Firmware 3: 24 PPQN Power-of-Two Slow Divider
  ------------------------------------------------------------
  - 24 PPQN -> /1,/2,/4,/8,/16,/32,/64,/128 (in quarter-notes)
  - RESET clears counters

  ------------------------------------------------------------
  Firmware 4: Classic Clock Divider
  ------------------------------------------------------------
  - Divides raw clock pulses by: 2,4,8,16,32,64,128
  - Output 7 = /1 passthrough
  - RESET clears counters

  ------------------------------------------------------------
  Firmware 5: 8-bit Binary Counter (steady outputs)
  ------------------------------------------------------------
  - Counts 0..255, increments on CLOCK rising edge (LOW->HIGH with INPUT_PULLUP clock)
  - Outputs represent the count in binary (steady HIGH/LOW):
    OUT0=1, OUT1=2, OUT2=4, OUT3=8, OUT4=16, OUT5=32, OUT6=64, OUT7=128
  - RESET sets count to 0

  ------------------------------------------------------------
  Firmware 6: 8-bit Binary Counter (pulsed outputs)
  ------------------------------------------------------------
  - Counts 0..255, increments on CLOCK rising edge
  - Outputs are short pulses when their bit changes 0->1

  ------------------------------------------------------------
  Firmware 7: 8-bit Gray Code Counter (steady outputs)
  ------------------------------------------------------------
  - Counts 0..255, increments on CLOCK rising edge
  - Outputs show Gray code (only ONE bit changes per step)
  - RESET sets count to 0


  ------------------------------------------------------------
  Firmware 8: 8-bit Binary Counter (pulsed outputs, "used bits")
  ------------------------------------------------------------
  - Counts 0..255, increments on CLOCK rising edge
  - On every step, each output whose bit is 1 in the NEW count produces a short pulse
    (so multiple outputs can pulse on the same clock tick)
  - RESET sets count to 0

  ============================================================
*/

#include <Arduino.h>
#include <EEPROM.h>

// Uncomment the following line to allow serial debugging (115200 bauds)
// #define DEBUG

// ---------------------------
// Hardware pin definitions
// ---------------------------
#define CLOCK_PIN     2
#define RESET_PIN     3

#define NUMBER_OUTS   8

const uint8_t out_pins[NUMBER_OUTS] = {
  4, 5, 6, 7, 8, 9, 10, 11
};

// ---------------------------
// Firmware selection (EEPROM)
// ---------------------------
// Add new firmwares by:
//  1) Extending this enum
//  2) Adding cases in firmware_* dispatch functions below
//  3) Creating firmwareX_setup/loop/reset/manageClock functions

enum FirmwareId : uint8_t {
  FW_RING_COUNTER = 0,
  FW_DIVIDER_POLYMETER = 1,

  // Added firmwares
  FW_DIVIDER_24PPQN_POLYRHYTHM = 2,
  FW_DIVIDER_24PPQN_POW2_SLOW  = 3,   // 24 PPQN -> /1,/2,/4,... in quarter-notes
  FW_DIVIDER_POW2_NOTE         = 4,   // Classic divider: /2,/4,/8,/16,/32,/64,/128 (+/1 passthrough)

  // Counters
  FW_BINARY_COUNTER_8BIT       = 5,   // steady binary outputs
  FW_BINARY_COUNTER_8BIT_PULSED= 6,   // pulses when bits rise 0->1
  FW_GRAY_COUNTER_8BIT         = 7,   // steady Gray code outputs

  // Pulsed counters
  FW_BINARY_COUNTER_8BIT_PULSED_USED = 8,   // pulses when bit is 1 in the new count ("used bits")
  FW_COUNT  // = 9 (0..8 are implemented)
};

// EEPROM layout
static const uint16_t EEPROM_MAGIC = 0x5543; // 'U''C'
static const int EEPROM_ADDR_MAGIC = 0;      // uint16_t
static const int EEPROM_ADDR_FW    = 2;      // uint8_t

// active firmware
volatile uint8_t activeFirmware = FW_RING_COUNTER;

// ---------------------------
// Long-hold detection (5s)
// ---------------------------
static const uint32_t SWITCH_HOLD_MS = 5000UL;

uint32_t bothPressedSinceMs = 0;
bool switchLatched = false;

// ---------------------------
// Shared interrupt debouncing
// ---------------------------
volatile uint8_t last_clock_pin_state = HIGH;
volatile uint8_t clock_state = LOW;

// ---------------------------------------------------------------------------
// Firmware 0: Ring Counter (from uc_ring_counter.ino)
// ---------------------------------------------------------------------------
volatile uint8_t fw0_counter = 0;

void fw0_reset_outputs()
{
  fw0_counter = 0;

#ifdef DEBUG
  Serial.print("[FW0] reset -> counter=");
  Serial.println(fw0_counter);
#endif
}

void fw0_manage_clock()
{
  uint8_t current_clock_pin_state = digitalRead(CLOCK_PIN);

  // check for effective change (removes some bouncy effect)
  if (current_clock_pin_state != last_clock_pin_state)
  {
    last_clock_pin_state = current_clock_pin_state;

    // negative logic: LOW means voltage is present
    if (current_clock_pin_state == LOW)
    {
      // Rising Edge -> start the beat!
      clock_state = HIGH;
    }
    else
    {
      // Falling Edge -> be ready for next step
      clock_state = LOW;

      fw0_counter++;
      if (fw0_counter >= NUMBER_OUTS)
      {
        fw0_counter = 0;
      }
    }
  }
}

void fw0_loop_outputs()
{
  for (uint8_t i = 0; i < NUMBER_OUTS; i++)
  {
    if (fw0_counter == i)
    {
      digitalWrite(out_pins[i], clock_state);
    }
    else
    {
      digitalWrite(out_pins[i], LOW);
    }
  }
}

// ---------------------------------------------------------------------------
// Firmware 1: Divider / Polymeter (from uc_divider_polymeter.ino)
// ---------------------------------------------------------------------------

const uint8_t fw1_max_counts[NUMBER_OUTS] = {
  2, 4, 8, 16, 3, 6, 12, 5
};

volatile uint8_t fw1_counters[NUMBER_OUTS];

void fw1_reset_outputs()
{
  for (uint8_t i = 0; i < NUMBER_OUTS; i++)
  {
    fw1_counters[i] = 0;
  }

#ifdef DEBUG
  Serial.println("[FW1] reset");
#endif
}

void fw1_manage_clock()
{
  uint8_t current_clock_pin_state = digitalRead(CLOCK_PIN);

  // check for effective change (removes some bouncy effect)
  if (current_clock_pin_state != last_clock_pin_state)
  {
    last_clock_pin_state = current_clock_pin_state;

    // negative logic: LOW means voltage is present
    if (current_clock_pin_state == LOW)
    {
      // Rising Edge -> start the beat!
      clock_state = HIGH;
    }
    else
    {
      // Falling Edge -> be ready for next step
      clock_state = LOW;

      for (uint8_t i = 0; i < NUMBER_OUTS; i++)
      {
        uint8_t v = fw1_counters[i];
        v++;

        if (v >= fw1_max_counts[i])
        {
          v = 0;
        }

        fw1_counters[i] = v;
      }
    }
  }
}

void fw1_loop_outputs()
{
  for (uint8_t i = 0; i < NUMBER_OUTS; i++)
  {
    if (fw1_counters[i] == 0)
    {
      digitalWrite(out_pins[i], clock_state);
    }
    else
    {
      digitalWrite(out_pins[i], LOW);
    }
  }
}

// ---------------------------------------------------------------------------
// Firmware 2: Divider 24 PPQN Polyrhythm (from uc_divider_24ppqn_polyrhythm.ino)
// ---------------------------------------------------------------------------

// Resolution of the main counter: 24 PPQN
static const uint8_t fw2_max_resolution = 24;

// One main counter, incremented on each pulse on clock input. From 0 to (fw2_max_resolution - 1)
volatile uint8_t fw2_ppqn_counter = 0;

// output states computed on the rising edge and then gated by clock_state
volatile uint8_t fw2_out_states[NUMBER_OUTS] = {
  LOW, LOW, LOW, LOW, LOW, LOW, LOW, LOW
};

void fw2_reset_outputs()
{
  for (uint8_t i = 0; i < NUMBER_OUTS; i++) fw2_out_states[i] = LOW;
  fw2_ppqn_counter = 0;
}

void fw2_manage_clock()
{
  uint8_t current_clock_pin_state = digitalRead(CLOCK_PIN);

  // check for effective change (removes some bouncy effect)
  if (current_clock_pin_state != last_clock_pin_state)
  {
    last_clock_pin_state = current_clock_pin_state;

    // negative logic: LOW means voltage is present
    if (current_clock_pin_state == LOW)
    {
      // Rising Edge -> start the beat!
      clock_state = HIGH;

      // BINARY 1 (1)
      fw2_out_states[0] = (fw2_ppqn_counter == 0);
      // BINARY 2 (1/2)
      fw2_out_states[1] = (fw2_ppqn_counter == 0) || (fw2_ppqn_counter == 12);
      // BINARY 4 (1/4)
      fw2_out_states[2] = (fw2_ppqn_counter == 0) || (fw2_ppqn_counter == 6) || (fw2_ppqn_counter == 12) || (fw2_ppqn_counter == 18);
      // BINARY 8 (1/8)
      fw2_out_states[3] = (fw2_ppqn_counter == 0) || (fw2_ppqn_counter == 3) || (fw2_ppqn_counter == 6) || (fw2_ppqn_counter == 9) ||
                          (fw2_ppqn_counter == 12) || (fw2_ppqn_counter == 15) || (fw2_ppqn_counter == 18) || (fw2_ppqn_counter == 21);

      // TERNARY 3 (1/3)
      fw2_out_states[4] = (fw2_ppqn_counter == 0) || (fw2_ppqn_counter == 8) || (fw2_ppqn_counter == 16);
      // TERNARY 6 (1/6)
      fw2_out_states[5] = (fw2_ppqn_counter == 0) || (fw2_ppqn_counter == 4) || (fw2_ppqn_counter == 8) || (fw2_ppqn_counter == 12) ||
                          (fw2_ppqn_counter == 16) || (fw2_ppqn_counter == 20);
      // TERNARY 12 (1/12) : toggles every 2 ppqn clocks
      fw2_out_states[6] = (~(fw2_ppqn_counter & 0b00000001) & 0b00000001);

      // 24 PPQN (copy from input clock) or any other function ----- TBD ------
      fw2_out_states[7] = 0; // TBD
    }
    else
    {
      // Falling Edge -> be ready for next step
      clock_state = LOW;

      // increment ppqn_counter
      fw2_ppqn_counter++;

      // reset ppqn_counter if it overflow its resolution
      if (fw2_ppqn_counter >= fw2_max_resolution)
      {
        fw2_ppqn_counter = 0;
      }
    }
  }
}

void fw2_loop_outputs()
{
  for (uint8_t i = 0; i < NUMBER_OUTS; i++)
  {
    // Only output a beat when the clock is HIGH (reflect the Clock state)
    digitalWrite(out_pins[i], (clock_state == HIGH) ? fw2_out_states[i] : LOW);
  }
}

// ---------------------------------------------------------------------------
// Firmware 3: 24 PPQN -> slower POW2 set in quarter-notes (/1,/2,/4,...,/128)
// ---------------------------------------------------------------------------
// This firmware uses the same 24 PPQN input, but outputs *slower* divisions:
//   1, 2, 4, 8, 16, 32, 64, 128  (in quarter-notes)
// So:
//   /1  -> pulse every quarter note
//   /2  -> every 2 quarter notes
//   /4  -> every 4 quarter notes
//   ...
// Pulses are gated by clock_state (HIGH only during the clock pulse) like FW2.

static const uint8_t fw3_max_resolution = 24;
static const uint16_t fw3_divisions[NUMBER_OUTS] = {
  1, 2, 4, 8, 16, 32, 64, 128
};

volatile uint8_t  fw3_ppqn_counter = 0;
volatile uint16_t fw3_quarter_counter = 0; // counts quarter notes (wraps naturally at 65535)
volatile uint8_t  fw3_out_states[NUMBER_OUTS] = {
  LOW, LOW, LOW, LOW, LOW, LOW, LOW, LOW
};

void fw3_reset_outputs()
{
  for (uint8_t i = 0; i < NUMBER_OUTS; i++) fw3_out_states[i] = LOW;
  fw3_ppqn_counter = 0;
  fw3_quarter_counter = 0;
}

void fw3_manage_clock()
{
  uint8_t current_clock_pin_state = digitalRead(CLOCK_PIN);

  if (current_clock_pin_state != last_clock_pin_state)
  {
    last_clock_pin_state = current_clock_pin_state;

    if (current_clock_pin_state == LOW)
    {
      // Rising edge
      clock_state = HIGH;

      // Only evaluate divisions at the start of the quarter note (ppqn_counter == 0)
      if (fw3_ppqn_counter == 0)
      {
        for (uint8_t i = 0; i < NUMBER_OUTS; i++)
        {
          // emit a pulse when quarter_counter is a multiple of the division
          fw3_out_states[i] = ((fw3_quarter_counter % fw3_divisions[i]) == 0) ? HIGH : LOW;
        }
      }
      else
      {
        // mid-quarter: no new pulses
        for (uint8_t i = 0; i < NUMBER_OUTS; i++) fw3_out_states[i] = LOW;
      }
    }
    else
    {
      // Falling edge
      clock_state = LOW;

      fw3_ppqn_counter++;
      if (fw3_ppqn_counter >= fw3_max_resolution)
      {
        fw3_ppqn_counter = 0;
        fw3_quarter_counter++; // next quarter note
      }
    }
  }
}

void fw3_loop_outputs()
{
  for (uint8_t i = 0; i < NUMBER_OUTS; i++)
  {
    digitalWrite(out_pins[i], (clock_state == HIGH) ? fw3_out_states[i] : LOW);
  }
}

// ---------------------------------------------------------------------------
// Firmware 4: Regular clock divider POW2 (/2,/4,/8,/16,/32,/64,/128 + /1 passthru)
// ---------------------------------------------------------------------------
// Here, each incoming clock pulse is treated as "one step" (one note).
// Outputs generate a short pulse (gated by clock_state) every N input clocks.
// OUT 0..6: /2,/4,/8,/16,/32,/64,/128
// OUT 7   : /1 (passthrough of the incoming clock pulse)

static const uint8_t fw4_div_counts[7] = { 2, 4, 8, 16, 32, 64, 128 };
volatile uint8_t fw4_counters[7];

void fw4_reset_outputs()
{
  for (uint8_t i = 0; i < 7; i++) fw4_counters[i] = 0;
}

void fw4_manage_clock()
{
  uint8_t current_clock_pin_state = digitalRead(CLOCK_PIN);

  if (current_clock_pin_state != last_clock_pin_state)
  {
    last_clock_pin_state = current_clock_pin_state;

    if (current_clock_pin_state == LOW)
    {
      // Rising edge: show pulses (width follows the input clock width)
      clock_state = HIGH;
    }
    else
    {
      // Falling edge: advance all dividers
      clock_state = LOW;

      for (uint8_t i = 0; i < 7; i++)
      {
        uint8_t v = fw4_counters[i];
        v++;
        if (v >= fw4_div_counts[i]) v = 0;
        fw4_counters[i] = v;
      }
    }
  }
}

void fw4_loop_outputs()
{
  // OUT 0..6: pulse when counter hits 0 (start of the division cycle)
  for (uint8_t i = 0; i < 7; i++)
  {
    digitalWrite(out_pins[i], (fw4_counters[i] == 0) ? clock_state : LOW);
  }

  // OUT 7: passthrough clock
  digitalWrite(out_pins[7], clock_state);
}

// ---------------------------------------------------------------------------
// Firmware 5: 8-bit binary counter (0..255 then wrap)
// ---------------------------------------------------------------------------
// Each incoming clock pulse increments a counter.
// Outputs are *steady* (not short pulses): they represent the counter value in binary.
// OUT 0..7 correspond to bit weights 1,2,4,8,16,32,64,128.
// The counter wraps automatically after 255 (i.e., when it would become 256).

volatile uint8_t fw5_count = 0;

void fw5_reset_outputs()
{
  fw5_count = 0;
}

void fw5_manage_clock()
{
  uint8_t current_clock_pin_state = digitalRead(CLOCK_PIN);

  if (current_clock_pin_state != last_clock_pin_state)
  {
    last_clock_pin_state = current_clock_pin_state;

    // Negative-logic clock: typical pulse is LOW while active.
    // To behave like FW1 (Divider/Polymeter), we advance on the active edge (HIGH -> LOW).
    if (current_clock_pin_state == LOW)
    {
      fw5_count++; // uint8_t naturally wraps 255 -> 0
    }
  }
}

void fw5_loop_outputs()
{
  // Steady binary outputs
  for (uint8_t i = 0; i < NUMBER_OUTS; i++)
  {
    uint8_t bit = (fw5_count >> i) & 0x01;
    digitalWrite(out_pins[i], bit ? HIGH : LOW);
  }
}


// ---------------------------------------------------------------------------
// Firmware 6: 8-bit Binary Counter (pulsed outputs on bit 0->1)
// ---------------------------------------------------------------------------

static volatile uint8_t fw6_count = 0;

// From ISR: bits that should start a pulse (bit rose 0->1)
static volatile uint8_t fw6_pulse_request_mask = 0;

// Pulse timing handled in loop
static const uint16_t FW6_PULSE_MS = 15;
static uint32_t fw6_pulse_end_ms[NUMBER_OUTS] = {0};

void fw6_reset_outputs()
{
  fw6_count = 0;
  fw6_pulse_request_mask = 0;

  for (uint8_t i = 0; i < NUMBER_OUTS; i++)
  {
    fw6_pulse_end_ms[i] = 0;
    digitalWrite(out_pins[i], LOW);
  }
}

void fw6_manage_clock()
{
  uint8_t current_clock_pin_state = digitalRead(CLOCK_PIN);

  if (current_clock_pin_state != last_clock_pin_state)
  {
    last_clock_pin_state = current_clock_pin_state;

    // Negative-logic clock: pulse is LOW while active.
    // Advance on the active edge (HIGH -> LOW), matching FW1 timing.
    if (current_clock_pin_state == LOW)
    {
      uint8_t oldCount = fw6_count;
      fw6_count++; // wraps naturally 255 -> 0
      uint8_t newCount = fw6_count;

      // Start pulses for bits that changed 0->1
      uint8_t risingMask = (~oldCount) & newCount;
      fw6_pulse_request_mask |= risingMask;
    }
  }
}

void fw6_loop_outputs()
{
  // Grab any new pulse requests atomically
  uint8_t req = 0;
  noInterrupts();
  req = fw6_pulse_request_mask;
  fw6_pulse_request_mask = 0;
  interrupts();

  uint32_t now = millis();

  // Start requested pulses
  for (uint8_t i = 0; i < NUMBER_OUTS; i++)
  {
    if (req & (1 << i))
    {
      digitalWrite(out_pins[i], HIGH);
      fw6_pulse_end_ms[i] = now + FW6_PULSE_MS;
    }
  }

  // End pulses when time is up
  for (uint8_t i = 0; i < NUMBER_OUTS; i++)
  {
    if (fw6_pulse_end_ms[i] != 0 && (int32_t)(now - fw6_pulse_end_ms[i]) >= 0)
    {
      fw6_pulse_end_ms[i] = 0;
      digitalWrite(out_pins[i], LOW);
    }
  }
}

// ---------------------------------------------------------------------------
// Firmware 7: 8-bit Gray Code Counter (steady outputs)
// ---------------------------------------------------------------------------

static volatile uint8_t fw7_count = 0;

void fw7_reset_outputs()
{
  fw7_count = 0;
  for (uint8_t i = 0; i < NUMBER_OUTS; i++)
  {
    digitalWrite(out_pins[i], LOW);
  }
}

void fw7_manage_clock()
{
  uint8_t current_clock_pin_state = digitalRead(CLOCK_PIN);

  if (current_clock_pin_state != last_clock_pin_state)
  {
    last_clock_pin_state = current_clock_pin_state;

    // Negative-logic clock: pulse is LOW while active.
    // Advance on the active edge (HIGH -> LOW), matching FW1 timing.
    if (current_clock_pin_state == LOW)
    {
      fw7_count++; // wraps naturally 255 -> 0
    }
  }
}

void fw7_loop_outputs()
{
  // Gray code: g = n ^ (n >> 1)
  uint8_t gray = (uint8_t)(fw7_count ^ (fw7_count >> 1));

  for (uint8_t i = 0; i < NUMBER_OUTS; i++)
  {
    uint8_t bit = (gray >> i) & 0x01;
    digitalWrite(out_pins[i], bit ? HIGH : LOW);
  }
}
// ---------------------------------------------------------------------------
// Firmware 8: 8-bit Binary Counter (pulsed outputs on "used bits")
// ---------------------------------------------------------------------------
//
// Meaning of "used bits":
// - We increment the counter on each CLOCK rising edge.
// - Look at the NEW counter value.
// - Any bit that is 1 in that value produces a short pulse on its output,
//   even if that bit was already 1 on the previous step.

static volatile uint8_t fw8_count = 0;

// Per-bit pending pulse requests (queued from ISR)
static volatile uint8_t fw8_pending[NUMBER_OUTS] = {0};

// Pulse timing handled in loop
static const uint16_t FW8_PULSE_MS = 15;
static uint32_t fw8_pulse_end_ms[NUMBER_OUTS] = {0};

void fw8_reset_outputs()
{
  fw8_count = 0;

  noInterrupts();
  for (uint8_t i = 0; i < NUMBER_OUTS; i++)
  {
    fw8_pending[i] = 0;
  }
  interrupts();

  for (uint8_t i = 0; i < NUMBER_OUTS; i++)
  {
    fw8_pulse_end_ms[i] = 0;
    digitalWrite(out_pins[i], LOW);
  }
}

void fw8_manage_clock()
{
  uint8_t current_clock_pin_state = digitalRead(CLOCK_PIN);

  if (current_clock_pin_state != last_clock_pin_state)
  {
    last_clock_pin_state = current_clock_pin_state;

    // Negative-logic clock: pulse is LOW while active.
    // Advance on the active edge (HIGH -> LOW), matching FW1 timing.
    if (current_clock_pin_state == LOW)
    {
      fw8_count++; // wraps naturally 255 -> 0
      uint8_t newCount = fw8_count;

      // Request a pulse for every bit that is 1 in the NEW count ("used bits").
      // Queue pulses so fast clocks don't drop events.
      for (uint8_t i = 0; i < NUMBER_OUTS; i++)
      {
        if (newCount & (uint8_t)(1U << i))
        {
          // Saturating increment (avoid overflow if clock is extremely fast)
          if (fw8_pending[i] < 255)
          {
            fw8_pending[i]++;
          }
        }
      }
    }
  }
}

void fw8_loop_outputs()
{
  uint32_t now = millis();

  // Start at most one queued pulse per bit per loop pass
  for (uint8_t i = 0; i < NUMBER_OUTS; i++)
  {
    bool startPulse = false;

    noInterrupts();
    if (fw8_pending[i] > 0)
    {
      fw8_pending[i]--;
      startPulse = true;
    }
    interrupts();

    if (startPulse)
    {
      digitalWrite(out_pins[i], HIGH);
      fw8_pulse_end_ms[i] = now + FW8_PULSE_MS;
    }
  }

  // End pulses when time is up
  for (uint8_t i = 0; i < NUMBER_OUTS; i++)
  {
    if (fw8_pulse_end_ms[i] != 0 && (int32_t)(now - fw8_pulse_end_ms[i]) >= 0)
    {
      fw8_pulse_end_ms[i] = 0;
      digitalWrite(out_pins[i], LOW);
    }
  }
}

// ---------------------------------------------------------------------------
// Placeholders for future firmwares
// ---------------------------------------------------------------------------




// ---------------------------------------------------------------------------
// Dispatch helpers (NO function pointers, just switch-cases)
// ---------------------------------------------------------------------------

void firmware_reset_outputs()
{
  switch (activeFirmware)
  {
    case FW_RING_COUNTER:              fw0_reset_outputs(); break;
    case FW_DIVIDER_POLYMETER:         fw1_reset_outputs(); break;
    case FW_DIVIDER_24PPQN_POLYRHYTHM: fw2_reset_outputs(); break;
    case FW_DIVIDER_24PPQN_POW2_SLOW:  fw3_reset_outputs(); break;
    case FW_DIVIDER_POW2_NOTE:         fw4_reset_outputs(); break;
    case FW_BINARY_COUNTER_8BIT:       fw5_reset_outputs(); break;
    case FW_BINARY_COUNTER_8BIT_PULSED:fw6_reset_outputs(); break;
    case FW_GRAY_COUNTER_8BIT:         fw7_reset_outputs(); break;
    case FW_BINARY_COUNTER_8BIT_PULSED_USED: fw8_reset_outputs(); break;
default:                           fw0_reset_outputs(); break;
  }
}

void firmware_manage_clock()
{
  switch (activeFirmware)
  {
    case FW_RING_COUNTER:              fw0_manage_clock(); break;
    case FW_DIVIDER_POLYMETER:         fw1_manage_clock(); break;
    case FW_DIVIDER_24PPQN_POLYRHYTHM: fw2_manage_clock(); break;
    case FW_DIVIDER_24PPQN_POW2_SLOW:  fw3_manage_clock(); break;
    case FW_DIVIDER_POW2_NOTE:         fw4_manage_clock(); break;
    case FW_BINARY_COUNTER_8BIT:       fw5_manage_clock(); break;
    case FW_BINARY_COUNTER_8BIT_PULSED:fw6_manage_clock(); break;
    case FW_GRAY_COUNTER_8BIT:         fw7_manage_clock(); break;
    case FW_BINARY_COUNTER_8BIT_PULSED_USED: fw8_manage_clock(); break;
default:                           fw0_manage_clock(); break;
  }
}

void firmware_loop_outputs()
{
  switch (activeFirmware)
  {
    case FW_RING_COUNTER:              fw0_loop_outputs(); break;
    case FW_DIVIDER_POLYMETER:         fw1_loop_outputs(); break;
    case FW_DIVIDER_24PPQN_POLYRHYTHM: fw2_loop_outputs(); break;
    case FW_DIVIDER_24PPQN_POW2_SLOW:  fw3_loop_outputs(); break;
    case FW_DIVIDER_POW2_NOTE:         fw4_loop_outputs(); break;
    case FW_BINARY_COUNTER_8BIT:       fw5_loop_outputs(); break;
    case FW_BINARY_COUNTER_8BIT_PULSED:fw6_loop_outputs(); break;
    case FW_GRAY_COUNTER_8BIT:         fw7_loop_outputs(); break;
    case FW_BINARY_COUNTER_8BIT_PULSED_USED: fw8_loop_outputs(); break;
default:                           fw0_loop_outputs(); break;
  }
}

// ---------------------------------------------------------------------------
// Interrupt Service Routines
// ---------------------------------------------------------------------------

void isr_clock_change()
{
  firmware_manage_clock();
}

void isr_reset_falling()
{
  firmware_reset_outputs();
}

// ---------------------------------------------------------------------------
// EEPROM helpers
// ---------------------------------------------------------------------------

uint8_t clampFirmware(uint8_t fw)
{
  if (fw >= FW_COUNT) return FW_RING_COUNTER;
  return fw;
}

uint8_t eeprom_read_firmware_or_default()
{
  uint16_t magic = 0;
  EEPROM.get(EEPROM_ADDR_MAGIC, magic);

  if (magic != EEPROM_MAGIC)
  {
    return FW_RING_COUNTER;
  }

  uint8_t fw = EEPROM.read(EEPROM_ADDR_FW);
  return clampFirmware(fw);
}

void eeprom_write_firmware(uint8_t fw)
{
  fw = clampFirmware(fw);

  uint16_t magic = EEPROM_MAGIC;
  EEPROM.put(EEPROM_ADDR_MAGIC, magic);
  EEPROM.update(EEPROM_ADDR_FW, fw);
}

// ---------------------------------------------------------------------------
// Firmware switching logic
// ---------------------------------------------------------------------------

void applyFirmware(uint8_t newFw)
{
  newFw = clampFirmware(newFw);

  // Prevent ISR from using partially updated state
  noInterrupts();

  // Detach interrupts while switching to avoid calling old firmware mid-change
  detachInterrupt(digitalPinToInterrupt(CLOCK_PIN));
  detachInterrupt(digitalPinToInterrupt(RESET_PIN));

  activeFirmware = newFw;

  // Reset shared debounce so first edge after switching is clean
  last_clock_pin_state = digitalRead(CLOCK_PIN);
  clock_state = LOW;

  // Initialize the selected firmware state
  firmware_reset_outputs();

  // Re-attach interrupts
  attachInterrupt(digitalPinToInterrupt(CLOCK_PIN), isr_clock_change, CHANGE);
  attachInterrupt(digitalPinToInterrupt(RESET_PIN), isr_reset_falling, FALLING);

  interrupts();

#ifdef DEBUG
  Serial.print("Switched firmware -> ");
  Serial.println(activeFirmware);
#endif
}

void checkFirmwareSwitchGesture()
{
  // Both inputs are active LOW in these firmwares
  bool clockPressed = (digitalRead(CLOCK_PIN) == LOW);
  bool resetPressed = (digitalRead(RESET_PIN) == LOW);
  bool bothPressed  = (clockPressed && resetPressed);

  uint32_t now = millis();

  if (bothPressed)
  {
    if (!switchLatched)
    {
      if (bothPressedSinceMs == 0)
      {
        bothPressedSinceMs = now;
      }
      else if ((now - bothPressedSinceMs) >= SWITCH_HOLD_MS)
      {
        // Next firmware (wrap around)
        uint8_t nextFw = activeFirmware + 1;
        if (nextFw >= FW_COUNT) nextFw = 0;

        eeprom_write_firmware(nextFw);
        applyFirmware(nextFw);

        // Latch until the user releases both buttons
        switchLatched = true;
      }
    }
  }
  else
  {
    // Not both pressed
    bothPressedSinceMs = 0;
    switchLatched = false;
  }
}

// ---------------------------------------------------------------------------
// Arduino setup/loop
// ---------------------------------------------------------------------------

void setup()
{
#ifdef DEBUG
  Serial.begin(115200);
  Serial.println("UC multi-firmware boot");
#endif

  // inputs
  pinMode(CLOCK_PIN, INPUT_PULLUP);
  pinMode(RESET_PIN, INPUT_PULLUP);

  // outputs
  for (uint8_t i = 0; i < NUMBER_OUTS; i++)
  {
    pinMode(out_pins[i], OUTPUT);
    digitalWrite(out_pins[i], LOW);
  }

  // load firmware selection
  activeFirmware = eeprom_read_firmware_or_default();

  // Initialize debounce baseline
  last_clock_pin_state = digitalRead(CLOCK_PIN);
  clock_state = LOW;

  // Initialize firmware state
  firmware_reset_outputs();

  // interrupts
  attachInterrupt(digitalPinToInterrupt(CLOCK_PIN), isr_clock_change, CHANGE);
  attachInterrupt(digitalPinToInterrupt(RESET_PIN), isr_reset_falling, FALLING);
  interrupts();

#ifdef DEBUG
  Serial.print("Active firmware: ");
  Serial.println(activeFirmware);
#endif
}

void loop()
{
  // Main output logic
  firmware_loop_outputs();

  // Non-blocking: check the long-hold gesture
  checkFirmwareSwitchGesture();
}
