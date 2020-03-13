//display-MAX7219.h

#ifndef DISPLAY_MAX7219_H
#define DISPLAY_MAX7219_H

void displaySetup();
void displayLoop();
void displayTime(unsigned long mils, bool showSeconds);
void displayClear();
void displayInt(int n);
int displayToggleBrightness();

#endif