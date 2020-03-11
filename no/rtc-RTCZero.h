//rtc-RTCZero.h

#ifndef RTC_RTCZERO_H
#define RTC_RTCZERO_H

void rtcSetup();
void rtcLoop();
void checkRTC(bool justSet);
void rtcDisplayTime(bool showSeconds);
void rtcChangeMinuteSync();

#endif