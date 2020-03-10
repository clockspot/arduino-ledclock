#ifndef CONFIG_HEADER
#define CONFIG_HEADER


////////// DISPLAY //////////
#define DISPLAY_H "display-MAX7219.h"
#include DISPLAY_H

////////// INPUTS //////////
#define INPUTS_H "inputs-LSM6DS3.h"
//#define INPUTS_H "inputs-none.h"
#include INPUTS_H

////////// RTC //////////
#include "rtc-fake.h"
//#include "rtc-RTCZero.h"
//#include "rtc-DS3231.h"

////////// NETWORK //////////
#include "network-wifinina.h"
//#include "network-none.h"


#endif