/*
Implements Drag Racing (Christmas Tree) lights
MajicDesigns Feb 2023

Implementation
==============
- The software encodes the rules below with the 'lamps' implemented as NeoPixel LEDs
  managed by the FastLED library. 
- One active low digital input switch is used to start/reset the lights sequence.
- Pairs of active low digital inputs are used to signal the prestage and staged optical 
  beams per racer.
- Optionally, one digital input signals racer going past finish line if the timing mode 
  is enabled. At the end of the run the time results are printed to the serial monitor.

Light Rules
===========
Drag races are started electronically by a system known as a Christmas tree.
A common Christmas tree consists of a column of seven lights for each driver or lane,
as well as a set of light beams across the track itself.  The light beams are arranged
with one set on the starting line, and another set 7 inches behind it.

Each side of the column of lights is the same from the top down:
- two blue LED light set
- three amber bulbs
- one green bulb
- one red bulb.

When drivers are preparing to race, they first cross the beams 7 inches behind the 
starting line. Crossing this beam activates the top bulbs. At this point, with most
modern starting lights, the tree is activated.

Once pre-staged, drivers roll up 7 inches and cross the second beam on the starting 
line. Once the first driver activates the bottom bulbs, the subsequent drivers have 
seven seconds or they are timed out and automatically disqualified.

Once all drivers have crossed the staged sensor or are timed out, the automatic 
starting system will activate the next lighting sequence within 1.3 seconds of 
the last car being staged or being disqualified.

After this point, the lighting sequence will be different based on the type of 
tree and start that a race is using. 
 - A "Standard" tree lights up each amber light in sequence with a 500 ms delay between 
   them, followed by the green light after another 500 ms delay. 
 - A "Professional" tree lights up all the amber lights at the same time, followed
   by the green light after a 400 ms delay.
 - A "Hybrid" (or Professional 500) tree, is the same as a professional tree with a 500 ms
   delay before the green light.
   
On the activation of the green light from either style of tree, the drivers are supposed 
to start the race.

Leaving the "Staged" line before the green light activates will instantly stop the count 
down and result in a lighting of the red light and a provisional disqualification of the
offending driver.

The software will operate in any of the three modes. Change the mode using the compile time
definition TREE_MODE.

Hardware definitions
====================
- PIN_CONTROL defines the digital input to reset the lights sequence and enable 
the next one.
- Racer stage, foul and finish line beams digital inputs are defined in the racer[] array. 
- DATA_PIN defines the neopixel serial output data pin

Dependencies
============
MD_UISwitch library can be found at https://github.com/MajicDesign/MD_UISwitch or the IDE library manager
FastLED library can be found at https://fastled.io/ or the IDE library manager.
*/

#include <FastLED.h>
#include <MD_UISwitch.h>

// Define whether timing results option is enabled
// Set true = enabled, false = dsabled
#define TIMING_ENABLED true

// Define the running mode for the tree
// Set 0 = Standard, 1 = Professional or 2 = Hybrid
#ifndef TREE_MODE
#define TREE_MODE 0
#endif

// Set up parameters for different modes
#if TREE_MODE == 0  // Standard Mode
#warning "Compiling for STANDARD TREE"

const uint32_t STAGE_DELAY = 7000; // in milliseconds
const uint32_t READY_DELAY = 1300; // in milliseconds  
const uint32_t SET_DELAY = 500;    // in milliseconds
const uint32_t GO_DELAY = 500;     // in milliseconds

#elif TREE_MODE == 1  // Professional Mode
#warning "Compiling for PROFESSIONAL TREE"

const uint32_t STAGE_DELAY = 7000; // in milliseconds
const uint32_t READY_DELAY = 1300; // in milliseconds  
const uint32_t SET_DELAY = 0;      // in milliseconds
const uint32_t GO_DELAY = 400;     // in milliseconds

#elif TREE_MODE == 2  // Hybrid Mode
#warning "Compiling for HYBRID TREE"

const uint32_t STAGE_DELAY = 7000; // in milliseconds
const uint32_t READY_DELAY = 1300; // in milliseconds  
const uint32_t SET_DELAY = 0;      // in milliseconds
const uint32_t GO_DELAY = 500;     // in milliseconds

#endif

#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))

// Define the colors to use for the lights
const CRGB LAMP_OFF = CRGB::Black;
const CRGB LAMP_STAGING = CRGB::Blue;
const CRGB LAMP_STAGED = CRGB::Blue;
const CRGB LAMP_READY = CRGB::Orange;
const CRGB LAMP_GO = CRGB::Green;
const CRGB LAMP_FOUL = CRGB::Red;

// Define the offsets from arbitrary base for each set of lights
const uint8_t idStaging[] = { 0, 1 };   // LED offset for the staging lamp (LAMP_STAGING)
const uint8_t idStaged[] = { 2, 3 };    // LED number for the staged lamp (LAMP_STAGED)
const uint8_t idReady[] = { 4, 5, 6 };  // LED numbers for the ready lamps (LAMP_READY)
const uint8_t idGo = 7;                 // LED number for the go lamp (LAMP_GO)
const uint8_t idFoul = 8;               // LED number for the foul lamp (LAMP_FOUL)

const uint8_t LED_PER_TREE = 9;                     // Number of LEDS in each tree
const uint8_t STAGING_COUNT = ARRAY_SIZE(idStaging);// Number of LEDS in staging
const uint8_t STAGED_COUNT = ARRAY_SIZE(idStaged);  // Number of LEDS in staged
const uint8_t READY_COUNT = ARRAY_SIZE(idReady);    // Number of ready LEDS in ready

// Define what we need to keep track of for one tree of lamps
struct oneTree_t
{
  uint8_t idRacer;    // 0 .. NUM_RACERS-1. Which racer this tree is for.
  uint8_t idLEDBase;  // The first led number for this tree
};

// Define the data for all the lamp trees required
// There are only 2 types of trees (one per racer) but these may be displayed 
// multiple times (eg, front and back of a column, around the park, etc). 
// This array defines the number of trees required, which racer they 'belong'
// to and the starting neopixel address for the LED_PER_TREE leds for this tree.
// Altogether this is the total number of LEDS that the FastLED software has 
// to manage.
oneTree_t display[] =
{
  { 0, 0 },
  { 1, LED_PER_TREE * 1 },
  { 0, LED_PER_TREE * 2 },
  { 1, LED_PER_TREE * 3 },
};

// Derive some global constants from this array definition
const uint8_t NUM_TREES = ARRAY_SIZE(display);
const uint16_t NUM_LEDS = LED_PER_TREE * NUM_TREES;

// Create an array for the FastLED neopixel strip
CRGB led[NUM_LEDS];

// This is the LED type and other parameters for FastLED initialization
// For led chips like WS2812, which have 3 wires (data, ground, power) define 
// a data pin for comms to the LED string.
#define CHIPSET_TYPE WS2812B
const EOrder RGB_TYPE = GRB;
const uint8_t DATA_PIN = 3;

// Define what we need to keep track of for one racer
struct oneRacer_t
{
  // Keep statically (once only) initialised items together
  uint8_t pinStaged;  // input pin indicates staged when low
  uint8_t pinFoul;    // input pin indicates foul when low
#if TIMING_ENABLED
  uint8_t pinFinish;  // input pin indicates racer past finish line when low
#endif

  // Dynamically initialised items
  bool isStaged;      // true if the racer is staged
  bool isFoul;        // true if tree is showing foul
#if TIMING_ENABLED
  uint32_t timeResult; // timing result for foul or finish line
#endif
};

// Define the pin data for all the racers
// One line entry per racer. There are normally only 2 racers in a drag race
// but more can be defined if required without changes to the software.
// Data not statically initialized should be done in the RESET state.
oneRacer_t racer[] =
{
#if TIMING_ENABLED
  // Stage, Foul, Finish
  { 4, 5, 6 },
  { 7, 8, 9 },
#else
  // Stage, Foul
  { 4, 5 },
  { 7, 8 },
#endif
};

// Derive global constants from this array definition
const uint8_t NUM_RACERS = ARRAY_SIZE(racer);

// Define what we need to know to control the event
const uint8_t PIN_CONTROL = 10;
MD_UISwitch_Digital swControl(PIN_CONTROL);

void setLampsOff(void)
{
  for (auto i = 0; i < NUM_TREES; i++)
  {
    for (auto j=0; j<STAGING_COUNT; j++)
      led[display[i].idLEDBase + idStaging[j]] = LAMP_OFF;
    for (auto j=0; j<STAGED_COUNT; j++)
      led[display[i].idLEDBase + idStaged[j]] = LAMP_OFF;
    for (auto j=0; j<READY_COUNT; j++)
      led[display[i].idLEDBase + idReady[j]] = LAMP_OFF;
    led[display[i].idLEDBase + idGo] = LAMP_OFF;
    led[display[i].idLEDBase + idFoul] = LAMP_OFF;
  }
  FastLED.show();
}

void setLampsStaging(void)
{
  for (auto i = 0; i < NUM_TREES; i++)
  {
    for (auto j=0; j<STAGING_COUNT; j++)
      led[display[i].idLEDBase + idStaging[j]] = LAMP_STAGING;
  }
  FastLED.show();
}

void setLampsStaged(uint8_t racer)
{
  for (auto i = 0; i < NUM_TREES; i++)
  {
    if (display[i].idRacer == racer)
    {
      for (auto j=0; j<STAGED_COUNT; j++)
        led[display[i].idLEDBase + idStaged[j]] = LAMP_STAGED;
    }
  }
  FastLED.show();
}

void setLampsReady(uint8_t idx)
{
  for (auto i=0; i<NUM_TREES; i++)
    led[display[i].idLEDBase + idReady[idx]] = LAMP_READY;
  FastLED.show();
}

void setLampsGo(void)
{
  for (auto i=0; i<NUM_TREES; i++)
    if (!racer[display[i].idRacer].isFoul)
      led[display[i].idLEDBase + idGo] = LAMP_GO;
  FastLED.show();
}

void setLampsFoul(uint8_t racer)
{
  for (auto i=0; i<NUM_TREES; i++)
  {
    if (display[i].idRacer == racer)
      led[display[i].idLEDBase + idFoul] = LAMP_FOUL;
  }
  FastLED.show();
}

bool alreadyFoul(void)
// return true if at least one racer is fouled
{
  bool b = false;

  // check at least one already fouled
  for (auto i=0; i<NUM_RACERS; i++)
    b |= racer[i].isFoul;

  return(b);
}

bool checkForFalseStart(uint32_t timeStart, uint32_t timePeriod)
// return true if any racers have fouled
{
  bool b = false;

  // check the foul inputs
  for (auto i=0; i<NUM_RACERS; i++)
  {
    if ((digitalRead(racer[i].pinFoul) == LOW)  && !alreadyFoul())
    {
      setLampsFoul(i);
      racer[i].isFoul = b = true;
#if TIMING_ENABLED
      racer[i].timeResult = timePeriod - (millis() - timeStart);
#endif
    }
  }
  return(b);
}

#if TIMING_ENABLED

bool checkForFinish(uint32_t timeStart)
// return true is all racers are finished
{
  bool b = true;

  // check the finish inputs
  for (auto i=0; i<NUM_RACERS; i++)
  {
    // not fouled or not already recorded time
    if (!racer[i].isFoul && (racer[i].timeResult == 0))
    {
      if (digitalRead(racer[i].pinFinish) == LOW)   // this racer has finished
        racer[i].timeResult = millis() - timeStart;
      else
        b = false; // not finished, change return status
    }
  }

  return(b);
}

void showTimeResults(void)
{
  Serial.print(F("\n\nRace Result\n-----------"));
  for (uint8_t i = 0; i < NUM_RACERS; i++)
  {
    Serial.print(F("\nLane "));
    Serial.print(i+1);
    Serial.print(F(": "));
    if (racer[i].isFoul)
    {
      Serial.print(F("DQ"));        // disqualified
      if (racer[i].timeResult != 0) // reaction time recorded
      {
        Serial.print(F(" [-"));
        Serial.print(racer[i].timeResult);
        Serial.print(F("]"));
      }
    }
    else if (racer[i].timeResult == 0)
      Serial.print(F("DNF"));       // did not finish
    else
      Serial.print(racer[i].timeResult);
  }
}

#endif

void setup(void)
{
#if TIMING_ENABLED
  Serial.begin(57600);    // use this to output timing results
#endif
  FastLED.addLeds<CHIPSET_TYPE, DATA_PIN, RGB_TYPE>(led, NUM_LEDS);

  setLampsOff();

  for (auto i = 0; i < NUM_RACERS; i++)
  {
    pinMode(racer[i].pinStaged, INPUT_PULLUP);
    pinMode(racer[i].pinFoul, INPUT_PULLUP);
#if TIMING_ENABLED
    pinMode(racer[i].pinFinish, INPUT_PULLUP);
#endif
  }
  swControl.begin();
}

void loop(void)
{
  static enum { 
    RESET,        // reset variables for next run
    PRE_STAGE,    // wait for signal to enable tree
    STAGING,      // wait for all lanes to stage or time out
    WAIT_START,   // delay before start sequence
    START_READY,  // yellow light sequence 
    START_SET,    // delay before green
    START_GO,     // set green light
#if TIMING_ENABLED
    WAIT_FINISH,  // wait for racers to finish to get time
#endif
    WAIT_RESET,   // sequence ended, waiting for signal to reset
  } curState = RESET;

  static uint32_t timeStart = 0;
  static uint8_t count = 0;

  switch (curState)
  {
  case RESET:           // reset variables for next run
    for (auto i = 0; i < NUM_RACERS; i++)
    {
      racer[i].isStaged = racer[i].isFoul = false;
#if TIMING_ENABLED
      racer[i].timeResult = 0;
#endif
    }
    setLampsOff();
    curState = PRE_STAGE;
    break;

  case PRE_STAGE:       // wait for signal to enable tree
    if (swControl.read() == MD_UISwitch::KEY_PRESS)
    {
      setLampsStaging();
      timeStart = 0;
      curState = STAGING;
    }
    break;

  case STAGING:         // wait for lanes all lanes to stage or time out
    // check for timeout
    if (timeStart != 0 && (millis() - timeStart >= STAGE_DELAY))
    {
      // set everyone not staged to foul
      for (auto i=0; i<NUM_RACERS; i++)
      {
        if (!racer[i].isStaged)
        {
          setLampsFoul(i);
          racer[i].isFoul = true;
        }
      }
    }

    // check if racers are staged
    for (auto i=0; i<NUM_RACERS; i++)
    {
      // handle the digital input
      if (digitalRead(racer[i].pinStaged) == LOW)
      {
        setLampsStaged(i);
        racer[i].isStaged = true;
        if (timeStart == 0) timeStart = millis();
      }
    }

    // work out if everyone is staged or fouled
    count = 0;
    for (auto i=0; i<NUM_RACERS; i++)
      if (racer[i].isStaged or racer[i].isFoul)
        count++;

    if (count == NUM_RACERS)
    {
      timeStart = millis();
      curState = WAIT_START;
    }
    break;

  case WAIT_START:      // delay before start sequence
    checkForFalseStart(timeStart, READY_DELAY + (READY_COUNT*SET_DELAY) + GO_DELAY);

    if (millis() - timeStart >= READY_DELAY)
    {
      curState = START_READY;
      timeStart = millis();
      count = 0;
    }
    break;

  case START_READY:     // yellow light sequence 
    checkForFalseStart(timeStart, (READY_COUNT*SET_DELAY) + GO_DELAY);

    if (SET_DELAY == 0)   // all lights simultaneously
    {
      for (count=0; count<READY_COUNT; count++)
        setLampsReady(count);
    }
    else                  // lights sequenced with delay between
    {
      if (millis() - timeStart >= SET_DELAY)
      {
        setLampsReady(count);
        count++;
        timeStart = millis();
      }
    }

    // check if all lights are on
    if (count == READY_COUNT)
    {
      timeStart = millis();
      curState = START_SET;
    }
    break;

  case START_SET:       // delay before green
    checkForFalseStart(timeStart, GO_DELAY);

    // monitor the timer
    if (millis() - timeStart >= GO_DELAY)
      curState = START_GO;
    break;
    
  case START_GO:       // set green lights
    setLampsGo();
    timeStart = millis();
#if TIMING_ENABLED
    curState = WAIT_FINISH;
#else
    curState = WAIT_RESET;
#endif
    break;

#if TIMING_ENABLED
  case WAIT_FINISH:    // wait for all racers to finish or indicator to reset
    {
      bool allDone = false;

      // If controller termintes the run, finish it off now
      if (swControl.read() == MD_UISwitch::KEY_PRESS)
      {
        allDone = true;
        curState = RESET;
      }

      // When all racers have finished, display the results and wait for controller
      if (checkForFinish(timeStart))
      {
        allDone = true;
        curState = WAIT_RESET;
      }
      if (allDone) showTimeResults();
    }
    break;
#endif
    
  case WAIT_RESET:     // tree sequence ended, waiting for signal to reset
    if (swControl.read() == MD_UISwitch::KEY_PRESS)
      curState = RESET;
    break;

  default:
    curState = RESET;
    break;
  }
}
