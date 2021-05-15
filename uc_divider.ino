// pin definitions
#define CLOCK_PIN     2
#define RESET_PIN     3

// number of outputs
#define NUMBER_OUTS   7

const uint8_t out_pins[NUMBER_OUTS] = {
  4, 5, 6, 7, 8, 9, 10
};    // array of output pin numbers

// each output uses a counter, incremented at each clock event
uint8_t counters[NUMBER_OUTS];

// Set here the resolution of each output (the output is activated when the max value is reached)
const uint8_t max_counts[NUMBER_OUTS] = {
  2, 4, 8, 16, 3, 6, 12
};

uint8_t clock_state = LOW;
uint8_t reset_state = LOW;

int reset_outputs(void);
int manage_clock();


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

  Serial.begin(57600);
  Serial.println("coucou"); 

  reset_outputs();
}

void loop() {
  for (uint8_t i = 0; i < NUMBER_OUTS; i++)
  {
    // if the counter is at max, it's the beat!
    //if (counters[i] == max_counts[i])
    if (counters[i] == 0)
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

int reset_outputs(void)
{
  // load each counter with its MAX value -> Ready for the beat!
  //for (uint8_t i = 0; i < NUMBER_OUTS; i++) counters[i] = max_counts[i];
  for (uint8_t i = 0; i < NUMBER_OUTS; i++) counters[i] = 0;

    for (uint8_t i = 0; i < NUMBER_OUTS; i++)
    {
      Serial.print("\t");
      Serial.print(counters[i]);
    }
    Serial.println("\n\t----------------------------------------------------------------------");

  return 0;
}

int manage_clock()
{
  if (digitalRead(CLOCK_PIN) == LOW)
  {
    // Rising Edge -> start the beat!
    clock_state = HIGH;

    for (uint8_t i = 0; i < NUMBER_OUTS; i++)
    {
      Serial.print("\t");
      Serial.print(counters[i]);
    }
    Serial.println("");
    delay(100);
  }
  else
  {
    // Falling Edge -> be ready for next step
    clock_state = LOW;

    // increment each counter
    for (uint8_t i = 0; i < NUMBER_OUTS; i++)
    {
      counters[i]++;
    }

    // reset counters that are overflowed
    for (uint8_t i = 0; i < NUMBER_OUTS; i++)
    {
      if (counters[i] >= max_counts[i])
      {
        counters[i] = 0;
      }
    }
    delay(100);
  }


  return 0;
}
