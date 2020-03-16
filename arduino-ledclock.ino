// Digital clock code for Arduino Nano 33 IoT and a chain of four MAX7219 LED display units
// Sketch by Luke McKenzie (luke@theclockspot.com) - https://github.com/clockspot/arduino-ledclock
// Based largely on public domain and Arduino code, with other sources mentioned herein

//Software version
const byte vMajor = 0;
const byte vMinor = 0;
const byte vPatch = 1;

#include "config.h"
#include RTC_H
#include INPUTS_H
#include NETWORK_H
#include DISPLAY_H
#include "controller.h"

void setup() {
  Serial.begin(9600);
  while(!Serial); //only works on 33 IOT
  delay(1000);
  displaySetup();
  rtcSetup();
  inputsSetup();
  networkSetup();
}

void loop() {
  checkSerialInput(); //controller
  rtcLoop();
  inputsLoop();
  networkLoop();
  displayLoop();
}

String getSoftwareVersion(){
  String out = ""; out+=vMajor; out+="."; out+=vMinor; out+="."; out+=vPatch; return out;
}