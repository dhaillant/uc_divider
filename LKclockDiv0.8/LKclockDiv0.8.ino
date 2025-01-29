/*
11 jan 2025
By LineKernel
I made this code to synch my eurorack with my semi modular moog (mother32 serie) i have a DFAM , that i was thinking would do great on output 2, a Labyrinth that would do great on output 4 , and a Subharmonicon that i dont know how to use properly yet :P
sorry for this horror of a code , i m not really a programmer , i thought ChatGPT would write it all , but i ended up re writing everything , so the code was quite glitchy with dropped tigs and of syncops , i then asked Claude and following its suggestion i was able to get a robust code mainly by mouving things (the tic function) outside of the interrup , so thanks Claude 3.5Sonnet Normal Style , also i didnt clean unused/commented lines sorry 
this code is under GPL license 
there should be an overflow at the end of the unsigned long that hold the microseconds , use at your own risk , idk what will happend , it s around 71mn  
Thanks To David Haillant for this great module with its infinit possibilities 
*/
//25 jan 2025 i used DeepSeek in deepthinking R1 mode to correct a missing bool preventng the standalone "fallback" mode to fully work . The code is working as intended but i have problems with my Behringer marbles clone , and i m thiking it s not my code ...

/*
FEATURES
-clock fallback when no clock is input for 5 sec 
-clock fallback list of 16 BPM , increment with the reset button 30, 40, 60, 80, 90, 100, 110, 120, 125, 130, 140, 160, 180, 200, 220, 240
-5ms trigs 
-Output 1: Copy of the input clock (unmodified).
-Output 2: Same speed as the input clock but 180° phase-shifted.
-Output 3: Clock at double the input speed.
-Output 4: Clock at four times the input speed.
-Output 5: Clock at half the input speed.
-Output 6: Clock at quarter the input speed. 
-Output 7: Coin toss with output 8 
-Output 8: Coin toss with output 7
*/

//#define DEBUG //-----------------------------------------------------------------------------------------------------------------

//define inputs
#define INPUT_CLOCK_PIN 2
#define BUTTON_PIN 3
//define outputs
#define OUTPUT_1_PIN 4
#define OUTPUT_2_PIN 5
#define OUTPUT_3_PIN 6
#define OUTPUT_4_PIN 7
#define OUTPUT_5_PIN 8
#define OUTPUT_6_PIN 9
#define OUTPUT_7_PIN 10
#define OUTPUT_8_PIN 11

//define variables
unsigned long interval = 545455; //start with bpm of 110
unsigned long f5SECONDS = 5000000; //fallback after 5 SECONDS when no clock in 
//bool fallback = false; //are we in fallback mode //this is never used 
unsigned long fallbackStartTime = 0;
const unsigned long INTERVAL_LIST[] = {2000000, 1500000, 1000000, 750000, 666667, 600000, 545455, 500000, 480000, 461538, 428571, 375000, 333333, 300000, 272727, 250000}; //interval in useconcdes = 60 000 000 / BPM
const int INTERVAL_LIST_SIZE = sizeof(INTERVAL_LIST) / sizeof(INTERVAL_LIST[0]);  // Number of BPM values in the list;
int currentBpmIndex = 7; // Start with 110 BPM (index 5)
const unsigned long TRIG_DURATION = 5000; // 5 ms trig length 
unsigned long lastTicTime =0;
unsigned long note = 0; // counter to % for outputs 5 and 6 //i think this isnt used 
bool noteHalf, noteQuarter, noteTreeForth= false; //only used for outputs 2 3 and 4 
bool fallbackTik = true; 
volatile unsigned long newInterval = 0;
volatile bool needsTic = false;
unsigned long nextHalfTime, nextQuarterTime, nextThreeQuarterTime =0;
static bool inFallback = false;

// Timing variables
unsigned long lastPulseTime = 0;         // Timestamp of the last detected pulse
unsigned long currentTime = 0;           // current time in MicroSeconds 
unsigned long trigEndTime1 = 0;          // End time for the current trig on Output 1
unsigned long trigEndTime2 = 0;
unsigned long trigEndTime3 = 0;
unsigned long trigEndTime4 = 0;
unsigned long trigEndTime5 = 0;
unsigned long trigEndTime6 = 0;
unsigned long trigEndTime7 = 0;
unsigned long trigEndTime8 = 0;
unsigned long trigEndTimeLED = 0; // for visual debug when connected to the computer


void setup() {
  // init in
  pinMode(INPUT_CLOCK_PIN, INPUT_PULLUP);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  //init outputs 
  pinMode(OUTPUT_1_PIN, OUTPUT);
  pinMode(OUTPUT_2_PIN, OUTPUT);
  pinMode(OUTPUT_3_PIN, OUTPUT);
  pinMode(OUTPUT_4_PIN, OUTPUT);
  pinMode(OUTPUT_5_PIN, OUTPUT);
  pinMode(OUTPUT_6_PIN, OUTPUT);
  pinMode(OUTPUT_7_PIN, OUTPUT);
  pinMode(OUTPUT_8_PIN, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT); // LED i ll use for debug purpuse , when i m debuging connected to the computer
  digitalWrite(OUTPUT_1_PIN, LOW);
  digitalWrite(OUTPUT_2_PIN, LOW);
  digitalWrite(OUTPUT_3_PIN, LOW);
  digitalWrite(OUTPUT_4_PIN, LOW);
  digitalWrite(OUTPUT_5_PIN, LOW);
  digitalWrite(OUTPUT_6_PIN, LOW);
  digitalWrite(OUTPUT_7_PIN, LOW);
  digitalWrite(OUTPUT_8_PIN, LOW);
  // Attach interrupts
  attachInterrupt(digitalPinToInterrupt(INPUT_CLOCK_PIN), clockPulseHandler, RISING);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonPressHandler, FALLING);
  //coin toss setup
  randomSeed(analogRead(A0));
}

void loop() {
  currentTime = micros();

  //calculate time without clock in fallback 
  if(currentTime - lastPulseTime >= f5SECONDS)
  {
    interval = INTERVAL_LIST[currentBpmIndex];
    inFallback = true;  // Properly mark fallback state

    if(currentTime - fallbackStartTime >= interval)
    {
      needsTic = true;
      fallbackStartTime = currentTime; // Reset to current time

    }
  } 
  else {
    inFallback = false; // Reset fallback state when clock is present
  }

    // Handle intermediate triggers with more precise timing
    if(currentTime >= nextHalfTime && noteHalf) {
        digitalWrite(OUTPUT_2_PIN, HIGH);
        trigEndTime2 = currentTime + TRIG_DURATION;
        digitalWrite(OUTPUT_3_PIN, HIGH);
        trigEndTime3 = currentTime + TRIG_DURATION;
        digitalWrite(OUTPUT_4_PIN, HIGH);
        trigEndTime4 = currentTime + TRIG_DURATION;
        noteHalf = false;
    }

    if(currentTime >= nextQuarterTime && noteQuarter) {
        digitalWrite(OUTPUT_4_PIN, HIGH);
        trigEndTime4 = currentTime + TRIG_DURATION;
        noteQuarter = false;
    }

    if(currentTime >= nextThreeQuarterTime && noteTreeForth) {
        digitalWrite(OUTPUT_4_PIN, HIGH);
        trigEndTime4 = currentTime + TRIG_DURATION;
        noteTreeForth = false;
    }

    if (needsTic) {
        if (!inFallback) {
          interval = newInterval;  // Only update from external clock when not in fallback
        }
        tic();                  // Process the tick
        needsTic = false;       // Reset flag
    }

  if(currentTime >= trigEndTime1)digitalWrite(OUTPUT_1_PIN, LOW);
  if(currentTime >= trigEndTime2)digitalWrite(OUTPUT_2_PIN, LOW);
  if(currentTime >= trigEndTime3)digitalWrite(OUTPUT_3_PIN, LOW);
  if(currentTime >= trigEndTime4)digitalWrite(OUTPUT_4_PIN, LOW);
  if(currentTime >= trigEndTime5)digitalWrite(OUTPUT_5_PIN, LOW);
  if(currentTime >= trigEndTime6)digitalWrite(OUTPUT_6_PIN, LOW);
  if(currentTime >= trigEndTime7)digitalWrite(OUTPUT_7_PIN, LOW);
  if(currentTime >= trigEndTime8)digitalWrite(OUTPUT_8_PIN, LOW);
  if(currentTime >= trigEndTimeLED)digitalWrite(LED_BUILTIN, LOW);

}

void tic(){
  
  lastTicTime = micros();

  // Schedule the next intermediate triggers  
  nextHalfTime = lastTicTime + (interval / 2);
  nextQuarterTime = lastTicTime + (interval / 4);
  nextThreeQuarterTime = lastTicTime + ((interval * 3) / 4);  

  noteHalf = true;
  noteQuarter = true;
  noteTreeForth = true;
  
  note++; // i think this is not used 
  int toss = random(2);

  digitalWrite(OUTPUT_1_PIN, HIGH);
  trigEndTime1 = currentTime + TRIG_DURATION;

  //2

  //3
  digitalWrite(OUTPUT_3_PIN, HIGH);
  trigEndTime3 = currentTime + TRIG_DURATION;

  //4
  digitalWrite(OUTPUT_4_PIN, HIGH);
  trigEndTime4 = currentTime + TRIG_DURATION;

  //5
  if(note % 2 == 1)digitalWrite(OUTPUT_5_PIN, HIGH);
  trigEndTime5 = currentTime + TRIG_DURATION;

  //6
  if(note % 4 == 1)digitalWrite(OUTPUT_6_PIN, HIGH);
  trigEndTime6 = currentTime + TRIG_DURATION;

  //7
  if(toss == 0)digitalWrite(OUTPUT_7_PIN, HIGH);
  trigEndTime7 = currentTime + TRIG_DURATION;
  //8
  if(toss == 1)digitalWrite(OUTPUT_8_PIN, HIGH);
  trigEndTime8 = currentTime + TRIG_DURATION;
  
  #ifdef DEBUG
    //DebugLED when plugged to the computer without clock in to test the fallback system
    digitalWrite(LED_BUILTIN, HIGH);
    trigEndTimeLED = currentTime + TRIG_DURATION;
  #endif
}


void clockPulseHandler() { //interrupt when clock in 
  //fallback = false;
  currentTime = micros();

  // Step 1: Calculate the interval 
  newInterval = currentTime - lastPulseTime;
  lastPulseTime = currentTime;
  needsTic = true;

}

void buttonPressHandler() { //interrupt when reset button press , only usefull in fallback 
    static unsigned long lastInterruptTime = 0;
    unsigned long interruptTime = millis();

    // Debounce: Ignore if interrupt occurs within 50ms of the previous one
    if (interruptTime - lastInterruptTime > 50) {
        currentBpmIndex++;
        if(currentBpmIndex > INTERVAL_LIST_SIZE -1)currentBpmIndex=0;
    }
    lastInterruptTime = interruptTime;
}


//not used 
void resetOutputs() {
  digitalWrite(OUTPUT_1_PIN, LOW);
  digitalWrite(OUTPUT_2_PIN, LOW);
  digitalWrite(OUTPUT_3_PIN, LOW);
  digitalWrite(OUTPUT_4_PIN, LOW);
  digitalWrite(OUTPUT_5_PIN, LOW);
  digitalWrite(OUTPUT_6_PIN, LOW);
  digitalWrite(OUTPUT_7_PIN, LOW);
  digitalWrite(OUTPUT_8_PIN, LOW);
}
//not used 
void allHigh(){
  digitalWrite(OUTPUT_1_PIN, HIGH);
  digitalWrite(OUTPUT_2_PIN, HIGH);
  digitalWrite(OUTPUT_3_PIN, HIGH);
  digitalWrite(OUTPUT_4_PIN, HIGH);
  digitalWrite(OUTPUT_5_PIN, HIGH);
  digitalWrite(OUTPUT_6_PIN, HIGH);
  digitalWrite(OUTPUT_7_PIN, HIGH);
  digitalWrite(OUTPUT_8_PIN, HIGH);  
}
