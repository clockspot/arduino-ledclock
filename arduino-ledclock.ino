// Digital clock code for Arduino Nano 33 IoT and a chain of four MAX7219 LED display units
// Sketch by Luke McKenzie (luke@theclockspot.com) - https://github.com/clockspot/arduino-ledclock
// Based largely on public domain and Arduino code, with other sources mentioned herein

#include "config.h"

void setup() {
  Serial.begin(9600); while(!Serial); //only works on 33 IOT
  // rtcSetup();
  // inputsSetup();
  // networkSetup();
  displaySetup();
}

void loop() {
  checkSerialInput();
  // rtcLoop();
  // inputsLoop();
  // networkLoop();
  displayLoop();
}

// void ctrlEvt(byte ctrl, byte evt){ //called by the input interface
//   Serial.print(F("Btn "));
//   switch(ctrl){
//     case mainSel:   Serial.print(F("mainSel ")); break;
//     case mainAdjDn: Serial.print(F("mainAdjDn ")); break;
//     case mainAdjUp: Serial.print(F("mainAdjUp ")); break;
//     case altSel:    Serial.print(F("altSel ")); break;
//     default: break;
//   }
//   switch(evt){
//     case 1: Serial.println(F("pressed")); break;
//     case 2: Serial.println(F("short-held")); break;
//     case 3: Serial.println(F("long-held")); break;
//     case 0: Serial.println(F("released")); Serial.println(); break;
//     default: break;
//   }
//   if(ctrl==mainSel && evt==4) networkStartAdmin(); //very long hold
//
// } //end ctrlEvt
//
//
//
// //Serial input for control
// int incomingByte = 0;
// void checkSerialInput(){
//   if(Serial.available()>0){
//     incomingByte = Serial.read();
//     switch(incomingByte){
//       case 97: //a
//         networkStartAdmin(); break;
//         //networkStartAP(); break;
//       case 119: //w
//         networkStartWiFi(); break;
//       case 100: //d
//         networkDisconnectWiFi(); break;
//       default: break;
//     }
//   }
// }
