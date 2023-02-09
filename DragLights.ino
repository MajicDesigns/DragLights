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
- Racer prestage and staged beams digital inputs are defined in the racer[] array. 

Dependencies
============
MD_UISwitch library can be found at https://github.com/MajicDesign/MD_UISwitch or the IDE library manager
FASTLed can be found at https://fastled.io/ or the IDE library manager.
*/

#include <FastLED.h>
#include <MD_UISwitch.h>

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

// Define the offsets from base for each set of lights
const uint8_t idStaging = 0;            // LED offset for the staging lamp
const uint8_t idStaged = 1;             // LED number for the staged lamp
const uint8_t idReady[3] = { 2, 3, 4 }; // LED numbers for the ready lamps (yellow)
const uint8_t idGo = 5;                 // LED number for the go lamp (green)
const uint8_t idFoul = 6;               // LED number for the foul lamp (red)

const uint8_t LED_PER_TREE = 7;         // Number of LEDS in each tree

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
#define CHIPSET_TYPE WS2811
const EOrder RGB_TYPE = GRB;
const uint8_t DATA_PIN = 3;

// Define what we need to keep track of for one racer
struct oneRacer_t
{
  uint8_t pinStaged;  // input pin indicates staged when low
  uint8_t pinFoul;    // input pin indicates foul when low
  bool isStaged;      // true if the racer is staged
  bool isFoul;        // true if tree is showing foul
};

// Define the data for all the racers
// One line entry per racer. There are normally only 2 racers in a drag race
// but more can be defined if required without changes to the software.
oneRacer_t racer[] =
{
  { 4, 5, false, false },
  { 6, 7, false, false },
};

// Derive global constants from this array definition
const uint8_t NUM_RACERS = ARRAY_SIZE(racer);

// Define what we need to know to control the event
const uint8_t PIN_CONTROL = 8;

MD_UISwitch_Digital swControl(PIN_CONTROL);


void setLampsOff(void)
{
  for (auto i = 0; i < NUM_TREES; i++)
  {
    led[display[i].idLEDBase + idStaging] = LAMP_OFF;
    led[display[i].idLEDBase + idStaged] = LAMP_OFF;
    for (auto j=0; j<ARRAY_SIZE(idReady); j++)
      led[display[i].idLEDBase + idReady[j]] = LAMP_OFF;
    led[display[i].idLEDBase + idGo] = LAMP_OFF;
    led[display[i].idLEDBase + idFoul] = LAMP_OFF;
  }
  FastLED.show();
}

void setLampsPreStage(void)
{
  for (auto i=0; i<NUM_TREES; i++)
    led[display[i].idLEDBase + idStaging] = LAMP_STAGING;
  FastLED.show();
}

void setLampsStaged(uint8_t racer)
{
  for (auto i = 0; i < NUM_TREES; i++)
  {
    if (display[i].idRacer == racer)
      led[display[i].idLEDBase + idStaged] = LAMP_STAGED;
  }
  FastLED.show();
}

void setLampsReady(uint8_t idx)
{
  for (auto i = 0; i < NUM_TREES; i++)
    led[display[i].idLEDBase + idReady[idx]] = LAMP_READY;
  FastLED.show();
}

void setLampsGo(void)
{
  for (auto i = 0; i < NUM_TREES; i++)
    if (!racer[display[i].idRacer].isFoul)
      led[display[i].idLEDBase + idGo] = LAMP_GO;
  FastLED.show();
}

void setLampsFoul(uint8_t racer)
{
  for (auto i = 0; i < NUM_TREES; i++)
  {
    if (display[i].idRacer == racer)
      led[display[i].idLEDBase + idFoul] = LAMP_FOUL;
  }
  FastLED.show();
}

bool alreadyFoul(void)
{
  bool b = false;

  // check at least one already fouled
  for (auto i = 0; i < NUM_RACERS; i++)
    b |= racer[i].isFoul;

  return(b);
}

bool checkForFalseStart(void)
{
  bool b = false;

  // check the foul inputs
  for (auto i = 0; i < NUM_RACERS; i++)
  {
    if ((digitalRead(racer[i].pinFoul) == LOW)  && !alreadyFoul())
    {
      setLampsFoul(i);
      racer[i].isFoul = b = true;
    }
  }
  return(b);
}

void setup(void)
{
  FastLED.addLeds<CHIPSET_TYPE, DATA_PIN, RGB_TYPE>(led, NUM_LEDS);

  setLampsOff();

  for (auto i = 0; i < NUM_RACERS; i++)
  {
    pinMode(racer[i].pinStaged, INPUT_PULLUP);
    pinMode(racer[i].pinFoul, INPUT_PULLUP);
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
    WAIT_RESET,   // sequence ended, waiting for signal to reset
  } curState = RESET;

  static uint32_t timeStart = 0;
  static uint8_t count = 0;

  switch (curState)
  {
  case RESET:           // reset variables for next run
    for (auto i = 0; i < NUM_RACERS; i++)
      racer[i].isStaged = racer[i].isFoul = false;
    setLampsOff();
    curState = PRE_STAGE;
    break;

  case PRE_STAGE:       // wait for signal to enable tree
    if (swControl.read() == MD_UISwitch::KEY_PRESS)
    {
      setLampsPreStage();
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
    checkForFalseStart();

    if (millis() - timeStart >= READY_DELAY)
    {
      curState = START_READY;
      count = 0;
    }
    break;

  case START_READY:     // yellow light sequence 
    checkForFalseStart();

    if (SET_DELAY == 0)   // all lights simultaneously
    {
      for (count=0; count<ARRAY_SIZE(idReady); count++)
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
    if (count == ARRAY_SIZE(idReady))
    {
      timeStart = millis();
      curState = START_SET;
    }
    break;

  case START_SET:       // delay before green
    checkForFalseStart();

    // monitor the timer
    if (millis() - timeStart >= GO_DELAY)
      curState = START_GO;
    break;
    
  case START_GO:       // set green lights
    setLampsGo();
    curState = WAIT_RESET;
    break;
    
  case WAIT_RESET:     // tree sequence ended, waiting for signal to reset
    if (swControl.read() == MD_UISwitch::KEY_PRESS)
      curState = RESET;
    break;

  default:
    curState = RESET;
    break;
  }
}
