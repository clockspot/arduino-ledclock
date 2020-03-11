//rtc-DS3231.h

#ifndef RTC_DS3231_H
#define RTC_DS3231_H

void rtcSetup();
void rtcLoop();
void checkRTC(bool justSet);
void rtcDisplayTime(showSeconds);

#endif