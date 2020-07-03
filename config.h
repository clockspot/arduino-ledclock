#ifndef CONFIG_H
#define CONFIG_H


//Which display?
#define DISPLAY_H "display-MAX7219.h"


//Which inputs?
//#define INPUTS_H "inputs-none.h"
#define INPUTS_H "inputs-LSM6DS3.h"

//What pins are associated with each input? (if using the LSM6DS3, just use any unique ints such as 1,2,3,4)
//As the IMU "buttons" don't have pins, these are made-up values.
const byte mainSel = 1;
const byte mainAdjUp = 2;
const byte mainAdjDn = 3;
const byte altSel = 4;

const word btnShortHold = 1000; //for setting the displayed feataure
const word btnLongHold = 3000; //for for entering options menu
const word btnVeryLongHold = 5000; //for wifi IP info / AP start / admin start
const word btnSuperLongHold = 15000; //for wifi forget

const int imuTestCountTrigger = 60; //IMU debouncing: how many test count needed to change the reported state


//Which RTC?
#define RTC_H "rtc-fake.h"
//#define RTC_H "rtc-RTCZero.h"
//#define RTC_H "rtc-DS3231.h"


//Which network?
//#define NETWORK_H "network-none.h"
#define NETWORK_H "network-wifinina.h"


//Other stuff
#define OFFSET -21600+3600 //we are this many seconds behind UTC


#endif