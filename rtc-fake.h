////////// FAKE RTC using millis //////////
unsigned long todMils = 0; //time of day in milliseconds
#define OFFSET -21600 //we are this many seconds behind UTC
#define ANTI_DRIFT 0 //msec to add per second - or seconds to add per day divided by 86.4
//Doesn't have to be perfect – just enough for decent timekeeping display until the next ntp sync

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
    if(todMils>86400000) { todMils-=86400000; rtcDate = 0; } //is this a new day? set it back 24 hours and clear the date until rtc can be reset from ntp
    if(WiFi.status()!=WL_CONNECTED) ntpOK = 0; //if wifi is out, flag ntp as out too //TODO maybe have an indicator of how long it's been out?
    if(!justSet && rtcSec==0 && (ntpOK && !TESTSyncEveryMinute? rtcMin==0: true)){ //Trigger NTP update at top of the hour, or at the top of any minute if ntp is out TODO change to 5 minutes?
      Serial.println(F("\nTime for an NTP check"));
      startNTP();
    }
    rtcDisplayTime(true);
  } //end if new second
} //end fn checkRTC

rtcDisplayTime(showSeconds){
  displayTime(todMils,showSeconds);
}