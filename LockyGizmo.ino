/*
  Locky Gizmo
                                                         
  Author: Jeff Vyduna
  Contact: web@jeffvyduna.com                        
  License: MIT
  
  Program Description: See README.md
             
  Revisions:
  
  - 0.1 (December, 2013): Initial release
  
*/


#define DEBUG                   // Debugging print utilities. Comment this line for "production"
#include "DebugUtils.h"

#include <Servo.h>              // Servo PWM control.
#include "pitches.h"            // Pitch constants, e.g. #define NOTE_FS2 93
#include "Narcoleptic.h"        // Low power sleep by Peter Knight.
                                // https://code.google.com/p/narcoleptic/

//Servo servo;                  // create servo object to control a servo 
#define servoUnlockedPos 20     // Calibrade to the servo arm position when locked
#define servoLockedPos   160    // Calibrade to the servo arm position when unlocked
#define MOMENTARY 1
#define TOGGLE 0

const int debounceDelay = 10;   // milliseconds to wait until a button is stable

const unsigned int cMajorFreqs[] = {NOTE_C4, NOTE_D4, NOTE_E4, NOTE_F4, NOTE_G4, NOTE_A4, NOTE_B4}; // First is NOTE_C4
const unsigned int chromaticFreqs[] = {NOTE_C2, NOTE_CS2, NOTE_D2, NOTE_DS2, NOTE_E2, NOTE_F2, NOTE_FS2, NOTE_G2, NOTE_GS2, NOTE_A2, NOTE_AS2, NOTE_B2, NOTE_C3, NOTE_CS3, NOTE_D3, NOTE_DS3, NOTE_E3, NOTE_F3, NOTE_FS3, NOTE_G3, NOTE_GS3, NOTE_A3, NOTE_AS3, NOTE_B3, NOTE_C4, NOTE_CS4, NOTE_D4, NOTE_DS4, NOTE_E4, NOTE_F4, NOTE_FS4, NOTE_G4, NOTE_GS4, NOTE_A4, NOTE_AS4, NOTE_B4};

//   Lock Servo  Switches (max 6 + power)  Door switch
//  +--------------------------------------------------+
//  |  9           8  7  6  5  4  3          2         |
//  |                (Pro Mini PCB)               FDTI |
//  |  10          11 12 13 14 15 16         17        |
//  +--------------------------------------------------+
//   Speaker     Corresponding LEDs        Power switch's LED
// Truncate unused end of array
// byte buttonPins[]  =  {8,  7,  6,  5,  4,  3}; 
// byte buttonPins[]  =  {11, 12, 13, 14, 15, 16}; 

#define doorSwitchPin 2         // Momentary SPST NO, but closed (low) when door is closed.
#define servoPin      9         // Physical latch servo. Lock/unlock position is #define'd above
#define speakerPin    10        // Speaker to play tones
#define firstLEDPin   17        // This LED is active while the power is on, because
                                // the first button (a SPST toggle) is also the power button.
                                // Why not just have an LED on VCC? To facilitate a low power
                                //   sleep mode in the sketch. People forget to turn things off.
// #define boardLedPin 13          // on-board LED (Uno)

// Power - EStop with key to reset to closed (popped out). Push to turn off power. Red LED on box.
// 8: Knife switch.                     Pin 11: Hard disk blue LEDs 15ma
// 7: Big green pushbutton.             Pin 12: Built in green LED  6maj
// 6: Covered missile switch. Pulldown. Pin 13: Built in green LED  6ma + onBoard LED
// 5: Magnetic reed switch.             Pin 14: Green LED on side 6ma
// 4: Yellow arcade game pushbutton     Pin 15: Fan and internal yellow LED. 50ma
// 3: N/C                               Pin 16: N/C


// Constants
const byte    LEDPins[]     = {11, 12, 13, 14, 15}; // LEDs corresponding to buttonPins' switches    
const boolean LEDOnLevel[]  = {1,  1,  0,  1,  1 }; // Each LED is ON when this logic level is output
const byte    buttonPins[]  = {8,  7,  6,  5,  4 }; // Array of the pins monitored
const boolean btnOnLevel[]  = {0,  0,  1,  0,  0 }; // Active (momentary: pushed) state is HIGH or LOW?
                                                    //   E.g. A Normally Open w/pullup resistor reads 1 (HIGH) 
                                                    //   when inactive and 0 when pushed. Enter 0.
const byte buttonTypes[]    = {TOGGLE, MOMENTARY, TOGGLE, TOGGLE, MOMENTARY};  // Type of switch
const byte numToggles       = sizeof(buttonPins) / sizeof(buttonPins[0]);

// Global variables
boolean states[]            = {0,  0,  0,  0,  0 }; // Is each toggle currently on or off?
unsigned int count = 0;        // Number of toggles activated this run of the loop
unsigned int lastCount = 0;    // Number of toggles activated last run of the loop
boolean locked = true;         // Is the door lock servo currently in the locked position
boolean initialized = false;   // Are we done with setup()?

boolean wasSleeping;           // Track that we were in sleep mode so we can re-enable things
unsigned long lastAction;      // The millis() when user input last happened. 
                               //   This supports us sleeping when we're not being played with.
#define sleepInterval 1000     // How long to sleep between wakeups to re-check for input

// Devices
Servo lockServo;

void setup()                   // Set up code called once on start-up
{  
  // Define pin modes
  #ifdef boardLedPin           // If on an Uno or other board with LED on 13, and we're using it
    pinMode(boardLedPin, OUTPUT);
    digitalWrite(boardLedPin, LOW);      // turn onboard UNO LED off
  #endif
  pinMode(servoPin, OUTPUT);
  pinMode(speakerPin, OUTPUT);
  pinMode(doorSwitchPin, INPUT);
  digitalWrite(doorSwitchPin, HIGH);                        // activate Arduino internal 20K pull-up resistor
  
  for (byte i = 0; i < numToggles; i++) {
    pinMode(buttonPins[i], INPUT);
    if (~btnOnLevel[i]) digitalWrite(buttonPins[i], HIGH);  // activate Arduino internal 20K pull-up resistor
    pinMode(LEDPins[i], OUTPUT);        
    digitalWrite(LEDPins[i], ~LEDOnLevel[i]);               // Init all LEDs to off
  }
  
  // Set first LED high. The LED associated with the power switch is controlled
  // by a pin so we can sleep and eliminate most current consumption
  digitalWrite(firstLEDPin, HIGH);
  
  // At power on, if the door is closed, lock it. If the door is open, unlock it.
  lockServo.attach(servoPin);
  if (doorClosed()) lock(); else unlock();
  
  playStartupNoise();
  
  lastAction = millis();            // Initialize our power saving timer
  
  initialized = true;               // Done setting up. Actions now make noises
  
  Serial.begin(57600);
  DEBUG_PRINTLN("\n\n Initialized! \n");
  DEBUG_PRINT("Monitoring ");
  DEBUG_PRINT(numToggles);
  DEBUG_PRINTLN(" toggles.\n");
}



//    LLL
//     LL    
//     LL   OOOOO    OOOOO    P PPPP
//     LL  OO   OO  OO   OO   PP   PP
//     LL  OO   OO  OO   OO   PP   PP
//     LLL  OOOOO    OOOOO    PPPPPP
//                            PP
//                            PP

void loop()                           // Main code, to run repeatedly
{

  if ( doorClosed() && locked )       // Main state: door is close and locked
  {
    DEBUG_PRINTLN("State: Door closed, locked.");
    readButtons();
    if (buttonsChanged()) 
    {
      somethingHappened();
      debugPrintStates();
      setLEDs(states);
      playProgressNoise();
    }
    if (allActivated()) unlock();
    sleepIfNoActionsFor(5L*60L*1000L); // Sleep a second if we haven't seen action for 5 minutes
  }
  else if ( doorClosed() && !locked )  // Door is closed and unlocked.
  {
    DEBUG_PRINTLN("State: Door closed, and unlocked");
    somethingHappened();
    blinkAllUntilOpened();
  }
  else if ( doorOpen() )               // Door is open. Patiently wait for it to close.
  {
    DEBUG_PRINTLN("State: Door open");
    somethingHappened();
    resetStates();
    playDoorOpen();
    waitForClosed();
  }  
}




//***********************************************************************
//
// STATE CHECKS
//

void readButtons()
{
  lastCount = count;  // Store the number of toggles activated from last time
  
  // Set states from reading each button;
  for (int i = 0; i < numToggles; i++)
  {
    states[i] = readButton(i);
    
//    DEBUG_PRINT("Reading button index: ");
//    DEBUG_PRINT(i);
//    DEBUG_PRINT(". Read normalized value of:");
//    DEBUG_PRINTLN(states[i]);
  }
  
  count = numberActivated();  // Store the number of toggles activated now
}


//***********************************************************************
//
// ACTIONS
//

void unlock()
{
  Serial.println("Unlocking.");
  lockServo.write(servoUnlockedPos);
  locked = false;
  if (initialized) playUnlockNoise();
}


void lock()
{
  Serial.println("Locking.");
  lockServo.write(servoLockedPos);
  locked = true;
  if (initialized) playLockNoise();
}


// Play an intriguing series of clicks
void playStartupNoise()
{
  randomSeed(analogRead(0));    // Unused analog pin in this particular sketch
  
  for (int i=0; i<10+random(10); i++)
  {
    tone(speakerPin, random(150), random(50));
    delay(random(200));
  }
}

// Play noises that correspond to the number of toggles activated.
// Each progress noise is a run of notes ascending (more toggles activated)
//   or descending (toggles deactivated
void playProgressNoise()
{
  if ( count > 0 ) 
  {
    if ( lastCount < count )
      playPositiveProgress(count);
    else
      playNegativeProgress(count);
  } else // count == 0
  {
    playFailNoise(); // Make funny fail noise. Wah Wah
  }
}


void playPositiveProgress(unsigned int thisCount)
{
  const int totalDuration = 1000;  // ms. The whole noise should take this long to play
  int noteDuration = totalDuration/thisCount/3; // ms. Each note should nominally take this long to play.
 
  // Play as many major chords with chromatic ascending roots as we have toggles activated
  for (int i=0; i<thisCount; i++)
  {
    playMajorTriad(i, map(i,0,thisCount-1,60,150));  // Most recent added triad takes longer
  }
}


// Play a minor descending triad, with lowest roots for earliest progress lost
void playNegativeProgress(unsigned int count)
{
  playDescMinorTriad(count+12, 120);  // Start an octive up, 200 ms per note
}


// Play a major chord. The root is the index into a chromatic array.
//   duration is the number of milliseconds per note
void playMajorTriad(unsigned int root, unsigned int duration)
{
  toneWrapper(speakerPin, chromaticFreqs[root]*2, duration);
  toneWrapper(speakerPin, chromaticFreqs[root+4]*2, duration);
  toneWrapper(speakerPin, chromaticFreqs[root+7]*2, duration);
}


// Play a descending minor triad that ends on the given root
void playDescMinorTriad(unsigned int root, unsigned int duration)
{
  toneWrapper(speakerPin, chromaticFreqs[root+7], duration);
  toneWrapper(speakerPin, chromaticFreqs[root+3], duration);
  toneWrapper(speakerPin, chromaticFreqs[root], duration);
}


// Wah, wah, wah, waaaaaAaaAaaAAaAaA
void playFailNoise()
{
  const unsigned int duration = 60000L/120/4; 
  toneWrapper(speakerPin, NOTE_C4, duration);
  toneWrapper(speakerPin, NOTE_B3, duration);
  toneWrapper(speakerPin, NOTE_AS3, duration);
  
  const int cycles = 100;
  const int baseFrequency = NOTE_A3;
  int frequency;

  for (int i = 1; i<=cycles; i++)
  {
    frequency =  baseFrequency + i*((i % 9)*4 - 16)/cycles ; // Vibrato develops
    tone(speakerPin, frequency);
    delay(10);
  }
  noTone(speakerPin);
}


// Success. Accelerating major triads and a final high note.
void playUnlockNoise() 
{
  const int duration = 60000L/120/4;  // 120 BPM 4:4 half note
  
  for (int multiplier=1; multiplier <= 4; multiplier *= 2) // Integer frequency multiplies means octives
  {
    for (int octave=1; octave<=multiplier; octave*= 2)
    {
      for (int repeats=1; repeats<=multiplier; repeats++)
      {
        for (int i = 0; i <= 4; i += 2) // Major triad chord
        {
          toneWrapper(speakerPin, cMajorFreqs[i]*octave*repeats, duration/multiplier);
        }
      }
    }
  }
  toneWrapper(speakerPin, cMajorFreqs[0]*8, duration*3);  // Final high C
}

// Ascending, acceleration glissando
void playDoorOpen()
{
  for (int freq=70; freq<1000; freq +=2)
  {
    tone(speakerPin, freq, 2);
    delay(2);
    tone(speakerPin, freq);
  }
  noTone(speakerPin);
}

// Quick minor progression, semi spy-ish
void playLockNoise()
{
  const int duration = 60000L/120/8;  // 120 BPM quarter note
  
  toneWrapper(speakerPin, NOTE_C4, duration);
  toneWrapper(speakerPin, NOTE_DS4, duration);
  delay(duration/2);
  toneWrapper(speakerPin, NOTE_G4, duration);
  delay(duration*3/2);
  toneWrapper(speakerPin, NOTE_C5, duration/2);
  for (int i=0; i< 4; i++)
  {
    toneWrapper(speakerPin, NOTE_B4, duration/4);
    toneWrapper(speakerPin, NOTE_B3, duration/8);
    toneWrapper(speakerPin, NOTE_B4, duration/4);
    toneWrapper(speakerPin, NOTE_B5, duration/8);
  }
}


// The door is closed but the servo lock is unlocked.
//   blink all leds to celebrate, until the door is opened or power cycles
void blinkAllUntilOpened() {
  turnOnOddLEDs();  // Door animates every other LED waiting to be opened.
  // Monitor door for opening or switches changing, ~ half a second at a time
  
  long startWaiting = millis();  // Stop the LED dance after a set time
  
  while ( doorClosed() && allActivated() )
  {
    Narcoleptic.delay(500);
    if (millis() > startWaiting + 10000)  // Tested - this is about 80 seconds
    {
      setLEDs(0); // Turn off LEDs because >30 seconds of blinking has passed 

      sleepIfNoActionsFor(20000L);  // Low power sleep, not listening to inputs 1s at a time
                                   // Note use of Narcoleptic above even pauses millis();
                                   // So this 1000L ms isn't a real 1000 ms 
    } else {
      toggleAllLEDs();  // Animate the LEDs every half second  
    }
    readButtons();
  }
  setLEDs(0);  // Door opened or buttons changed - cancel alernating LEDs
  digitalWrite(firstLEDPin, LOW);  // But Power LED should be on
  somethingHappened();
  
  // Door not opened but the buttons changed - lock and resume LEDs
  if (buttonsChanged() && doorClosed())
  {
    lock();
    setLEDs(states);
  }
}

// The door is open. Wait in low-power for it to close.
void waitForClosed()
{
  setLEDs(0);
  digitalWrite(firstLEDPin, LOW);  // Even turn off the first "power" LED
  while (doorOpen())
  {
    Narcoleptic.delay(500);        // Check if door is still open every half second in low power sleep.
  }
  lock();                          // Door was closed - lock it
  digitalWrite(firstLEDPin, HIGH); // If sketch is still running, then first switch (power) is still on
}


// Set all LEDs to on or off
void setLEDs(int state)
{
  if (state == 0 || state == 1)
  {
    for (int i = 0; i < numToggles; i++)
    {
      digitalWrite(LEDPins[i], state ^ !LEDOnLevel[i]);
    }
  }
}

// Set all LEDs according to a binary vector where 1 = on
//   Normalizes the requested state against the LEDOnLevel const
void setLEDs(boolean vector[])
{
  for (int i=0; i < numToggles; i++)
  {
    digitalWrite(LEDPins[i], vector[i] ^ !LEDOnLevel[i]);
  }
}


void turnOnOddLEDs()
{
  const boolean odds[] = {0, 1, 0, 1, 0, 1};     // Template
  boolean vector[numToggles];                    // Sized to our circuit
  
  // Only copy as much of the template as we have LEDs
  memcpy(vector, &odds[0], numToggles*sizeof(*odds));  
  setLEDs(vector);
}


void toggleAllLEDs()
{
  for ( int i = 0; i < numToggles; i++ ) {
    digitalWrite(LEDPins[i], !digitalRead(LEDPins[i]));  // Toggle
  }
}



//***********************************************************************
//
// INPUT
//

// buttonIndex is which button to read, 0-indexed
// Waits (or times-out) for momentary buttons to be released
boolean readButton(int buttonIndex)
{
  byte buttonPin = buttonPins[buttonIndex];
  boolean momentary = buttonTypes[buttonIndex];
  
  if ( momentary )
  {
    if (debounce(buttonPin, btnOnLevel[buttonIndex]))             // Momentarily pressed
    {
      unsigned long startWait = millis();
      
      // wait for release or 2s timeout
      while ( debounce(buttonPin, btnOnLevel[buttonIndex]) &&
              millis() < (startWait + 2000) )
      {}
         
      if (millis() > startWait + 2000) {
        DEBUG_PRINT("***  Exception: Timed out waiting on momentary to release on pin ");
        DEBUG_PRINTLN(buttonPin);
      }
      return !states[buttonIndex];       // We're toggled; We're the opposite of whatever we were
    } else {
      return states[buttonIndex];        // Not pushed, so return whatever toggled state it's already in
    }
  }
  else // Toggle type: read directly
  {
    return debounce(buttonPin, btnOnLevel[buttonIndex]);
  }
}


// Momentary SPST is CLOSED (low) when door is physically closed, open otherwise.
boolean doorClosed()
{
  return debounce(doorSwitchPin, 0);
}



//***********************************************************************
//
// CONVENIENCE FUNCTIONS
//

int numberActivated()
{
  int count = 0;
  for (byte i = 0; i < numToggles; i++) {
    if (states[i]) count++;
  }
  return count;
}


// The buttons have changed if the number activated now is different
//   from how many were activated last time.
boolean buttonsChanged()
{
  return lastCount != count;
}


// All toggles have been activated
boolean allActivated()
{
  return count == numToggles;
}


boolean doorOpen()
{
  return !doorClosed();
}


// In arduino you have to delay after calling tone()
void toneWrapper(int pin, unsigned int freq, unsigned long duration)
{
  tone(pin, freq, duration);
  delay(duration);
}


// If we tasted sweet success, activated all toggles, and opened the door,
//   we want to reset states to 0 so it's not automatically unlocked when 
//   we close the door again.
void resetStates()
{
  for (int i=0; i<numToggles; i++)
  {
    states[i] = 0;
  }
}

//***********************************************************************
//
// UTILITIES
//

// We record when something last happened that indicates user input.
void somethingHappened()
{
  lastAction = millis();
  if (wasSleeping)              // Re-enable stuff that was turned off
  {
    wasSleeping = false;
    
    digitalWrite(firstLEDPin, HIGH);  // Turn back on the "power" LED
    lockServo.attach(servoPin);       // Re-enable servo
    setLEDs(states);                  // restore the LEDs
  }
}

// If more than "duration" milliseconds have passed since something happened
//   sleep for a second to effectively start polling the inputs on a longer period
void sleepIfNoActionsFor(unsigned long duration)
{
  if (millis()-lastAction > duration)
  { 
    wasSleeping = true;
    
    setLEDs(0);                       // Turn off LEDs
    lockServo.detach();               // Detach servo
    noTone(speakerPin);               // Just in case we were playing sound
    digitalWrite(firstLEDPin, LOW);   // Even turn off the first "power" LED
    
    Narcoleptic.delay(sleepInterval); // Sleep zzZZzzZzZZZZ - note, evem millis() stops
  }
}

// Original Credit: Arduino Cookbook
//   Modified to handle NO switches with either pulldowns or pullups
//   Debounce a pin by waiting for a stable state
boolean debounce(int pin, boolean onState)
{
  boolean state;
  boolean previousState;
  
  previousState = digitalRead(pin) ^ !onState;
  for(int counter=0; counter < debounceDelay; counter++)
  {
    delay(1); // wait for 1 millisecond 
    state = digitalRead(pin) ^ !onState; // read the pin
    if (state != previousState)
    {
      counter = 0; // reset the counter if the state changes
      previousState = state; // and save the current state 
    }
  }
    // here when the switch state has been stable longer than the debounce period 
  return state;
}

// Debugging print utility
void debugPrintStates()
{
  DEBUG_PRINTLN("\n\n");
  DEBUG_PRINTLN("01234  <-- btnIndex");
  for (int i = 0; i < numToggles; i++)
  {
    DEBUG_PRINT(states[i]);
  }
  DEBUG_PRINTLN("       <-- states");
    
  DEBUG_PRINT("lastCount:");
  DEBUG_PRINTLN(lastCount);
  
  DEBUG_PRINT("count:");
  DEBUG_PRINTLN(count);
  
  DEBUG_PRINT("Door open? ");
  DEBUG_PRINTLN(doorOpen());
  
  DEBUG_PRINT("Locked? ");
  DEBUG_PRINTLN(locked);
}
