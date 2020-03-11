////////// ONBOARD RTC ZERO //////////
#include <RTCZero.h>
RTCZero rtc;

void rtcSetup(){
  rtc.begin();
}
void rtcLoop(){
  //checkRTC(false);
  //TODO
}
void rtcDisplayTime(bool showSeconds){
  //displayTime(todMils,showSeconds);
  //TODO
}