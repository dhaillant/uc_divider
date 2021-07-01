/* 
 * UC Divider is a configurable step/beat divider for musical application
 * It features: 
 * 8 gated/trigged outputs (5V)
 * 1 clock input (positive voltage detection, max +5V)
 * 1 reset input (positive voltage detection, max +5V)
 * 
 * 2021 David Haillant
 * 
 * See hardware and additional information:
 * www.davidhaillant.com
 * 
 */


/*
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/


// This version offers true POLYRHYTHM outputs from 24 PPQN clock input
// Revision 20210630


// Uncomment the following line to allow serial debugging (115200 bauds)
//#define DEBUG


// pin definitions (see Arduino pin mapping)
#define CLOCK_PIN     2
#define RESET_PIN     3

// number of outputs
#define NUMBER_OUTS   8

// array of output pin numbers
const uint8_t out_pins[NUMBER_OUTS] = {
  4, 5, 6, 7, 8, 9, 10, 11
};

// array of output states
uint8_t out_states[NUMBER_OUTS] = {
  LOW, LOW, LOW, LOW, LOW, LOW, LOW, LOW
};




// resolution of the main counter. 24 PPQN
#define MAX_RESOLUTION 24

// One main counter, incremented on each pulse on clock input. From 0 to (MAX_RESOLUTION - 1)
uint8_t ppqn_counter = 0;




uint8_t clock_state = LOW;
uint8_t reset_state = LOW;

// deboucing
uint8_t last_clock_pin_state = HIGH;

#ifdef DEBUG
uint8_t debug_counter = 0;
#endif

void reset_outputs(void);
void manage_clock();


void setup() {
  // inputs
  pinMode(CLOCK_PIN, INPUT_PULLUP);
  pinMode(RESET_PIN, INPUT_PULLUP);

  // outputs
  for (uint8_t i = 0; i < NUMBER_OUTS; i++) pinMode(out_pins[i], OUTPUT);


  // set up interrupt on clock pin
  attachInterrupt(digitalPinToInterrupt(CLOCK_PIN), manage_clock, CHANGE);

  // set up interrupt on reset pin
  attachInterrupt(digitalPinToInterrupt(RESET_PIN), reset_outputs, FALLING);

  interrupts();

#ifdef DEBUG
  Serial.begin(115200);
  Serial.println("hello"); 
#endif

  reset_outputs();
}

void loop() {
  for (uint8_t i = 0; i < NUMBER_OUTS; i++)
  {
    // but only output a beat when the clock is HIGH (reflect the Clock state)
    digitalWrite(out_pins[i], (clock_state == HIGH) ? out_states[i] : LOW);
  }
}

void reset_outputs(void)
{
  // load each counter with its MAX value -> Ready for the beat!
  for (uint8_t i = 0; i < NUMBER_OUTS; i++) out_states[i] = LOW;
  ppqn_counter = 0;

#ifdef DEBUG
  Serial.print("PPQN: ");
  Serial.println(ppqn_counter);
  for (uint8_t i = 0; i < NUMBER_OUTS; i++)
  {
    Serial.print("\t");
    Serial.print(out_states[i]);
  }
  Serial.println("\n\t----------------------------------------------------------------------");
#endif
}


void manage_clock()
{
  uint8_t current_clock_pin_state = digitalRead(CLOCK_PIN);

  // check for effective change (removes some bouncy effect)
  if (current_clock_pin_state != last_clock_pin_state)
  {
    last_clock_pin_state = current_clock_pin_state; // keep that in mind for the next call

    // manage either start or end of pulse (the logic is negative: LOW means voltage is present)
    if (current_clock_pin_state == LOW)
    {
      // Rising Edge -> start the beat!

      // the clock state is useful for activating the outputs only while the clock is high
      clock_state = HIGH;
      // -> the outputs are allowed to turn ON


/*
  binary_1_out = (ppqn_counter == 0);
  binary_2_out = (ppqn_counter == 0) || (ppqn_counter == 12);
  binary_4_out = (ppqn_counter == 0) || (ppqn_counter == 6) || (ppqn_counter == 12) || (ppqn_counter == 18);
  binary_8_out = (ppqn_counter == 0) || (ppqn_counter == 3) || (ppqn_counter == 6) || (ppqn_counter == 9) || (ppqn_counter == 12) || (ppqn_counter == 15) || (ppqn_counter == 18) || (ppqn_counter == 21);

  ternary_3_out = (ppqn_counter == 0) || (ppqn_counter == 8) || (ppqn_counter == 16);
  ternary_6_out = (ppqn_counter == 0) || (ppqn_counter == 4) || (ppqn_counter == 8) || (ppqn_counter == 12) || (ppqn_counter == 16) || (ppqn_counter == 20);
  ternary_12_out = ~(ppqn_counter & 0b00000001);
 */

      // BINARY 1 (1)
      out_states[0] = (ppqn_counter == 0);
      // BINARY 2 (1/2)
      out_states[1] = (ppqn_counter == 0) || (ppqn_counter == 12);
      // BINARY 4 (1/4)
      out_states[2] = (ppqn_counter == 0) || (ppqn_counter == 6) || (ppqn_counter == 12) || (ppqn_counter == 18);
      // BINARY 8 (1/8)
      out_states[3] = (ppqn_counter == 0) || (ppqn_counter == 3) || (ppqn_counter == 6) || (ppqn_counter == 9) || (ppqn_counter == 12) || (ppqn_counter == 15) || (ppqn_counter == 18) || (ppqn_counter == 21);

      // TERNARY 3 (1/3)
      out_states[4] = (ppqn_counter == 0) || (ppqn_counter == 8) || (ppqn_counter == 16);
      // TERNARY 6 (1/6)
      out_states[5] = (ppqn_counter == 0) || (ppqn_counter == 4) || (ppqn_counter == 8) || (ppqn_counter == 12) || (ppqn_counter == 16) || (ppqn_counter == 20);
      // TERNARY 12 (1/12)
      out_states[6] = (~(ppqn_counter & 0b00000001) & 0b00000001);

      // 24 PPQN (copy from input clock) or any other function ----- TBD ------
      out_states[7] = 0;  // TBD


#ifdef DEBUG
      Serial.print("\t");
      Serial.print(debug_counter);
      debug_counter++;
      
      Serial.print("\t");
      Serial.print(ppqn_counter);

      for (uint8_t i = 0; i < NUMBER_OUTS; i++)
      {
        Serial.print("\t");
        Serial.print(out_states[i]);
      }
      Serial.println("");
#endif
    }
    else
    {
      // Falling Edge -> be ready for next step
      clock_state = LOW;

      // increment ppqn_counter
      ppqn_counter++;
  
      // reset ppqn_counter if it overflow its resolution
      if (ppqn_counter >= MAX_RESOLUTION)
      {
        ppqn_counter = 0;
      }
#ifdef DEBUG
      Serial.println(".");
#endif
    }
  }
  else
  {
    // we are in the same state as previous call... this shouldn't happen (bounces?)
    // do nothing
#ifdef DEBUG
      Serial.println("B");
#endif
  }

}
