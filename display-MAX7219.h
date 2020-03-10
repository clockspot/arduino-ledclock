#ifndef DISPLAY_MAX7219_HEADER
#define DISPLAY_MAX7219_HEADER

#include "Arduino.h"

void displaySetup();
void displayLoop();
void displayTime(unsigned long mils, bool showSeconds);
void displayClear();
void displayByte(byte b);
void displayToggleBrightness();

#endif