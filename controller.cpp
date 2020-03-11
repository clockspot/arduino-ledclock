//controller.cpp

#include "Arduino.h"
#include "config.h" //includes consts used here
//#include "controller.h" //definitions for your own functions - needed only if calling functions before they're defined
#include NETWORK_H //networkStartAdmin etc

void ctrlEvt(uint8_t ctrl, uint8_t evt){ //called by the input interface
  Serial.print(F("Btn "));
  switch(ctrl){
    case mainSel:   Serial.print(F("mainSel ")); break;
    case mainAdjDn: Serial.print(F("mainAdjDn ")); break;
    case mainAdjUp: Serial.print(F("mainAdjUp ")); break;
    case altSel:    Serial.print(F("altSel ")); break;
    default: break;
  }
  switch(evt){
    case 1: Serial.println(F("pressed")); break;
    case 2: Serial.println(F("short-held")); break;
    case 3: Serial.println(F("long-held")); break;
    case 0: Serial.println(F("released")); Serial.println(); break;
    default: break;
  }
  if(ctrl==mainSel && evt==4) networkStartAdmin(); //very long hold

} //end ctrlEvt

//Serial input for control
int incomingByte = 0;
void checkSerialInput(){
  if(Serial.available()>0){
    incomingByte = Serial.read();
    switch(incomingByte){
      case 97: //a
        networkStartAdmin(); break;
        //networkStartAP(); break;
      case 119: //w
        networkStartWiFi(); break;
      case 100: //d
        networkDisconnectWiFi(); break;
      default: break;
    }
  }
}
