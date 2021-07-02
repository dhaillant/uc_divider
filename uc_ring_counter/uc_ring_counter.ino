/* 
 * UC Divider is a configurable step/beat divider for musical application
 * It features: 
 * 8 gate/trig outputs (5V)
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

/*
 * This code will transform the UC Divider in a 8 step Ring Counter
 * 
 * 
 */
// Revision 20210702


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


// main counter, incremented on each Clock pulse
uint8_t counter = 0;


uint8_t clock_state = LOW;
uint8_t reset_state = LOW;


// deboucing thing
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
    // if the main counter correspond to the selected pin output...
    if (counter == i)
    {
      // but only output a beat when the clock is HIGH (reflect the Clock state)
      digitalWrite(out_pins[i], clock_state);
    }
    else
    {
      // keep the outputs LOW
      digitalWrite(out_pins[i], LOW);
    }
  }
}

void reset_outputs(void)
{
  counter = 0;

#ifdef DEBUG
    Serial.print("\t");
    Serial.print(counter);
  Serial.println("\n\t----------------------------------------------------------------------");
#endif
}


void manage_clock()
{
  uint8_t current_clock_pin_state = digitalRead(CLOCK_PIN);

  // check for effective change (removes some bouncy effect)
  if (current_clock_pin_state != last_clock_pin_state)
  {
    last_clock_pin_state = current_clock_pin_state;

    // manage either start or end of pulse (the logic is negative: LOW means voltage is present)
    if (current_clock_pin_state == LOW)
    {
      // Rising Edge -> start the beat!

      // the clock state is useful for activating the outputs only while the clock is high
      clock_state = HIGH;
      // -> the outputs are allowed to turn ON


#ifdef DEBUG
      Serial.print("\t");
      Serial.print(debug_counter);
      debug_counter++;

      Serial.print("\t");
      Serial.print(counter);
      Serial.println("");
#endif
    }
    else
    {
      // Falling Edge -> be ready for next step
      clock_state = LOW;
  
      counter++;

      // reset counter if it overflows
      if (counter >= NUMBER_OUTS)
      {
        counter = 0;
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
