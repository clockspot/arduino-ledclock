//rtc-fake.h

#ifndef RTC_FAKE_H
#define RTC_FAKE_H

void rtcSetup();
void rtcLoop();
void checkRTC(bool justSet);
void rtcDisplayTime(bool showSeconds);
void rtcChangeMinuteSync();
void rtcSetTime(unsigned int newtodMils, unsigned int requestTime);
void printTODFromMils(unsigned long t);


#endif