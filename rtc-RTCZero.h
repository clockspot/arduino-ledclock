////////// ONBOARD RTC ZERO //////////
#include <RTCZero.h>
RTCZero rtc;

void rtcSetup(){
  rtc.begin();
}
void rtcLoop(){
  checkRTC(false);
}
rtcDisplayTime(showSeconds){
  //displayTime(todMils,showSeconds);
  //TODO
}