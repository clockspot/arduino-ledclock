// Digital clock code for Arduino Nano 33 IoT and a chain of four MAX7219 LED display units
// Sketch by Luke McKenzie (luke@theclockspot.com) - https://github.com/clockspot/arduino-ledclock
// Based largely on public domain and Arduino code, and Eberhard Farle's LedControl library - http://wayoda.github.io/LedControl

////////// WIFI and NTP //////////
#include <SPI.h>
#include <WiFiNINA.h>
#include <WiFiUdp.h>
#include <Arduino_LSM6DS3.h>

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

WiFiServer server(80);

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


///// inputs /////
// Hardware inputs and value setting
byte btnCur = 0; //Momentary button currently in use - only one allowed at a time
byte btnCurHeld = 0; //Button hold thresholds: 0=none, 1=unused, 2=short, 3=long, 4=verylong, 5=superlong, 10=set by btnStop()
unsigned long inputLast = 0; //When a button was last pressed
unsigned long inputLast2 = 0; //Second-to-last of above
//TODO the math between these two may fail very rarely due to millis() rolling over while setting. Need to find a fix. I think it only applies to the rotary encoder though.
int inputLastTODMins = 0; //time of day, in minutes past midnight, when button was pressed. Used in paginated functions so they all reflect the same TOD.
//As the IMU "buttons" don't have pins, these are made-up values.
const byte mainSel = 1;
const byte mainAdjUp = 2;
const byte mainAdjDn = 3;
const byte altSel = 4;

const word btnShortHold = 1000; //for setting the displayed feataure
const word btnLongHold = 3000; //for for entering options menu
const word btnVeryLongHold = 5000; //for wifi IP info / AP start / admin start
const word btnSuperLongHold = 15000; //for wifi forget

unsigned long adminInputLast = 0; //for noticing when the admin page hasn't been interacted with in 2 minutes, so we can time it out

int curBrightness = 7;


void setup() {
  Serial.begin(9600);
  while(!Serial); //only works on 33 IOT
  rtc.begin();
  for(int i=0; i<NUM_MAX; i++) { lc.shutdown(i,false); lc.setIntensity(i,curBrightness); }
  initInputs();
  startNTP();
}

void loop() {
  checkSerialInput();
  checkNTP();
  checkRTC(false);
  if(status==WL_CONNECTED) checkServerClients();
  checkInputs();
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
    status = WiFi.begin(ssid, pass); //hangs while connecting
    if(status==WL_CONNECTED){ //did it work?
      Serial.println("Connected!");
      Serial.print("SSID: "); Serial.println(WiFi.SSID());
      Serial.print("IP address: "); Serial.println(WiFi.localIP());
      Serial.print("Signal strength (RSSI):"); Serial.print(WiFi.RSSI()); Serial.println(" dBm");
      server.begin(); //there's no way to stop it I think, so might as well keep it going TODO what about across wifi breakages
      // IPAddress theip = WiFi.localIP();
      // displayByte(theip[0]); delay(2000);
      // displayByte(theip[1]); delay(2000);
      // displayByte(theip[2]); delay(2000);
      // displayByte(theip[3]); delay(2000);
      // displayClear();
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


////////// WEB SERVER //////////
void checkServerClients(){
  if(status!=WL_CONNECTED) return;
  WiFiClient client = server.available();

  if (client) {                             // if you get a client,
    
    if(adminInputLast==0 || millis()-adminInputLast>120000) { client.stop(); Serial.print(F("Got a client but ditched it because adminInputLast=")); Serial.print(adminInputLast); Serial.print(F(" and millis()-aIL=")); Serial.println(millis()-adminInputLast); adminInputLast=0; return; } //TODO use a different flag from adminInputLast
    
    adminInputLast = millis();
    
    //Serial.println("new client");           // print a message out the serial port
    Serial.print(F("Got a client and keeping it because adminInputLast=")); Serial.print(adminInputLast); Serial.print(F(" and millis()-aIL=")); Serial.println(millis()-adminInputLast);
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected()) {            // loop while the client's connected
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        if (c == '\n') {                    // if the byte is a newline character

          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println();

            // the content of the HTTP response follows the header:
            client.print(F("<!DOCTYPE html><html><head><title>Clock Admin</title><style>body { background-color: #222; color: white; font-family: -apple-system, sans-serif; font-size: 18px; margin: 1.5em; } a { color: white; }</style><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'><script src='https://ajax.googleapis.com/ajax/libs/jquery/3.4.1/jquery.min.js'></script><script type='text/javascript'>$(function(){ $('a').click(function(e){ e.preventDefault(); $.ajax({ url: $(this).attr('data-action') }).fail(function(){ $('body').html('<p>Admin page has timed out. Please hold Select for 5 seconds to reactivate it.</p>'); }); }); });</script></head><body><h2>Clock Admin</h2><p><a href='#' data-action='/b'>Cycle brightness</a></p><p><a href='#' data-action='/s'>Test: toggle sec instead of min display</a></p><p><a href='#' data-action='/m'>Test: toggle sync frequency</a></p><p><a href='#' data-action='/n'>Test: toggle blocking NTP packets</a></p><p><a href='#' data-action='/d'>Print RTC date</a></p></body></html>"));
            //<p><a href='#' data-action=\"/w\">Test: disconnect WiFi</a></p>

            // The HTTP response ends with another blank line:
            client.println();
            // break out of the while loop:
            break;
          } else {    // if you got a newline, then clear currentLine:
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }

        // Check to see if the client request was
             if (currentLine.endsWith("GET /w")) doDisconnectWiFi();
        else if (currentLine.endsWith("GET /m")) doChangeMinuteSync();
        else if (currentLine.endsWith("GET /n")) doToggleNTPTest();
        else if (currentLine.endsWith("GET /s")) doToggleSecMin();
        else if (currentLine.endsWith("GET /d")) doPrintRTCDate();
        else if (currentLine.endsWith("GET /b")) doToggleBrightness();
      }
    }
    // close the connection:
    client.stop();
    Serial.println("client disconnected");
  }
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

////////// INPUTS ///////////
void initInputs(){
  if(!IMU.begin()){ Serial.println("Failed to initialize IMU!"); while(1); }
  // Serial.print("Accelerometer sample rate = ");
  // Serial.print(IMU.accelerationSampleRate());
  // Serial.println(" Hz");
  // Serial.println();
  // Serial.println("Acceleration in G's");
  // Serial.println("X\tY\tZ");

  // vertical: x=1, y=0, z=0
  // forward: x=0, y=0, z=1
  /// f a bit  0.8  0   0.7
  // bakward: x=0. y=0, z=-1
  // b a bit   0.8  0 -0.5
  //left:     x=0, y=1, z=0
  //right:          -1
  //upside     -1    0    0
}

//IMU "debouncing"
const int imuTestCountTrigger = 60; //how many test count needed to change the reported state
int imuYState = 0; //the state we're reporting (-1, 0, 1)
int imuYTestState = 0; //the state we've actually last seen
int imuYTestCount = 0; //how many times we've seen it
int imuZState = 0; //the state we're reporting (-1, 0, 1)
int imuZTestState = 0; //the state we've actually last seen
int imuZTestCount = 0; //how many times we've seen it

void checkInputs(){
  float x, y, z;
  IMU.readAcceleration(x,y,z);
  int imuState;
  //Assumes Arduino is oriented with components facing back of clock, and USB port facing up
       if(y<=-0.5) imuState = -1;
  else if(y>= 0.5) imuState = 1;
  else if(y>-0.3 && y<0.3) imuState = 0;
  else imuState = imuYTestState; //if it's not in one of the ranges, treat it as "same"
  if(imuYTestState!=imuState){ imuYTestState=imuState; imuYTestCount=0; }
  if(imuYTestCount<imuTestCountTrigger){ imuYTestCount++; /*Serial.print("Y "); Serial.print(imuYTestState); Serial.print(" "); for(char i=0; i<imuYTestCount; i++) Serial.print("#"); Serial.println(imuYTestCount);*/ if(imuYTestCount==imuTestCountTrigger) imuYState=imuYTestState; }
  
       if(z<=-0.5) imuState = -1;
  else if(z>= 0.5) imuState = 1;
  else if(z>-0.3 && z<0.3) imuState = 0;
  else imuState = imuZTestState;
  if(imuZTestState!=imuState){ imuZTestState=imuState; imuZTestCount=0; }
  if(imuZTestCount<imuTestCountTrigger){ imuZTestCount++; /*Serial.print("Z "); Serial.print(imuZTestState); Serial.print(" "); for(char i=0; i<imuZTestCount; i++) Serial.print("#"); Serial.println(imuZTestCount);*/ if(imuZTestCount==imuTestCountTrigger) imuZState=imuZTestState; }
  
  
  //TODO change this to millis / compare to debouncing code
  checkBtn(mainSel);
  checkBtn(mainAdjUp);
  checkBtn(mainAdjDn);
  checkBtn(altSel);
} //end checkInputs

bool readInput(byte btn){
  switch(btn){
    //Assumes Arduino is oriented with components facing back of clock, and USB port facing up
    //!() necessary since the pushbutton-derived code expects false (low) to mean pressed
    case mainSel:   return !(imuZState < 0); //clock tilted backward
    case mainAdjDn: return !(imuYState > 0); //clock tilted left
    case mainAdjUp: return !(imuYState < 0); //clock tilted right
    case altSel:    return !(imuZState > 0); //clock tilted forward
    default: break;
  }
} //end readInput

void checkBtn(byte btn){
  //Changes in momentary buttons, LOW = pressed.
  //When a button event has occurred, will call ctrlEvt
  bool bnow = readInput(btn);
  unsigned long now = millis();
  //If the button has just been pressed, and no other buttons are in use...
  if(btnCur==0 && bnow==LOW) {
    btnCur = btn; btnCurHeld = 0; inputLast2 = inputLast; inputLast = now; //inputLastTODMins = tod.hour()*60+tod.minute();
    ctrlEvt(btn,1); //hey, the button has been pressed
  }
  //If the button is being held...
  if(btnCur==btn && bnow==LOW) {
    if((unsigned long)(now-inputLast)>=btnSuperLongHold && btnCurHeld < 5) { //account for rollover
      btnCurHeld = 5;
      ctrlEvt(btn,5); //hey, the button has been long-held
    }
    else if((unsigned long)(now-inputLast)>=btnVeryLongHold && btnCurHeld < 4) { //account for rollover
      btnCurHeld = 4;
      ctrlEvt(btn,4); //hey, the button has been long-held
    }
    else if((unsigned long)(now-inputLast)>=btnLongHold && btnCurHeld < 3) { //account for rollover
      btnCurHeld = 3;
      ctrlEvt(btn,3); //hey, the button has been long-held
    }
    else if((unsigned long)(now-inputLast)>=btnShortHold && btnCurHeld < 2) {
      btnCurHeld = 2;
      ctrlEvt(btn,2); //hey, the button has been short-held
    }
  }
  //If the button has just been released...
  if(btnCur==btn && bnow==HIGH) {
    btnCur = 0;
    if(btnCurHeld < 10) ctrlEvt(btn,0); //hey, the button was released //4 to 10
    btnCurHeld = 0;
  }
}
void btnStop(){
  //In some cases, when handling btn evt 1/2/3, we may call this so following events 2/3/0 won't cause unintended behavior (e.g. after a fn change, or going in or out of set) //4 to 10
  btnCurHeld = 10;
}



void ctrlEvt(byte ctrl, byte evt){
  Serial.print(F("Btn "));
  switch(ctrl){
    case mainSel:   Serial.print(F("mainSel ")); break;
    case mainAdjDn: Serial.print(F("mainAdjDn ")); break;
    case mainAdjUp: Serial.print(F("mainAdjUp ")); break;
    case altSel:    Serial.print(F("altSel ")); break;
    default: break;
  }
  switch(evt){
    case 1: Serial.println(F("pressed")); break;
    case 2: Serial.println(F("short-held")); break;
    case 3: Serial.println(F("long-held")); break;
    case 0: Serial.println(F("released")); Serial.println(); break;
    default: break;
  }
  if(ctrl==mainSel && evt==4){ //very long hold: "start" server and show IP
    adminInputLast = millis();
    IPAddress theip = WiFi.localIP();
    displayByte(theip[0]); delay(2500);
    displayByte(theip[1]); delay(2500);
    displayByte(theip[2]); delay(2500);
    displayByte(theip[3]); delay(2500);
    displayClear();
  }
} //end ctrlEvt




////////// LED DISPLAY //////////

//See commit fb1419c for binary counter display test
//See commit 67bc64f for more font options

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
byte bignum[50]={ //chicagolike 5x8
  B01111110, B11111111, B10000001, B11111111, B01111110, // 0
  B00000000, B00000100, B11111110, B11111111, B00000000, // 1
  B11000010, B11100001, B10110001, B10011111, B10001110, // 2
  B01001001, B10001101, B10001111, B11111011, B01110001, // 3
  B00111000, B00100100, B11111110, B11111111, B00100000, // 4
  B01001111, B10001111, B10001001, B11111001, B01110001, // 5
  B01111110, B11111111, B10001001, B11111001, B01110000, // 6 more squared tail
//B01111100, B11111110, B10001011, B11111001, B01110000, // 6 original
  B00000001, B11110001, B11111001, B00001111, B00000111, // 7
  B01110110, B11111111, B10001001, B11111111, B01110110, // 8
  B00001110, B10011111, B10010001, B11111111, B01111110  // 9 more squared tail
//B00001110, B10011111, B11010001, B01111111, B00111110  // 9 original
};

bool TESTSecNotMin = 0; //display seconds instead of minutes
void displayTime(){
  unsigned long todSecs = todMils/1000; if(todSecs>=86400) todSecs=0;
  int rtcHrs = (todMils/1000)/3600; //rtc.getHours() - 43195/3600 = 11hr
  int rtcMin = (TESTSecNotMin? (todMils/1000)%60: ((todMils/1000)/60)%60); //rtc.getMinutes() - (43195/60)=719min, 719%60=59min
  int rtcSec = (todMils/1000)%60; //rtc.getSeconds() - 43195%60 = 55sec
  int ci = (NUM_MAX*8)-1; //total column index - we will start at the last one and move backward
  //display index = (NUM_MAX-1)-(ci/8)
  //display column index = ci%8
  for(int i=0; i<5; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, (rtcHrs<10?0:bignum[(rtcHrs/10)*5+i])); ci--; } //h tens (leading blank)
  for(int i=0; i<1; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, 0); ci--; } //1col gap
  for(int i=0; i<5; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, bignum[(rtcHrs%10)*5+i]); ci--; } //h ones
  for(int i=0; i<2; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, 0); ci--; } //2col gap
  for(int i=0; i<5; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, bignum[(rtcMin/10)*5+i]); ci--; } //m tens + 1col gap
  for(int i=0; i<1; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, 0); ci--; } //1col gap
  for(int i=0; i<5; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, bignum[(rtcMin%10)*5+i]); ci--; } //m ones + 1col gap
  for(int i=0; i<1; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, 0); ci--; } //1col gap
  for(int i=0; i<3; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, (!wifiConnecting?smallnum[(rtcSec/10)*3+i]:0)); ci--; } //s tens (unless wifi is connecting, then blank) + 1col gap
  for(int i=0; i<1; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, 0); ci--; } //1col gap
  for(int i=0; i<3; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, (!wifiConnecting?smallnum[(rtcSec%10)*3+i]:0)+(i==0&&WiFi.status()!=WL_CONNECTED?1:0)+(i==2&&!ntpOK?1:0)); ci--; } //s ones (unless wifi is connecting, then blank), plus wifi and ntp fail indicators
} //end fn displayTime

void displayByte(byte b){
  int ci = (NUM_MAX*8)-1; //total column index - we will start at the last one and move backward
  //display index = (NUM_MAX-1)-(ci/8)
  //display column index = ci%8
  displayClear();
  for(int i=0; i<5; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, (b<100?0:bignum[(b/100)     *5+i])); ci--; }
  for(int i=0; i<1; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, 0); ci--; } //1col gap
  for(int i=0; i<5; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, (b<10? 0:bignum[((b%100)/10)*5+i])); ci--; }
  for(int i=0; i<1; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8, 0); ci--; } //1col gap
  for(int i=0; i<5; i++){ lc.setColumn((NUM_MAX-1)-(ci/8),ci%8,          bignum[(b%10)      *5+i]);  ci--; }
} //end fn displayTime

void displayClear(){
  for(int i=0; i<NUM_MAX; i++) { lc.clearDisplay(i); }
}

void displayBrightness(int brightness){
  for(int i=0; i<NUM_MAX; i++) { lc.setIntensity(i,brightness); }
}


////////// SERIAL INPUT for prototype runtime changes //////////
void doDisconnectWiFi(){ //w
  Serial.println(F("Disconnecting WiFi - will try to connect at next NTP sync time"));
  WiFi.disconnect();
}
void doChangeMinuteSync(){ //m
  if(!TESTSyncEveryMinute) {
    Serial.println(F("Now syncing every minute"));
    TESTSyncEveryMinute = 1;
  } else {
    Serial.println(F("Now syncing every hour (or every minute when wifi/sync is bad)"));
    TESTSyncEveryMinute = 0;
  }
}
void doToggleNTPTest(){ //n
  if(!TESTNTPfail) {
    Serial.println(F("Now preventing incoming NTP packets"));
    TESTNTPfail = 1;
  } else {
    Serial.println(F("Now allowing incoming NTP packets"));
    TESTNTPfail = 0;
  }
}
void doToggleSecMin(){ //s
  if(!TESTSecNotMin) {
    Serial.println(F("Displaying seconds instead of minutes, for font testing"));
    TESTSecNotMin = 1;
  } else {
    Serial.println(F("Displaying minutes as usual"));
    TESTSecNotMin = 0;
  }
}
void doPrintRTCDate(){ //d
  Serial.print(F("rtcDate is ")); Serial.println(rtcDate,DEC);
}
void doToggleBrightness(){ //b
  switch(curBrightness){
    case 0: curBrightness = 1; break;
    case 1: curBrightness = 7; break;
    case 7: curBrightness = 15; break;
    case 15: curBrightness = 0; break;
    default: break;
  }
  Serial.print(F("Changing brightness to ")); Serial.print(curBrightness,DEC); Serial.println(F("/15"));
  displayBrightness(curBrightness);
}

int incomingByte = 0;
void checkSerialInput(){
  if(Serial.available()>0){
    incomingByte = Serial.read();
    switch(incomingByte){
      case 119: //w
        doDisconnectWiFi(); break;
      case 109: //m
        doChangeMinuteSync(); break;
      case 110: //n
        doToggleNTPTest(); break;
      case 115: //s
        doToggleSecMin(); break;
      case 100: //d
        doPrintRTCDate(); break;
      case 98: //b
        doToggleBrightness(); break;
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
      case 99: //c
      case 101: //e
      case 102: //f
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
