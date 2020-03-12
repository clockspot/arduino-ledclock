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

char rtcChangeMinuteSync(){ //m
  if(!TESTSyncEveryMinute) {
    Serial.println(F("Now syncing every minute"));
    TESTSyncEveryMinute = 1;
  } else {
    Serial.println(F("Now syncing every hour (or every minute when wifi/sync is bad)"));
    TESTSyncEveryMinute = 0;
  }
  return TESTSyncEveryMinute;
}

//mosstrMilsToHmsync
unsigned long lastSyncMillis  = 0; //millis - used to "detect" sync gaps passing midnight (as long as it's <50 days!)
unsigned long lastSyncTodMils = 0; //true time of day, in mils
unsigned int  lastSyncUncMils = 0; //± uncertainty, in mils (1/2 the request time)
long          lastSyncOffMils = 0; //how far the RTC had drifted
//second most recent sync
unsigned long prevSyncMillis  = 0;
unsigned long prevSyncTodMils = 0;
unsigned int  prevSyncUncMils = 0;
long          prevSyncOffMils = 0;

void rtcSetTime(unsigned long newtodMils, unsigned int uncertainty){
  // //TODO if setting a real RTC, don't set it until the top of the next second
  // //TODO I suppose ntpTime has been adjusted for leap seconds?
  
  prevSyncMillis  = lastSyncMillis;  lastSyncMillis  = millis();
  prevSyncTodMils = lastSyncTodMils; lastSyncTodMils = newtodMils;
  prevSyncUncMils = lastSyncUncMils; lastSyncUncMils = uncertainty;
  prevSyncOffMils = lastSyncOffMils; lastSyncOffMils = (prevSyncTodMils? todMils-newtodMils: 0); //only if not set before
  
  todMils = newtodMils;
  
  Serial.print(F("RTC set to "));
  Serial.println(getLastSync());
}

String strMilsToHms(unsigned long t){
  t = t%86400000;
  String out = "";
  int todHrs = (t/1000)/3600;
  int todMin = ((t/1000)/60)%60;
  int todSec = (t/1000)%60;
  int todMil = t%1000;
  if(todHrs<10) out=out+F("0"); out=out+todHrs; out=out+F(":");
  if(todMin<10) out=out+F("0"); out=out+todMin; out=out+F(":");
  if(todSec<10) out=out+F("0"); out=out+todSec; out=out+F(".");
  if(todMil<100) out=out+F("0"); if(todMil<10) out=out+F("0"); out=out+todMil;
  return out;
}

String getLastSync(){
  if(!lastSyncMillis) return "None";
  String out = "";
  out=out+strMilsToHms(lastSyncTodMils);
  if(prevSyncMillis){
    out=out+F(" ("); out=out+strDiffLength(millis()-lastSyncMillis); out=out+F(" ago)");
    out=out+F("; drifted "); out=out+lastSyncOffMils; out=out+F(" ±"); out=out+lastSyncUncMils;
    out=out+F("ms since sync at ");
    out=out+strMilsToHms(prevSyncTodMils);
    out=out+F(" ("); out=out+strDiffLength(lastSyncMillis-prevSyncMillis); out=out+F(" prior)");
  }
  else out=out+F(" (new)");
  return out;
}

String strDiffLength(unsigned long diff){
  String out = "";
       if(diff<60000){    out=out+diff/1000;     out=out+F("s"); }
  else if(diff<3600000){  out=out+diff/60000;    out=out+F("m"); }
  else if(diff<86400000){ out=out+diff/3600000;  out=out+F("h"); }
  else                  { out=out+diff/86400000; out=out+F("d"); }
  return out;
}