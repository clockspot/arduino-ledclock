//inputs-LSM6DS3.cpp

#include "Arduino.h"
#include <Arduino_LSM6DS3.h>
#include "config.h" //includes consts used here
#include "inputs-LSM6DS3.h" //definitions for your own functions - needed only if calling functions before they're defined
#include "controller.h" //ctrlEvt

// Hardware inputs and value setting
byte btnCur = 0; //Momentary button currently in use - only one allowed at a time
byte btnCurHeld = 0; //Button hold thresholds: 0=none, 1=unused, 2=short, 3=long, 4=verylong, 5=superlong, 10=set by btnStop()
unsigned long inputLast = 0; //When a button was last pressed
unsigned long inputLast2 = 0; //Second-to-last of above
//TODO the math between these two may fail very rarely due to millis() rolling over while setting. Need to find a fix. I think it only applies to the rotary encoder though.
int inputLastTODMins = 0; //time of day, in minutes past midnight, when button was pressed. Used in paginated functions so they all reflect the same TOD.

//IMU "debouncing"
int imuYState = 0; //the state we're reporting (-1, 0, 1)
int imuYTestState = 0; //the state we've actually last seen
int imuYTestCount = 0; //how many times we've seen it
int imuZState = 0; //the state we're reporting (-1, 0, 1)
int imuZTestState = 0; //the state we've actually last seen
int imuZTestCount = 0; //how many times we've seen it


void inputsSetup(){
  if(!IMU.begin()){ Serial.println("Failed to initialize IMU!"); while(1); }
  // Serial.print("Accelerometer sample rate = ");
  // Serial.print(IMU.accelerationSampleRate());
  // Serial.println(" Hz");
  // Serial.println();
  // Serial.println("Acceleration in G's");
  // Serial.println("X\tY\tZ");

  // vertical: x=1, y=0, z=0
  // forward: x=0, y=0, z=1
  /// f a bit  0.8  0   0.7
  // bakward: x=0. y=0, z=-1
  // b a bit   0.8  0 -0.5
  //left:     x=0, y=1, z=0
  //right:          -1
  //upside     -1    0    0
}
void inputsLoop(){
  float x, y, z;
  IMU.readAcceleration(x,y,z);
  int imuState;
  //Assumes Arduino is oriented with components facing back of clock, and USB port facing up
       if(y<=-0.5) imuState = -1;
  else if(y>= 0.5) imuState = 1;
  else if(y>-0.3 && y<0.3) imuState = 0;
  else imuState = imuYTestState; //if it's not in one of the ranges, treat it as "same"
  if(imuYTestState!=imuState){ imuYTestState=imuState; imuYTestCount=0; }
  if(imuYTestCount<imuTestCountTrigger){ imuYTestCount++; /*Serial.print("Y "); Serial.print(imuYTestState); Serial.print(" "); for(char i=0; i<imuYTestCount; i++) Serial.print("#"); Serial.println(imuYTestCount);*/ if(imuYTestCount==imuTestCountTrigger) imuYState=imuYTestState; }
  
       if(z<=-0.5) imuState = -1;
  else if(z>= 0.5) imuState = 1;
  else if(z>-0.3 && z<0.3) imuState = 0;
  else imuState = imuZTestState;
  if(imuZTestState!=imuState){ imuZTestState=imuState; imuZTestCount=0; }
  if(imuZTestCount<imuTestCountTrigger){ imuZTestCount++; /*Serial.print("Z "); Serial.print(imuZTestState); Serial.print(" "); for(char i=0; i<imuZTestCount; i++) Serial.print("#"); Serial.println(imuZTestCount);*/ if(imuZTestCount==imuTestCountTrigger) imuZState=imuZTestState; }
  
  
  //TODO change this to millis / compare to debouncing code
  checkBtn(mainSel);
  checkBtn(mainAdjUp);
  checkBtn(mainAdjDn);
  checkBtn(altSel);
}


bool readInput(byte btn){
  switch(btn){
    //Assumes Arduino is oriented with components facing back of clock, and USB port facing up
    //!() necessary since the pushbutton-derived code expects false (low) to mean pressed
    case mainSel:   return !(imuZState < 0); //clock tilted backward
    case mainAdjDn: return !(imuYState > 0); //clock tilted left
    case mainAdjUp: return !(imuYState < 0); //clock tilted right
    case altSel:    return !(imuZState > 0); //clock tilted forward
    default: break;
  }
} //end readInput

void checkBtn(byte btn){
  //Changes in momentary buttons, LOW = pressed.
  //When a button event has occurred, will call ctrlEvt
  bool bnow = readInput(btn);
  unsigned long now = millis();
  //If the button has just been pressed, and no other buttons are in use...
  if(btnCur==0 && bnow==LOW) {
    btnCur = btn; btnCurHeld = 0; inputLast2 = inputLast; inputLast = now; //inputLastTODMins = tod.hour()*60+tod.minute();
    ctrlEvt(btn,1); //hey, the button has been pressed
  }
  //If the button is being held...
  if(btnCur==btn && bnow==LOW) {
    if((unsigned long)(now-inputLast)>=btnSuperLongHold && btnCurHeld < 5) { //account for rollover
      btnCurHeld = 5;
      ctrlEvt(btn,5); //hey, the button has been long-held
    }
    else if((unsigned long)(now-inputLast)>=btnVeryLongHold && btnCurHeld < 4) { //account for rollover
      btnCurHeld = 4;
      ctrlEvt(btn,4); //hey, the button has been long-held
    }
    else if((unsigned long)(now-inputLast)>=btnLongHold && btnCurHeld < 3) { //account for rollover
      btnCurHeld = 3;
      ctrlEvt(btn,3); //hey, the button has been long-held
    }
    else if((unsigned long)(now-inputLast)>=btnShortHold && btnCurHeld < 2) {
      btnCurHeld = 2;
      ctrlEvt(btn,2); //hey, the button has been short-held
    }
  }
  //If the button has just been released...
  if(btnCur==btn && bnow==HIGH) {
    btnCur = 0;
    if(btnCurHeld < 10) ctrlEvt(btn,0); //hey, the button was released //4 to 10
    btnCurHeld = 0;
  }
}
void btnStop(){
  //In some cases, when handling btn evt 1/2/3, we may call this so following events 2/3/0 won't cause unintended behavior (e.g. after a fn change, or going in or out of set) //4 to 10
  btnCurHeld = 10;
}