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


//   Lock Servo       Switches (max 6 + power)       Door switch
//  +----------------------------------------------------------+
//  |  9              8  7  6  5  4  3               2         |
//  |                  (Pro Mini PCB)                     FDTI |
//  |  10             11 12 13 14 15 16              17        |
//  +----------------------------------------------------------+
//   Speaker          Corresponding LEDs             Power switch's LED

#define doorSwitchPin 2         // Momentary SPST NO, but closed (low) when door is closed.
#define servoPin      9         // Physical latch servo. Lock/unlock position is #define'd above
#define speakerPin    10        // Speaker to play tones
#define firstLEDPin   17        // This LED is active while the power is on, because
                                // the first button (a SPST toggle) is also the power button.
                                // Why not just have an LED on VCC? To facilitate a low power
                                //   sleep mode in the sketch. Kids forget to turn things off.

// Power - EStop with key to reset to closed (popped out). Push to turn off power. Red LED on box.
// 8: Knife switch.                     Pin 11: Hard disk blue LEDs 15ma
// 7: Big green pushbutton.             Pin 12: Built in green LED  6ma
// 6: Covered missile switch. Pulldown. Pin 13: Built in green LED (sinked) 6ma + onBoard LED on some Arduinos
// 5: Magnetic reed switch.             Pin 14: Green LED on side 6ma
// 4: Yellow arcade game pushbutton     Pin 15: Internal yellow LED. 15ma
// 3: N/C - unused                      Pin 16: N/C - unused   


// Constants

const byte    LEDPins[]     = {11, 12, 13, 14, 15}; // LEDs corresponding to buttonPins' switches    
const boolean LEDOnLevel[]  = {1,  1,  0,  1,  1 }; // Each LED is ON when this logic level is output
const byte    buttonPins[]  = {8,  7,  6,  5,  4 }; // Array of the pins monitored
const boolean btnOnLevel[]  = {0,  0,  1,  0,  0 }; // Active (or for momentary, "pushed") state is HIGH or LOW?
                                                    //   E.g. A Normally Open w/pullup resistor reads 1 (HIGH) 
                                                    //   when inactive and 0 when pushed. Enter 0.
#define MOMENTARY 1
#define TOGGLE 0
const byte buttonTypes[]    = {TOGGLE, MOMENTARY, TOGGLE, TOGGLE, MOMENTARY};  // Type of switch
const byte numToggles       = sizeof(buttonPins) / sizeof(buttonPins[0]);

const int debounceDelay = 10;   // milliseconds to wait until a button is stable
const unsigned int cMajorFreqs[] = {NOTE_C4, NOTE_D4, NOTE_E4, NOTE_F4, NOTE_G4, NOTE_A4, NOTE_B4}; // First is NOTE_C4
const unsigned int chromaticFreqs[] = {NOTE_C2, NOTE_CS2, NOTE_D2, NOTE_DS2, NOTE_E2, NOTE_F2, NOTE_FS2, NOTE_G2, NOTE_GS2, NOTE_A2, NOTE_AS2, NOTE_B2, NOTE_C3, NOTE_CS3, NOTE_D3, NOTE_DS3, NOTE_E3, NOTE_F3, NOTE_FS3, NOTE_G3, NOTE_GS3, NOTE_A3, NOTE_AS3, NOTE_B3, NOTE_C4, NOTE_CS4, NOTE_D4, NOTE_DS4, NOTE_E4, NOTE_F4, NOTE_FS4, NOTE_G4, NOTE_GS4, NOTE_A4, NOTE_AS4, NOTE_B4};

#define servoUnlockedPos 70     // Calibrade to the servo arm position when locked
#define servoLockedPos   160    // Calibrade to the servo arm position when unlocked

// Devices
Servo lockServo;

// Global variables
boolean btnStates[]         = {0,  0,  0,  0,  0 }; // Is each toggle currently on or off?
unsigned int count = 0;        // Number of toggles activated this run of the loop
unsigned int lastCount = 0;    // Number of toggles activated last run of the loop
boolean locked = true;         // Is the door lock servo currently in the locked position
boolean initialized = false;   // Are we done with setup()?
enum state {                   // State machine for conditional actions on transitions
  CLOSED_LOCKED,
  CLOSED_UNLOCKED,
  OPEN
} lastState;

boolean wasSleeping;           // Track that we were in sleep mode so we can re-enable things
unsigned long lastAction;      // The millis() when user input last happened. This supports
                               //   sleeping when the toy isn't being played with.
#define sleepInterval 1000     // How long (ms) to sleep between wakeups that re-check for input




void setup()                   // Set up code called once on start-up
{  
  // Define pin modes
  pinMode(servoPin, OUTPUT);
  pinMode(speakerPin, OUTPUT);
  pinMode(doorSwitchPin, INPUT);
  digitalWrite(doorSwitchPin, HIGH);                        // activate Arduino internal 20K pull-up resistor
  
  for (byte i = 0; i < numToggles; i++) {
    pinMode(buttonPins[i], INPUT);
    if (~btnOnLevel[i]) digitalWrite(buttonPins[i], HIGH);  // activate Arduino internal 20K pull-up resistor
    pinMode(LEDPins[i], OUTPUT);        
    digitalWrite(LEDPins[i], !LEDOnLevel[i]);               // Init all LEDs to off
  }
  
  // Set first LED high. The LED associated with the power switch is controlled
  // by a pin so we can sleep and eliminate most current consumption
  digitalWrite(firstLEDPin, HIGH);
  
  // At power on, if the door is closed, lock it. If the door is open, unlock it.
  lockServo.attach(servoPin);
  
  Serial.begin(57600);                                      // For debug print statements over serial port
  DEBUG_PRINT("\n\nDoor is closed? ");
  DEBUG_PRINTLN(doorClosed());
  
  if (doorClosed()) lock(); else unlock();                  // Init locking servo position
  
  playStartupNoise();               // Random clicks... hmmmm intriguing
  
  lastAction = millis();            // Initialize our inactivity / power save mode timer
  
  initialized = true;               // Done setting up. Actions (like lock()) will now make noises.
  
  DEBUG_PRINTLN("\n\nInitialized! \n");
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

void loop()                            // Main code, to run repeatedly
{
  if ( doorClosed() && locked )        // Main state: door is close and locked
  {
    readButtons();                     // Loop through all buttons. Set the "btnStates" variable.
    
    if (lastState != CLOSED_LOCKED)    // First entry into this state
    {
      DEBUG_PRINTLN("State: Door closed, locked.");
      setLEDs(btnStates);              // This call handles when we resume indicating status
                                       // from some other state, such as when closing the door
    }
    
    if (buttonsChanged()) 
    {
      somethingHappened();             // Reset power-save inactivity timer
      setLEDs(btnStates);              // This call handles when a button has been toggled
      debugPrintStates();
      playProgressNoise();             // Makes different noises depending on whether the button change was good or bad
    }
    
    if (allActivated()) unlock();      // All numToggles buttons have been activated, unlock the door!
    
    lastState = CLOSED_LOCKED;         // Poor C man's state machine
    sleepIfNoActionsFor(60L*1000L);    // Enter low power sleep if we haven't seen action for a minute
                                       //   Continue to poll buttons every second.
  }
  
  else if ( doorClosed() && !locked )  // Door is closed and unlocked
  {
    if (lastState != CLOSED_UNLOCKED)
    {
      DEBUG_PRINTLN("State: Door closed, and unlocked");
      playUnlockNoise();
      somethingHappened();             // Reset power-save inactivity timer
    }
    lastState = CLOSED_UNLOCKED;
    blinkAllUntilOpened();             // Alternate blinking all button LEDs to invite opening the door
  }
  
  else if ( doorOpen() )               // Door is open. Patiently wait for it to close. Also handles this 
                                       //   state's call to sleepIfNoActionsFor() for power-saving.
  {
    DEBUG_PRINTLN("State: Door open");
    
    somethingHappened();               // Reset power-save inactivity timer
    unlock();                          // Seems redundant (aren't we already unlocked if the door is open?), 
                                       //   but it's necessary for door switch alignment offsets (kids forcing mechanisms)
    resetBtnStates();                  // You opened the door. Un-activate momentaries.
    
    if (lastState == CLOSED_UNLOCKED) playDoorOpen();  // This prevents an opening noise from being played
                                                       //   when a power-on occures while the door is open
    lastState = OPEN;
    waitForClosed();                   // Also handles this state's call to sleepIfNoActionsFor() for power-saving.
  }  
}




//***********************************************************************
//
// INPUT
//

// Main state check. Called in a fast loop while active, slow polled while in power-save.
void readButtons()
{
  lastCount = count;  // Store the number of toggles activated from last time
  
  // Record the buttons' states from reading each button;
  for (int i = 0; i < numToggles; i++)
  {
    btnStates[i] = readButton(i);
    
//    DEBUG_PRINT("Reading button index: ");
//    DEBUG_PRINT(i);
//    DEBUG_PRINT(". Read normalized value of:");
//    DEBUG_PRINTLN(btnStates[i]);
  }
  
  count = numberActivated();  // Store the number of toggles activated now
}


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
      return !btnStates[buttonIndex];       // We're toggled; We're the opposite of whatever we were
    } else {
      return btnStates[buttonIndex];        // Not pushed, so return whatever toggled state it's already in
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
// ACTIONS
//

void lock()
{
  DEBUG_PRINTLN("Locking.");
  lockServo.write(servoLockedPos);
  locked = true;
  if (initialized) playLockNoise();    // Initial setup() might call lock() to set the servo.
                                       //   No need to play a noise indicating action occured.
}


void unlock()
{
  DEBUG_PRINTLN("Unlocking.");
  lockServo.write(servoUnlockedPos);
  locked = false;
}


// The door is closed but the servo lock is unlocked.
// Blink all leds to celebrate, until the door is opened or we enter power-save.
void blinkAllUntilOpened() {
  turnOnOddLEDs();  // Door animates every other LED while waiting to be opened
  
  // Monitor door for opening or switches changing, ~ half a second at a time
  long startWaiting = millis();     // To stop the LED dance after a set time, record current time.
  while ( doorClosed() && allActivated() )
  {
    Narcoleptic.delay(500);
    if (millis() > startWaiting + 2000)  // Tested - this is about 20 seconds because Narcoleptic pauses millis()
    {
      setLEDs(0);                   // Turn off LEDs because >30 seconds of blinking has passed 

      sleepIfNoActionsFor(2000L);   // Low power sleep, not listening to inputs 1s at a time
                                    // Note that the use of Narcoleptic above pauses millis();
                                    // So this 2000L ms isn't a real 2000 ms 
    } else {
      toggleAllLEDs();              // Animate the LEDs every half second  
    }
    readButtons();
  }
  somethingHappened();              // Door opened or buttons changed 
  setLEDs(0);                       // So cancel alernating LEDs...
  digitalWrite(firstLEDPin, LOW);   // But the power LED should be on in case we were sleeping
  
  // The door wasn't opened but the buttons changed; re-lock and restore the LEDs
  if (buttonsChanged() && doorClosed())
  {
    lock();
    setLEDs(btnStates);
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
  digitalWrite(firstLEDPin, HIGH); // If sketch is still running, then the first switch's LED (power LED) should be on
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


// Overload function for convenience. Set all LEDs to on or off.
void setLEDs(int state)
{
  if (state == 0 || state == 1)   // Boolean cast? Eh? Well I'm a ruby guy and I'll ask Alex
                                  //   someday, who's a bad ass Linux kernal guy. But he's never
                                  //   quite helpful with Arduino C because he references these.. nevermind.
                                  //   I typed "references" then realized I barely know how to pass-by-reference.
  {
    for (int i = 0; i < numToggles; i++)
    {
      digitalWrite(LEDPins[i], state ^ !LEDOnLevel[i]);
    }
  }
}

void turnOnOddLEDs()
{
  const boolean odds[] = {0, 1, 0, 1, 0, 1};     // Template
  boolean vector[numToggles];                    // Sized to our circuit
  
  // Only copy as much of the template as we have LEDs
  memcpy(vector, &odds[0], numToggles*sizeof(*odds));  // Alex would be proud.
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
// NOISES
//

// Play an intriguing series of clicks
void playStartupNoise()
{
  randomSeed(analogRead(0));    // Init our pseudo-random with some [unreliable] electrical noise
  
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


// Play ascending major triads
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


// Play a minor descending triad. Lowest triad roots correspond to the most progress lost
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


// Distorted ascending glissando
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


// Door is closed, re-locking. Play a quick minor progression, semi spy theme-ish
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



//***********************************************************************
//
// CONVENIENCE FUNCTIONS
//

int numberActivated()
{
  int count = 0;
  for (byte i = 0; i < numToggles; i++) {   // There's probably a slicker bit vector way
    if (btnStates[i]) count++;              //   ... when lazy, claim "well, for readability"
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


// In arduino you often delay after calling tone()
void toneWrapper(int pin, unsigned int freq, unsigned long duration)
{
  tone(pin, freq, duration);
  delay(duration);
}


// If we tasted sweet success, activated all toggles, and opened the door, we
//   want to reset the button states variable to 0 so it's not automatically unlocked when 
//   we close the door again. Since we'll re-read all buttons and most likely find the
//   toggles in the activated positions, the effect is that only the momentary switches 
//   really need to be re-activated to unlock again.
void resetBtnStates()
{
  for (int i=0; i<numToggles; i++)
  {
    btnStates[i] = 0;
  }
}



//***********************************************************************
//
// UTILITIES
//

// We record when something last happened that indicates user input.
//   This resets our inactivity timer
void somethingHappened()
{
  lastAction = millis();
  if (wasSleeping)              // Re-enable stuff that was turned off
  {
    wasSleeping = false;
    
    digitalWrite(firstLEDPin, HIGH);  // Turn back on the "power" LED
    lockServo.attach(servoPin);       // Re-enable servo
    setLEDs(btnStates);               // restore the LEDs
  }
}

// If more than "duration" milliseconds have passed since something happened,
//   sleep for sleepInterval ms (e.g. 1000 ms) to, in effect, poll the inputs 
//   on a longer period and save power.
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

//   Debounce a pin by waiting for a stable state
//   Original Credit: Arduino Cookbook
//   Modified to handle N.O. switches with either pulldowns or pullups
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
    DEBUG_PRINT(btnStates[i]);
  }
  DEBUG_PRINTLN("       <-- btnStates");
    
  DEBUG_PRINT("lastCount:");
  DEBUG_PRINTLN(lastCount);
  
  DEBUG_PRINT("count:");
  DEBUG_PRINTLN(count);
  
  DEBUG_PRINT("Door open? ");
  DEBUG_PRINTLN(doorOpen());
  
  DEBUG_PRINT("Locked? ");
  DEBUG_PRINTLN(locked);
}
