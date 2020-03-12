//display-MAX7219.cpp
//based on Eberhard Farle's LedControl library - http://wayoda.github.io/LedControl

#include "Arduino.h"
#include <LedControl.h>
#include "config.h" //includes consts used here
#include "display-MAX7219.h" //definitions for your own functions - needed only if calling functions before they're defined

#define NUM_MAX 4
#define ROTATE 90
#define CLK_PIN 2 //D2, pin 20
#define CS_PIN 3 //D3, pin 21
#define DIN_PIN 4 //D4, pin 22
//#include "fonts.h" //TODO
LedControl lc=LedControl(DIN_PIN,CLK_PIN,CS_PIN,NUM_MAX);

int curBrightness = 7;

void displaySetup(){
  for(int i=0; i<NUM_MAX; i++) { lc.shutdown(i,false); lc.setIntensity(i,curBrightness); }
}
void displayLoop(){} //it handles its own cycling and just needs updating as necessary

//See commit fb1419c for binary counter display test
//See commit 67bc64f for more font options

byte smallnum[30]={ //5px high, squared off, but for rounded
  B11111000, B10001000, B11111000, // 0
  B00010000, B11111000, B00000000, // 1 serif
  B11101000, B10101000, B10111000, // 2
  B10001000, B10101000, B11111000, // 3
  B01110000, B01000000, B11111000, // 4
  B10111000, B10101000, B11101000, // 5
  B11111000, B10101000, B11101000, // 6
  B00001000, B00001000, B11111000, // 7
  B11111000, B10101000, B11111000, // 8
  B10111000, B10101000, B11111000  // 9
};
byte bignum[50]={ //chicagolike 5x8
  B01111110, B11111111, B10000001, B11111111, B01111110, // 0
  B00000000, B00000100, B11111110, B11111111, B00000000, // 1
  B11000010, B11100001, B10110001, B10011111, B10001110, // 2
  B01001001, B10001101, B10001111, B11111011, B01110001, // 3
  B00111000, B00100100, B11111110, B11111111, B00100000, // 4
  B01001111, B10001111, B10001001, B11111001, B01110001, // 5
  B01111110, B11111111, B10001001, B11111001, B01110000, // 6 more squared tail
//B01111100, B11111110, B10001011, B11111001, B01110000, // 6 original
  B00000001, B11110001, B11111001, B00001111, B00000111, // 7
  B01110110, B11111111, B10001001, B11111111, B01110110, // 8
  B00001110, B10011111, B10010001, B11111111, B01111110  // 9 more squared tail
//B00001110, B10011111, B11010001, B01111111, B00111110  // 9 original
};

bool TESTSecNotMin = 0; //display seconds instead of minutes
void displayTime(unsigned long mils, bool showSeconds){
  if(mils>=86400000) mils=0;
  int todHrs = (mils/1000)/3600; //tod.getHours() - 43195/3600 = 11hr
  int todMin = (TESTSecNotMin? (mils/1000)%60: ((mils/1000)/60)%60); //tod.getMinutes() - (43195/60)=719min, 719%60=59min
  int todSec = (mils/1000)%60; //tod.getSeconds() - 43195%60 = 55sec
  int ci = (NUM_MAX*8)-1; //total column index - we will start at the last one and move backward
  //display index = (NUM_MAX-1)-(ci/8)
  //display column index = ci%8
  for(int i=0; i<5; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, (todHrs<10?0:bignum[(todHrs/10)*5+i])); ci--; } //h tens (leading blank)
  for(int i=0; i<1; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, 0); ci--; } //1col gap
  for(int i=0; i<5; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, bignum[(todHrs%10)*5+i]); ci--; } //h ones
  for(int i=0; i<2; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, 0); ci--; } //2col gap
  for(int i=0; i<5; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, bignum[(todMin/10)*5+i]); ci--; } //m tens + 1col gap
  for(int i=0; i<1; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, 0); ci--; } //1col gap
  for(int i=0; i<5; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, bignum[(todMin%10)*5+i]); ci--; } //m ones + 1col gap
  for(int i=0; i<1; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, 0); ci--; } //1col gap
  for(int i=0; i<3; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, (showSeconds?smallnum[(todSec/10)*3+i]:0)); ci--; } //s tens (unless wifi is connecting, then blank) + 1col gap
  for(int i=0; i<1; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, 0); ci--; } //1col gap
  for(int i=0; i<3; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, (showSeconds?smallnum[(todSec%10)*3+i]:0)+(i==0&&false?1:0)+(i==2&&false?1:0)); ci--; } //s ones (unless wifi is connecting, then blank), plus wifi and ntp fail indicators (disabled for now - they were WiFi.status()!=WL_CONNECTED and !ntpOK)
  //TODO reintroduce indicators by hiding seconds at certain times
} //end fn displayTime

void displayClear(){
  for(int i=0; i<NUM_MAX; i++) { lc.clearDisplay(i); }
}
void displayByte(byte b){
  int ci = (NUM_MAX*8)-1; //total column index - we will start at the last one and move backward
  //display index = (NUM_MAX-1)-(ci/8)
  //display column index = ci%8
  displayClear();
  for(int i=0; i<5; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, (b<100?0:bignum[(b/100)     *5+i])); ci--; }
  for(int i=0; i<1; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, 0); ci--; } //1col gap
  for(int i=0; i<5; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, (b<10? 0:bignum[((b%100)/10)*5+i])); ci--; }
  for(int i=0; i<1; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, 0); ci--; } //1col gap
  for(int i=0; i<5; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8,          bignum[(b%10)      *5+i]);  ci--; }
} //end fn displayByte

int displayToggleBrightness(){
  switch(curBrightness){
    case 0: curBrightness = 1; break;
    case 1: curBrightness = 7; break;
    case 7: curBrightness = 15; break;
    case 15: curBrightness = 0; break;
    default: break;
  }
  Serial.print(F("Changing brightness to ")); Serial.print(curBrightness,DEC); Serial.println(F("/15"));
  for(int i=0; i<NUM_MAX; i++) { lc.setIntensity(i,curBrightness); }
  return curBrightness;
}