//rtc-fake.h

#ifndef RTC_FAKE_H
#define RTC_FAKE_H

void rtcSetup();
void rtcLoop();
void checkRTC(bool justSet);
void rtcDisplayTime(bool showSeconds);
void rtcChangeMinuteSync();
void rtcSetTime(unsigned long newtodMils, unsigned int uncertainty);
String strMilsToHms(unsigned long t);
String strSyncState();
String strDiffLength(unsigned long diff);

#endif