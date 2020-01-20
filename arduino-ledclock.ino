// Digital clock code for Arduino Nano 33 IoT and a chain of four MAX7219 LED display units
// Sketch by Luke McKenzie (luke@theclockspot.com) - https://github.com/clockspot/arduino-ledclock
// Based largely on public domain and Arduino code, and Eberhard Farle's LedControl library - http://wayoda.github.io/LedControl

////////// WIFI and NTP //////////
#include <SPI.h>
#include <WiFiNINA.h>
#include <WiFiUdp.h>

int status = WL_IDLE_STATUS;
#include "secrets.h" //supply your own
char ssid[] = SECRET_SSID; // your network SSID (name)
char pass[] = SECRET_PASS; // your network password (use for WPA, or use as key for WEP)
int keyIndex = 0; // your network key Index number (needed only for WEP)
unsigned int localPort = 2390; // local port to listen for UDP packets
IPAddress timeServer(129, 6, 15, 28); // time.nist.gov NTP server
const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets
WiFiUDP Udp; // A UDP instance to let us send and receive packets over UDP

////////// FAKE RTC using millis //////////
unsigned long todMils = 0; //time of day in milliseconds
#define OFFSET -21600 //we are this many seconds behind UTC
#define ANTI_DRIFT 0 //msec to add per second - or seconds to add per day divided by 86.4
//Doesn't have to be perfect – just enough for decent timekeeping display until the next ntp sync

////////// REAL RTC just for getting date //////////
int rtcDate = 0; //date calculated from real rtc at set time
#include <RTCZero.h>
RTCZero rtc;

////////// DISPLAY //////////
#define NUM_MAX 4
#define ROTATE 90
#define CLK_PIN 2 //D2, pin 20
#define CS_PIN 3 //D3, pin 21
#define DIN_PIN 4 //D4, pin 22
#include <LedControl.h>
//#include "fonts.h" //TODO
LedControl lc=LedControl(DIN_PIN,CLK_PIN,CS_PIN,NUM_MAX);


void setup() {
  Serial.begin(9600);
  rtc.begin();
  for(int i=0; i<NUM_MAX; i++) { lc.shutdown(i,false); lc.setIntensity(i,8); }
  startNTP();
}

void loop() {
  checkSerialInput();
  checkNTP();
  checkRTC(false);
}



////////// WIFI and NTP //////////
bool wifiConnecting = 0;
bool ntpChecking = 0;
unsigned long ntpCheckingSince = 0;
bool ntpOK = 0;

void startWiFi(){
  if(WiFi.status() == WL_NO_MODULE) Serial.println("Communication with WiFi module failed!");
  else if(WiFi.firmwareVersion()<WIFI_FIRMWARE_LATEST_VERSION) Serial.println("Please upgrade the firmware");
  else { //hardware is ok
    wifiConnecting = true;
    displayTime(); //changes the minute but hides the seconds
    Serial.print(F("Attempting to connect to SSID: ")); Serial.println(ssid);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    WiFi.begin(ssid, pass); //hangs while connecting
    if(WiFi.status()==WL_CONNECTED){ //did it work?
      Serial.println("Connected!");
      Serial.print("SSID: "); Serial.println(WiFi.SSID());
      Serial.print("IP address: "); Serial.println(WiFi.localIP());
      Serial.print("Signal strength (RSSI):"); Serial.print(WiFi.RSSI()); Serial.println(" dBm");
    } else { //it didn't work
      Serial.println("Wasn't able to connect. Will try again later.");
    } //end it didn't work
    wifiConnecting = false;
  } //end hardware is ok
} //end fn startWiFi

void startNTP(){ //Called at intervals to check for ntp time
  if(!ntpChecking) { //if not already checking NTP
    if(WiFi.status()!=WL_CONNECTED) startWiFi();
    if(WiFi.status()==WL_CONNECTED){ //continue only if connected
      ntpChecking = true; //ok to go!
      Serial.println("Starting connection to NTP server...");
      //TODO create a new connection to server and close it every time? is that necessary for UDP? what about TCP?
      Udp.begin(localPort); //TODO do we need to create a new connection to the server every time? try this with wifi disconnected
      ntpCheckingSince = millis();
      // send an NTP request to the time server at the given address
      // set all bytes in the buffer to 0
      memset(packetBuffer, 0, NTP_PACKET_SIZE);
      // Initialize values needed to form NTP request
      // (see URL above for details on the packets)
      packetBuffer[0] = 0b11100011;   // LI, Version, Mode
      packetBuffer[1] = 0;     // Stratum, or type of clock
      packetBuffer[2] = 6;     // Polling Interval
      packetBuffer[3] = 0xEC;  // Peer Clock Precision
      // 8 bytes of zero for Root Delay & Root Dispersion
      packetBuffer[12]  = 49;
      packetBuffer[13]  = 0x4E;
      packetBuffer[14]  = 49;
      packetBuffer[15]  = 52;
      Udp.beginPacket(timeServer, 123); //NTP requests are to port 123
      Udp.write(packetBuffer, NTP_PACKET_SIZE);
      Udp.endPacket();
    } else { //not connected to wifi
      ntpOK = 0;
    }
  } //end if not already checking NTP
} //end fn startNTP

bool TESTNTPfail = 0;
void checkNTP(){ //Called on every cycle to see if there is an ntp response to handle
  if(ntpChecking){
    if(Udp.parsePacket() && !TESTNTPfail) { //testing failure every other minute: && ((todMils/1000)/60)%2==0
      unsigned int requestTime = millis()-ntpCheckingSince; //this handles rollovers OK b/c 2-(max-2) = 4
      
      // We've received a packet, read the data from it
      Udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer
      Udp.stop();
      
      //https://forum.arduino.cc/index.php?topic=526792.0
      unsigned long ntpTime = (packetBuffer[40] << 24) | (packetBuffer[41] << 16) | (packetBuffer[42] << 8) | packetBuffer[43];
      unsigned long ntpFrac = (packetBuffer[44] << 24) | (packetBuffer[45] << 16) | (packetBuffer[46] << 8) | packetBuffer[47];
      unsigned int  ntpMils = (int32_t)(((float)ntpFrac / UINT32_MAX) * 1000);
      //TODO if setting a real RTC, don't set it until the top of the next second
      //TODO I suppose ntpTime has been adjusted for leap seconds?
      ntpTime += OFFSET; //timezone correction
      
      unsigned long newtodMils = (ntpTime%86400)*1000+ntpMils+(requestTime/2); //ntpTime stamp, plus milliseconds, plus half the request time
      int drift = todMils-newtodMils;
      todMils = newtodMils;
      
      rtc.setEpoch(ntpTime-2208988800UL); //set the rtc just so we can get the date. subtract 70 years to get to unix epoch
      rtcDate = rtc.getDay();
            
      Serial.print(F("NTP request took ")); Serial.print(requestTime,DEC); Serial.print(F("ms. "));
      Serial.print(F("RTC is off by ")); Serial.print(drift,DEC); Serial.print(F("ms ±")); Serial.print(requestTime/2,DEC); Serial.print(F("ms. "));
      Serial.print(F("RTC set to ")); printRTCTime(); Serial.println();
      ntpChecking = 0;
      ntpOK = 1;
      checkRTC(true);
    }
    else if(millis()-ntpCheckingSince>5000){
      Udp.stop();
      Serial.println(F("Couldn't connect to NTP server"));
      ntpChecking = 0;
      ntpOK = 0;
    }
  }
} //end fn checkNTP


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
    displayTime(); //Display time on LEDs
  } //end if new second
} //end fn checkRTC

void printRTCTime(){
  unsigned long todSecs = todMils/1000;
  int rtcHrs = (todMils/1000)/3600; //rtc.getHours() - 43195/3600 = 11hrs
  int rtcMin = ((todMils/1000)/60)%60; //rtc.getMinutes() - (43195/60)=719min, 719%60=59min
  int rtcSec = (todMils/1000)%60; //rtc.getSeconds() - 43195%60 = 55sec
  int rtcMil = todMils%1000;
  if(rtcHrs<10) Serial.print(F("0")); Serial.print(rtcHrs); Serial.print(F(":"));
  if(rtcMin<10) Serial.print(F("0")); Serial.print(rtcMin); Serial.print(F(":"));
  if(rtcSec<10) Serial.print(F("0")); Serial.print(rtcSec); Serial.print(F("."));
  if(rtcMil<100) Serial.print(F("0")); if(rtcMil<10) Serial.print(F("0")); Serial.print(rtcMil);
}


////////// LED DISPLAY //////////

//See commit fb1419c for binary counter display test

byte smallnum[30]={ //5px high, squared off, but for rounded
  B11111000, B10001000, B11111000, // 0
  B00010000, B11111000, B00000000, // 1 serif
  B11101000, B10101000, B10111000, // 2
  B10001000, B10101000, B11111000, // 3
  B01110000, B01000000, B11111000, // 4
  B10111000, B10101000, B11101000, // 5
  B11111000, B10101000, B11101000, // 6
  B00001000, B00001000, B11111000, // 7
  B11111000, B10101000, B11111000, // 8
  B10111000, B10101000, B11111000  // 9
};
byte smallnum2[30]={ //5px high, squared off, no serif on 1 and add serif to 6
  B11111000, B10001000, B11111000, // 0
  B00000000, B11111000, B00000000, // 1 no serif
  B11101000, B10101000, B10111000, // 2
  B10001000, B10101000, B11111000, // 3
  B01110000, B01000000, B11111000, // 4
  B10111000, B10101000, B11101000, // 5
  B11111000, B10101000, B11100000, // 6 mini serif
  B00001000, B00001000, B11111000, // 7
  B11111000, B10101000, B11111000, // 8
  B00111000, B10101000, B11111000  // 9 mini serif
};
byte smallnum3[30]={ //6px high, squared off
  B11111100, B10000100, B11111100, // 0
  B00000000, B11111100, B00000000, // 1 no serif
  B11100100, B10100100, B10111100, // 2
  B10000100, B10010100, B11111100, // 3
  B00111000, B00100000, B11111100, // 4
  B10011100, B10010100, B11110100, // 5
  B11111100, B10010100, B11110000, // 6 bit of a serif
  B00000100, B00000100, B11111100, // 7
  B11111100, B10010100, B11111100, // 8
  B00111100, B10100100, B11111100  // 9 bit of a serif
};
// byte smallnum4[30]={ //5px high, rounded - terrible
//   B01110000, B10001000, B01110000, // 0
//   B00010000, B11111000, B00000000, // 1
//   B11001000, B10101000, B10010000, // 2
//   B10001000, B10101000, B01011000, // 3
//   B01100000, B01010000, B11111000, // 4
//   B10111000, B10101000, B01001000, // 5
//   B01110000, B10101000, B01000000, // 6
//   B00001000, B11101000, B00011000, // 7
//   B01010000, B10101000, B01010000, // 8
//   B00010000, B10101000, B01110000  // 9
// };

byte bignum[50]={ //chicagolike 5x8
  B01111110, B11111111, B10000001, B11111111, B01111110, // 0
  B00000000, B00000100, B11111110, B11111111, B00000000, // 1
  B11000010, B11100001, B10110001, B10011111, B10001110, // 2
  B01001001, B10001101, B10001111, B11111011, B01110001, // 3
  B00111000, B00100100, B11111110, B11111111, B00100000, // 4
  B01001111, B10001111, B10001001, B11111001, B01110001, // 5
  B01111110, B11111111, B10001001, B11111001, B01110000, // 6 more squared tail
  B00000001, B11110001, B11111001, B00001111, B00000111, // 7
  B01110110, B11111111, B10001001, B11111111, B01110110, // 8
  B00001110, B10011111, B10010001, B11111111, B01111110  // 9 more squared tail
};
// byte bignum[50]={ //chicagolike 5x8
//   B01111110, B11111111, B10000001, B11111111, B01111110, // 0
//   B00000000, B00000100, B11111110, B11111111, B00000000, // 1
//   B11000010, B11100001, B10110001, B10011111, B10001110, // 2
//   B01001001, B10001101, B10001111, B11111011, B01110001, // 3
//   B00111000, B00100100, B11111110, B11111111, B00100000, // 4
//   B01001111, B10001111, B10001001, B11111001, B01110001, // 5
//   B01111100, B11111110, B10001011, B11111001, B01110000, // 6
//   B00000001, B11110001, B11111001, B00001111, B00000111, // 7
//   B01110110, B11111111, B10001001, B11111111, B01110110, // 8
//   B00001110, B10011111, B11010001, B01111111, B00111110  // 9
// };
byte bignum2[50]={ //square 5x8 bold on right
  B11111111, B10000001, B10000001, B11111111, B11111111, // 0
  B00000000, B00000000, B11111111, B11111111, B00000000, // 1
  B11110001, B10010001, B10010001, B10011111, B10011111, // 2
  B10000001, B10001001, B10001001, B11111111, B11111111, // 3
  B00011110, B00010000, B00010000, B11111111, B11111111, // 4
  B10001111, B10001001, B10001001, B11111001, B11111001, // 5
  B11111111, B10001001, B10001001, B11111001, B11111000, // 6
  B00000001, B00000001, B00000001, B11111111, B11111111, // 7
  B11111111, B10001001, B10001001, B11111111, B11111111, // 8
  B00011111, B10010001, B10010001, B11111111, B11111111  // 9
};
byte bignum3[40]={ //square 4x8 bold on right
  B11111111, B10000001, B11111111, B11111111, // 0
  B00000000, B11111111, B11111111, B00000000, // 1
  B11110001, B10010001, B10011111, B10011111, // 2
  B10000001, B10001001, B11111111, B11111111, // 3
  B00011110, B00010000, B11111111, B11111111, // 4
  B10001111, B10001001, B11111001, B11111001, // 5
  B11111111, B10001001, B11111001, B11111000, // 6
  B00000001, B00000001, B11111111, B11111111, // 7
  B11111111, B10001001, B11111111, B11111111, // 8
  B00011111, B10010001, B11111111, B11111111  // 9
};
byte bignum4[30]={ //square 3x8
  B11111111, B10000001, B11111111, // 0
  B00000000, B11111111, B00000000, // 1
  B11110001, B10010001, B10011111, // 2
  B10000001, B10001001, B11111111, // 3
  B00011110, B00010000, B11111111, // 4
  B10001111, B10001001, B11111001, // 5
  B11111111, B10001001, B11111000, // 6
  B00000001, B00000001, B11111111, // 7
  B11111111, B10001001, B11111111, // 8
  B00011111, B10010001, B11111111, // 9
};

char whichFont = 0;
bool TESTSecNotMin = 0; //display seconds instead of minutes
void displayTime(){
  unsigned long todSecs = todMils/1000; if(todSecs>=86400) todSecs=0;
  int rtcHrs = (todMils/1000)/3600; //rtc.getHours() - 43195/3600 = 11hr
  int rtcMin = (TESTSecNotMin? (todMils/1000)%60: ((todMils/1000)/60)%60); //rtc.getMinutes() - (43195/60)=719min, 719%60=59min
  int rtcSec = (todMils/1000)%60; //rtc.getSeconds() - 43195%60 = 55sec
  int ci = (NUM_MAX*8)-1; //total column index - we will start at the last one and move backward
  //display index = (NUM_MAX-1)-(ci/8)
  //display column index = ci%8
  switch(whichFont){
    case 0: default: //big numbers are 5 pixels wide
      for(int i=0; i<5; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, (rtcHrs<10?0:bignum[(rtcHrs/10)*5+i])); ci--; } ci--; //h tens + 1col gap (leading blank instead of zero)
      for(int i=0; i<5; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, bignum[(rtcHrs%10)*5+i]); ci--; } ci--; ci--; //h ones + 2col gap
      for(int i=0; i<5; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, bignum[(rtcMin/10)*5+i]); ci--; } ci--; //m tens + 1col gap
      for(int i=0; i<5; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, bignum[(rtcMin%10)*5+i]); ci--; } ci--; //m ones + 1col gap
      break;
    case 1: //big numbers are 5 pixels wide
      for(int i=0; i<5; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, (rtcHrs<10?0:bignum2[(rtcHrs/10)*5+i])); ci--; } ci--; //h tens + 1col gap (leading blank instead of zero)
      for(int i=0; i<5; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, bignum2[(rtcHrs%10)*5+i]); ci--; } ci--; ci--; //h ones + 2col gap
      for(int i=0; i<5; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, bignum2[(rtcMin/10)*5+i]); ci--; } ci--; //m tens + 1col gap
      for(int i=0; i<5; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, bignum2[(rtcMin%10)*5+i]); ci--; } ci--; //m ones + 1col gap
      break;
    case 2: //big numbers are 4 pixels wide
      for(int i=0; i<3; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, 0); ci--; }
      for(int i=0; i<4; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, (rtcHrs<10?0:bignum3[(rtcHrs/10)*4+i])); ci--; } ci--; //h tens + 1col gap (leading blank instead of zero)
      for(int i=0; i<4; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, bignum3[(rtcHrs%10)*4+i]); ci--; } ci--; ci--; //h ones + 2col gap
      for(int i=0; i<4; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, bignum3[(rtcMin/10)*4+i]); ci--; } ci--; //m tens + 1col gap
      for(int i=0; i<4; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, bignum3[(rtcMin%10)*4+i]); ci--; } ci--; ci--; //m ones + 1col gap
      break;
    case 3: //big numbers are 3 pixels wide
      //for(int i=0; i<8; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, 0); ci--; } //blank first display - could put date here
      for(int i=0; i<3; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, (rtcDate<10?0:smallnum3[(rtcDate/10)*3+i])); ci--; } ci--; //date tens + 1col gap (leading blank instead of zero)
      for(int i=0; i<3; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, (rtcDate==0?0:smallnum3[(rtcDate%10)*3+i])); ci--; } ci--; //date ones + 1col gap (blank if date is completely zeroed out)
      for(int i=0; i<3; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, (rtcHrs<10?0:bignum4[(rtcHrs/10)*3+i])); ci--; } ci--; //h tens + 1col gap (leading blank instead of zero)
      for(int i=0; i<3; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, bignum4[(rtcHrs%10)*3+i]); ci--; } ci--; ci--; //h ones + 2col gap
      for(int i=0; i<3; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, bignum4[(rtcMin/10)*3+i]); ci--; } ci--; //m tens + 1col gap
      for(int i=0; i<3; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, bignum4[(rtcMin%10)*3+i]); ci--; } ci--; //m ones + 1col gap
      break;
  } //end switch whichFont
  switch(whichFont){
    //small numbers are all 3 pixels wide
    case 0: default:
      for(int i=0; i<3; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, (!wifiConnecting?smallnum[(rtcSec/10)*3+i]:0)); ci--; } ci--; //s tens (unless wifi is connecting, then blank) + 1col gap
      for(int i=0; i<3; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, (!wifiConnecting?smallnum[(rtcSec%10)*3+i]:0)+(i==0&&WiFi.status()!=WL_CONNECTED?1:0)+(i==2&&!ntpOK?1:0)); ci--; } //s ones (unless wifi is connecting, then blank), plus wifi and ntp fail indicators
      break;
    case 1:
      for(int i=0; i<3; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, (!wifiConnecting?smallnum2[(rtcSec/10)*3+i]:0)); ci--; } ci--; //s tens (unless wifi is connecting, then blank) + 1col gap
      for(int i=0; i<3; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, (!wifiConnecting?smallnum2[(rtcSec%10)*3+i]:0)+(i==0&&WiFi.status()!=WL_CONNECTED?1:0)+(i==2&&!ntpOK?1:0)); ci--; } //s ones (unless wifi is connecting, then blank), plus wifi and ntp fail indicators
      break;
    case 2:
      for(int i=0; i<3; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, (!wifiConnecting?smallnum3[(rtcSec/10)*3+i]:0)); ci--; } ci--; //s tens (unless wifi is connecting, then blank) + 1col gap
      for(int i=0; i<3; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, (!wifiConnecting?smallnum3[(rtcSec%10)*3+i]:0)+(i==0&&WiFi.status()!=WL_CONNECTED?1:0)+(i==2&&!ntpOK?1:0)); ci--; } //s ones (unless wifi is connecting, then blank), plus wifi and ntp fail indicators
      break;
    case 3:
      for(int i=0; i<3; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, (!wifiConnecting?smallnum3[(rtcSec/10)*3+i]:0)); ci--; } ci--; //s tens (unless wifi is connecting, then blank) + 1col gap
      for(int i=0; i<3; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, (!wifiConnecting?smallnum3[(rtcSec%10)*3+i]:0)+(i==0&&WiFi.status()!=WL_CONNECTED?1:0)+(i==2&&!ntpOK?1:0)); ci--; } //s ones (unless wifi is connecting, then blank), plus wifi and ntp fail indicators
      break;
  } //end switch whichFont
} //end fn displayTime

void displayClear(){
  for(int i=0; i<NUM_MAX; i++) { lc.clearDisplay(i); }
}

void displayBrightness(int brightness){
  for(int i=0; i<NUM_MAX; i++) { lc.setIntensity(i,brightness); }
}


////////// SERIAL INPUT for prototype runtime changes //////////
int incomingByte = 0;
void checkSerialInput(){
  if(Serial.available()>0){
    incomingByte = Serial.read();
    switch(incomingByte){
      case 119: //w
        Serial.println(F("Disconnecting WiFi - will try to connect at next NTP sync time"));
        WiFi.disconnect();
        break;
      case 109: //m
        if(!TESTSyncEveryMinute) {
          Serial.println(F("Now syncing every minute"));
          TESTSyncEveryMinute = 1;
        } else {
          Serial.println(F("Now syncing every hour (or every minute when wifi/sync is bad)"));
          TESTSyncEveryMinute = 0;
        }
        break;
      case 110: //n
        if(!TESTNTPfail) {
          Serial.println(F("Now preventing incoming NTP packets"));
          TESTNTPfail = 1;
        } else {
          Serial.println(F("Now allowing incoming NTP packets"));
          TESTNTPfail = 0;
        }
        break;
      case 102: //f
        Serial.println(F("Changing font"));
        whichFont++; if(whichFont>3) whichFont=0;
        displayClear(); displayTime();
        break;
      case 115: //s
        if(!TESTSecNotMin) {
          Serial.println(F("Displaying seconds instead of minutes, for font testing"));
          TESTSecNotMin = 1;
        } else {
          Serial.println(F("Displaying minutes as usual"));
          TESTSecNotMin = 0;
        }
        break;
      case 100: //d
        Serial.print(F("rtcDate is ")); Serial.println(rtcDate,DEC);
        break;
      case 49: //1
      case 50: //2
      case 51: //3
      case 52: //4
      case 53: //5
      case 54: //6
      case 55: //7
      case 56: //8
      case 57: //9
      case 48: //0
      case 97: //a
      case 98: //b
      case 99: //c
      case 101: //e
      case 103: //g
      case 104: //h
      case 105: //i
      case 106: //j
      case 107: //k
      case 108: //l
      case 111: //o
      case 112: //p
      case 113: //q
      case 114: //r
      case 116: //t
      case 117: //u
      case 118: //v
      case 120: //x
      case 121: //y
      case 122: //z
      case 96: //`
      case 45: //-
      case 61: //=
      default: break;
    }
    // Serial.print("I received: ");
    // Serial.println(incomingByte, DEC);
  }
}
