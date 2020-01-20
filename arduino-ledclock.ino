// Digital clock code for Arduino Nano 33 IoT and a chain of four MAX7219 LED display units
// Sketch by Luke McKenzie (luke@theclockspot.com) - https://github.com/clockspot/arduino-ledclock
// Based largely on public domain and Arduino code, and Eberhard Farle's LedControl library - http://wayoda.github.io/LedControl

////////// WIFI and NETWORKING //////////
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
  Serial.begin(9600); while (!Serial) {;}
  startWiFi();
  for(int i=0; i<NUM_MAX; i++) { lc.shutdown(i,false); lc.setIntensity(i,8); }
  startNTP();
}

void loop() {
  checkRTC(false);
  checkNTP();
}



////////// WIFI //////////
bool wifiConnected = false;
void startWiFi(){
  // check for the WiFi module:
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    // don't continue
    while (true);
  }

  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
    Serial.println("Please upgrade the firmware");
  }

  // // attempt to connect to WiFi network:
  // while (status != WL_CONNECTED) {
    Serial.print(F("Attempting to connect to SSID: "));
    Serial.print(ssid);
    Serial.print(F(" at "));
    Serial.println(millis(),DEC);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    //status = 
    WiFi.begin(ssid, pass);
    Serial.print(F("Completed WiFi.begin at todMils "));
    Serial.println(millis(),DEC);

  //   // wait for connection
  //   delay(2000);
  // }
}

void printWiFiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your board's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}


////////// NTP //////////
bool checkingNTP = false;
unsigned long checkNTPStart = 0;
bool checkNTPSuccess = false;

void startNTP(){ //Called at intervals to check for ntp time
  if(!checkingNTP) { //if not already checking NTP
    if(WiFi.status()==WL_CONNECTED){ //are we connected to wifi?
      if(!wifiConnected){ //did we just connect?
        wifiConnected = 1;
        Serial.println("Connected to wifi");
        printWiFiStatus();
      }
      checkingNTP = true; //ok to go!
      Serial.println("Starting connection to NTP server...");
      //TODO create a new connection to server and close it every time? is that necessary for UDP? what about TCP?
      Udp.begin(localPort);
      checkNTPStart = millis();
      sendNTPpacket(timeServer); // send an NTP packet to a time server
    } else { //not connected to wifi
      Serial.println("Not connected to WiFi. Trying to connect, and we'll try NTP again in a minute.");
      wifiConnected = 0;
      checkNTPSuccess = 0;
      startWiFi();
    } //end not connected to wifi
  } //end if not already checking NTP
} //end fn startNTP

unsigned long sendNTPpacket(IPAddress& address) {
  // send an NTP request to the time server at the given address
  //Serial.println("1");
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  //Serial.println("2");
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  //Serial.println("3");

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  //Serial.println("4");
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  //Serial.println("5");
  Udp.endPacket();
  //Serial.println("6");
}

void checkNTP(){ //Called on every cycle to see if there is an ntp response to handle
  if(checkingNTP){
    if(Udp.parsePacket()) { //testing failure every other minute: && ((todMils/1000)/60)%2==0
      // We've received a packet, read the data from it
      Udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer
      Udp.stop();

      //the timestamp starts at byte 40 of the received packet and is four bytes,
      // or two words, long. First, esxtract the two words:

      unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
      unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
      // combine the four bytes (two words) into a long integer
      // this is NTP time (seconds since Jan 1 1900):
      unsigned long secsSince1900 = highWord << 16 | lowWord;
      //TODO I suppose secsSince1900 has been adjusted for leap seconds?
      secsSince1900 += OFFSET;

      unsigned int requestTime = millis()-checkNTPStart; //this handles rollovers OK b/c 2-(max-2) = 4
    
      unsigned long newtodMils = (secsSince1900%86400)*1000+(requestTime/2); //TODO plus mils? //add half the request time
      int drift = todMils-newtodMils;
      todMils = newtodMils;
    
      Serial.print(F("NTP request took ")); Serial.print(requestTime,DEC); Serial.println(F("ms."));
      Serial.print(F("RTC is off by ")); Serial.print(drift,DEC); Serial.println(F(" (sec res and request time notwithstanding)."));
      Serial.print(F("RTC set to ")); printRTCTime(); Serial.println();
      checkingNTP = false;
      checkNTPSuccess = true;
      checkRTC(true);
    }
    else if(millis()-checkNTPStart>5000){
      Udp.stop();
      Serial.println(F("Couldn't connect to NTP server"));
      checkingNTP = false;
      checkNTPSuccess = false;
    }
  }
} //end fn checkNTP


////////// RTC and NTP //////////
byte rtcSecLast = 61;
unsigned long millisLast = 0;
bool justAppliedAntiDrift = 0;
void checkRTC(bool justSet){
  if(!justSet){ //if we haven't just set the fake "rtc", let's increment it
    unsigned long millisSnap = millis();
    todMils += millisSnap-millisLast;
    millisLast=millisSnap;
  }
  int rtcMin = ((todMils/1000)/60)%60;
  int rtcSec = (todMils/1000)%60;
  if(rtcSecLast != rtcSec || justSet){ //A new second!
    if(!justSet && !justAppliedAntiDrift){ //Apply anti-drift each time we naturally reach a new second
      //If it's negative, we'll set todMils back and set a flag, so we don't do it again at the upcoming "real" new second.
      //We shouldn't need to check if we'll be setting it back past zero, as we won't have done the midnight rollover yet (below)
      //and at the first second past midnight, todMils will almost certainly be larger than ANTI_DRIFT.
      todMils += ANTI_DRIFT;
      if(ANTI_DRIFT<0) justAppliedAntiDrift = 1;
      //TODO if we set back a second, need to apply the above and not run the below
    } else justAppliedAntiDrift = 0;
    //Now we can assume todMils is accurate
    if(todMils>86400000) todMils-=86400000; //is this a new day? set it back 24 hours
    if(!justSet && rtcSec==0 && (wifiConnected && checkNTPSuccess? rtcMin==0: true)){ //Trigger NTP update at top of the hour, or at the top of any minute if wifi is not connected or NTP is not successful TODO change to 5 minutes
      Serial.println(F("Time for an NTP check"));
      startNTP();
    }
    displayTime(); //Display time on LEDs
    rtcSecLast = rtcSec; //register that we have done stuff
  } //end if new second
} //end fn checkRTC

void printRTCTime(){
  unsigned long todSecs = todMils/1000;
  int rtcHrs = (todMils/1000)/3600; //rtc.getHours() - 43195/3600 = 11hrs
  int rtcMin = ((todMils/1000)/60)%60; //rtc.getMinutes() - (43195/60)=719min, 719%60=59min
  int rtcSec = (todMils/1000)%60; //rtc.getSeconds() - 43195%60 = 55sec
  if(rtcHrs<10) Serial.print(F("0")); Serial.print(rtcHrs); Serial.print(F(":"));
  if(rtcMin<10) Serial.print(F("0")); Serial.print(rtcMin); Serial.print(F(":"));
  if(rtcSec<10) Serial.print(F("0")); Serial.print(rtcSec);
}


////////// LED DISPLAY //////////

//See commit fb1419c for binary counter display test

// byte smallnum[30]={
//   B11111100, B10000100, B11111100, // 0
//   B00001000, B11111100, B00000000, // 1
//   B11100100, B10100100, B10111100, // 2
//   B10000100, B10010100, B11111100, // 3
//   B00111000, B00100000, B11111100, // 4
//   B10011100, B10010100, B11110100, // 5
//   B11111100, B10010100, B11110100, // 6
//   B00000100, B00000100, B11111100, // 7
//   B11111100, B10010100, B11111100, // 8
//   B00111100, B00100100, B11111100  // 9
// };
byte smallnum[30]={
  B11111000, B10001000, B11111000, // 0
  B00010000, B11111000, B00000000, // 1
  B11101000, B10101000, B10111000, // 2
  B10001000, B10101000, B11111000, // 3
  B01110000, B01000000, B11111000, // 4
  B10111000, B10101000, B11101000, // 5
  B11111000, B10101000, B11101000, // 6
  B00001000, B00001000, B11111000, // 7
  B11111000, B10101000, B11111000, // 8
  B00111000, B00101000, B11111000  // 9
};
byte bignum[50]={
  B01111110, B11111111, B10000001, B11111111, B01111110, // 0
  B00000000, B00000100, B11111110, B11111111, B00000000, // 1
  B11000010, B11100001, B10110001, B10011111, B10001110, // 2
  B01001001, B10001101, B10001111, B11111011, B01110001, // 3
  B00111000, B00100100, B11111110, B11111111, B00100000, // 4
  B01001111, B10001111, B10001001, B11111001, B01110001, // 5
  B01111100, B11111110, B10001011, B11111001, B01110000, // 6
  B00000001, B11110001, B11111001, B00001111, B00000111, // 7
  B01110110, B11111111, B10001001, B11111111, B01110110, // 8
  B00001110, B10011111, B11010001, B01111111, B00111110  // 9
};

void displayTime(){
  unsigned long todSecs = todMils/1000; if(todSecs>=86400) todSecs=0;
  int rtcHrs = (todMils/1000)/3600; //rtc.getHours() - 43195/3600 = 11hr
  int rtcMin = ((todMils/1000)/60)%60; //rtc.getMinutes() - (43195/60)=719min, 719%60=59min
  int rtcSec = (todMils/1000)%60; //rtc.getSeconds() - 43195%60 = 55sec
  int ci = (NUM_MAX*8)-1; //total column index - we will start at the last one and move backward
  //display index = (NUM_MAX-1)-(ci/8)
  //display column index = ci%8
  //big numbers are 5 pixels wide; small ones are 3 pixels wide
  for(int i=0; i<5; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, (rtcHrs<10?0:bignum[(rtcHrs/10)*5+i])); ci--; } ci--; //h tens + 1col gap (leading blank instead of zero)
  for(int i=0; i<5; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, bignum[(rtcHrs%10)*5+i]); ci--; } ci--; ci--; //h ones + 2col gap
  for(int i=0; i<5; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, bignum[(rtcMin/10)*5+i]); ci--; } ci--; //m tens + 1col gap
  for(int i=0; i<5; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, bignum[(rtcMin%10)*5+i]); ci--; } ci--; //m ones + 1col gap
  for(int i=0; i<3; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, smallnum[(rtcSec/10)*3+i]); ci--; } ci--; //s tens + 1col gap
  for(int i=0; i<3; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, smallnum[(rtcSec%10)*3+i]+(i==0&&!wifiConnected?1:0)+(i==2&&!checkNTPSuccess?1:0)); ci--; } //s ones, plus wifi and ntp fail indicators
  
}

//See commit fb1419c for serial input control if needed for runtime config