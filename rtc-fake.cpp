////////// FAKE RTC using millis //////////
unsigned long todMils = 0; //time of day in milliseconds
#define ANTI_DRIFT 0 //msec to add per second - or seconds to add per day divided by 86.4
//Doesn't have to be perfect – just enough for decent timekeeping display until the next ntp sync

#include "Arduino.h"
#include <SPI.h>
#include <WiFiNINA.h>
#include <WiFiUdp.h>
#include "config.h" //constants
#include "rtc-fake.h" //definitions for your own functions - needed only if calling functions before they're defined
#include NETWORK_H //definitions for the network
#include DISPLAY_H //definitions for the display (as defined in config)

void rtcSetup(){
  
}
void rtcLoop(){
  checkRTC(false);
}

////////// FAKE RTC using millis //////////
byte rtcSecLast = 61;
unsigned long millisLast = 0;
bool justAppliedAntiDrift = 0;
bool TESTSyncEveryMinute = 0;
void checkRTC(bool justSet){
  if(!justSet){ //if we haven't just set the fake "rtc", let's increment it
    unsigned long millisSnap = millis();
    todMils += millisSnap-millisLast;
    millisLast=millisSnap;
  }
  int rtcMin = ((todMils/1000)/60)%60;
  int rtcSec = (todMils/1000)%60;
  if(rtcSecLast != rtcSec || justSet){ //A new second!
    rtcSecLast = rtcSec;
    if(!justSet && !justAppliedAntiDrift){ //Apply anti-drift each time we naturally reach a new second
      //If it's negative, we'll set todMils back and set a flag, so we don't do it again at the upcoming "real" new second.
      //We shouldn't need to check if we'll be setting it back past zero, as we won't have done the midnight rollover yet (below)
      //and at the first second past midnight, todMils will almost certainly be larger than ANTI_DRIFT.
      todMils += ANTI_DRIFT;
      if(ANTI_DRIFT<0) justAppliedAntiDrift = 1;
      //TODO if we set back a second, need to apply the above and not run the below
    } else justAppliedAntiDrift = 0;
    //Now we can assume todMils is accurate
    if(todMils>86400000) { todMils-=86400000; } //is this a new day? set it back 24 hours
    //if(WiFi.status()!=WL_CONNECTED) ntpOK = 0; //if wifi is out, flag ntp as out too //TODO maybe have an indicator of how long it's been out?
    if(!justSet && rtcSec==0 && (networkNTPOK() && !TESTSyncEveryMinute? rtcMin==59: true)){ //Trigger NTP update at minute :59, or any minute if ntp is stale
      Serial.println(F("\nTime for an NTP check"));
      startNTP();
    }
    rtcDisplayTime(true);
  } //end if new second
} //end fn checkRTC

void rtcDisplayTime(bool showSeconds){
  displayTime(todMils,showSeconds);
}

void rtcChangeMinuteSync(){ //m
  if(!TESTSyncEveryMinute) {
    Serial.println(F("Now syncing every minute"));
    TESTSyncEveryMinute = 1;
  } else {
    Serial.println(F("Now syncing every hour (or every minute when wifi/sync is bad)"));
    TESTSyncEveryMinute = 0;
  }
}

void rtcSetTime(unsigned int newtodMils, unsigned int requestTime){
  // //TODO if setting a real RTC, don't set it until the top of the next second
  // //TODO I suppose ntpTime has been adjusted for leap seconds?
  
  int drift = todMils-newtodMils;
  todMils = newtodMils;
  //
  // rtc.setEpoch(ntpTime-2208988800UL); //set the rtc just so we can get the date. subtract 70 years to get to unix epoch
  // rtcDate = rtc.getDay();
  //
  Serial.print(F("NTP request took ")); Serial.print(requestTime,DEC); Serial.print(F("ms. "));
  Serial.print(F("RTC is off by ")); Serial.print(drift,DEC); Serial.print(F("ms ±")); Serial.print(requestTime/2,DEC); Serial.print(F("ms. "));
  Serial.print(F("RTC set to ")); printTODFromMils(todMils); Serial.println(); Serial.println();
  // ntpChecking = 0;
  // ntpOK = 1;
  // checkRTC(true);
  
}

void printTODFromMils(unsigned long t){
  int todHrs = (t/1000)/3600;
  int todMin = ((t/1000)/60)%60;
  int todSec = (t/1000)%60;
  int todMil = t%1000;
  if(todHrs<10) Serial.print(F("0")); Serial.print(todHrs); Serial.print(F(":"));
  if(todMin<10) Serial.print(F("0")); Serial.print(todMin); Serial.print(F(":"));
  if(todSec<10) Serial.print(F("0")); Serial.print(todSec); Serial.print(F("."));
  if(todMil<100) Serial.print(F("0")); if(todMil<10) Serial.print(F("0")); Serial.print(todMil);
}