/*
DebugUtils.h - Simple debugging utilities.

Ideas adapted from:
http://www.arduino.cc/cgi-bin/yabb2/YaBB.pl?num=1271517197

MIT License. Look it up.
*/

#ifndef DEBUGUTILS_H
#define DEBUGUTILS_H

#include <WProgram.h>

#ifdef DEBUG
  #define DEBUG_PRINTLN_VERBOSE(str)    \
    Serial.print(millis());     \
    Serial.print(": ");    \
    Serial.print(__PRETTY_FUNCTION__); \
    Serial.print(' ');      \
    Serial.print(__FILE__);     \
    Serial.print(':');      \
    Serial.print(__LINE__);     \
    Serial.print(' ');      \
    Serial.println(str);
  #define DEBUG_PRINT(str)   Serial.print(str);
  #define DEBUG_PRINTLN(str) Serial.println(str);
#else
  #define DEBUG_PRINTLN_VERBOSE(str)
  #define DEBUG_PRINT(str)
  #define DEBUG_PRINTLN(str)
#endif

#endif
