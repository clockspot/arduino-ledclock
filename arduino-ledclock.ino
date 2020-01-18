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

////////// RTC //////////
#include <RTCZero.h>
RTCZero rtc;
#define OFFSET -21600 //we are this many seconds behind UTC

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
  rtc.begin();
  for(int i=0; i<NUM_MAX; i++) { lc.shutdown(i,false); lc.setIntensity(i,8); }
  //TODO create a new connection to server and close it every time? is that necessary for UDP? what about TCP?
  Serial.println("\nStarting connection to server...");
  Udp.begin(localPort);
  //startNTP(); //checkRTC will handle this - except when repowering
}

void loop() {
  checkRTC();
  checkNTP();
}



////////// WIFI //////////
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

  // attempt to connect to WiFi network:
  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(ssid, pass);

    // wait for connection
    delay(2000);
  }

  Serial.println("Connected to wifi");
  printWiFiStatus();
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

void startNTP(){ //Called at intervals to check for ntp time
  if(!checkingNTP) {
    Serial.println(F("NTP call started"));
    checkingNTP = true;
    sendNTPpacket(timeServer); // send an NTP packet to a time server
  }
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
  if(checkingNTP && Udp.parsePacket()) {
    Serial.println(F("NTP packet received"));
    checkingNTP = false;
    // We've received a packet, read the data from it
    Udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

    //the timestamp starts at byte 40 of the received packet and is four bytes,
    // or two words, long. First, esxtract the two words:

    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    // combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900):
    unsigned long secsSince1900 = highWord << 16 | lowWord;
    //Serial.print("Seconds since Jan 1 1900 = ");
    //Serial.println(secsSince1900);

    // now convert NTP time into everyday time:
    //Serial.print("Unix time = ");
    // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
    const unsigned long seventyYears = 2208988800UL;
    // subtract seventy years:
    unsigned long epoch = secsSince1900 - seventyYears;

    rtc.setEpoch(epoch+OFFSET);
    Serial.print(F("RTC set to ")); printRTCTime(); Serial.println();
  }
} //end fn checkNTP


////////// RTC and NTP //////////
byte rtcSecLast = 61;
void checkRTC(){
  //we can't do a snapshot but should be ok because only triggered after a seconds value change
  if(rtcSecLast != rtc.getSeconds()){ //A new second! Do stuff!
    rtcSecLast = rtc.getSeconds();
    //Trigger time update at top of each minute
    if(rtc.getSeconds()==0 && rtc.getMinutes()%5==0){
      Serial.print(F("According to RTC, it's ")); printRTCTime(); Serial.println(F(" - time for an NTP check"));
      startNTP();
    }
    //Display time on LEDs
    displayHMS(rtc.getHours(),rtc.getMinutes(),rtc.getSeconds());
  } //end if new second
} //end fn checkRTC

void printRTCTime(){
  // Serial.print(rtc.getYear());
  // Serial.print("/");
  // Serial.print(rtc.getMonth());
  // Serial.print("/");
  // Serial.print(rtc.getDay());
  // Serial.print(" ");
  if(rtc.getHours()<10) Serial.print(F("0"));
  Serial.print(rtc.getHours()); //gmt/offset malarkey
  Serial.print(F(":"));
  if(rtc.getMinutes()<10) Serial.print(F("0"));
  Serial.print(rtc.getMinutes());
  Serial.print(F(":"));
  if(rtc.getSeconds()<10) Serial.print(F("0"));
  Serial.print(rtc.getSeconds());
}


////////// LED DISPLAY //////////

// //Binary counter display test
// byte lastAddr = NUM_MAX-1;
// byte lastCol = 7;
// byte lastCount = 0; //max 255
// void testDisplay(){
//   while(true){
//     lastAddr++; if(lastAddr>=NUM_MAX){
//       lastAddr=0; lastCount++; if(lastCount>=255){
//         lastCount=0;
//       }
//     }
//     lastCol++; if(lastCol>=8) lastCol=0;
//     Serial.print(F("Setting ")); Serial.print(lastAddr,DEC);
//     Serial.print(F("/")); Serial.print(lastCol,DEC);
//     Serial.print(F(" to ")); Serial.println(lastCount,DEC);
//     lc.setColumn((NUM_MAX-1)-lastAddr,lastCol,lastCount);
//     delay(250);
//   }
// }

byte smallnum[30]={
  B11111100, B10000100, B11111100, // 0
  B00001000, B11111100, B00000000, // 1
  B11100100, B10100100, B10111100, // 2
  B10000100, B10010100, B11111100, // 3
  B00111000, B00100000, B11111100, // 4
  B10011100, B10010100, B11110100, // 5
  B11111100, B10010100, B11110100, // 6
  B00000100, B00000100, B11111100, // 7
  B11111100, B10010100, B11111100, // 8
  B00111100, B00100100, B11111100  // 9
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

void displayHMS(int h, int m, int s){
  if(h>24 || h<0) h=0;
  if(m>60 || m<0) m=0;
  if(s>60 || s<0) s=0;
  int ci = (NUM_MAX*8)-1; //column index
  //display = (NUM_MAX-1)-(ci/8)
  //dispcol = ci%8
  //big numbers are 5 pixels wide; small ones are 3 pixels wide
  for(int i=0; i<5; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, bignum[(h/10)*5+i]); ci--; } ci--; //h tens + 1 col gap
  for(int i=0; i<5; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, bignum[(h%10)*5+i]); ci--; } ci--; ci--; //h ones + 2 col gap
  for(int i=0; i<5; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, bignum[(m/10)*5+i]); ci--; } ci--; //m tens + 1 col gap
  for(int i=0; i<5; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, bignum[(m%10)*5+i]); ci--; } ci--; //m ones + 1 col gap
  for(int i=0; i<3; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, smallnum[(s/10)*3+i]); ci--; } ci--; //s tens + 1 col gap
  for(int i=0; i<3; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, smallnum[(s%10)*3+i]); ci--; }       //s ones
}


// //Serial input control if we need it for live config
// void setup(){
//   Serial.begin(9600); while(!Serial){ ; }
//   Serial.println("Hello world!");
// }
// int incomingByte = 0;
// void loop(){
//   if(Serial.available()>0){
//     incomingByte = Serial.read();
//     switch(incomingByte){
//       case 96: clear
//         Serial.println
//         break;
//       default: break;
//     }
//     Serial.print("I received: ");
//     Serial.println(incomingByte, DEC);
//   }
// }
//