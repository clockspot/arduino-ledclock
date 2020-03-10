/*
To access the admin menu:

Hold Select for 5 seconds. If you have already connected the clock to your WiFi network, it will flash its IP address (as a series of three 4-digit numbers), which you can now use to access the admin page.

If not, it will display a 4-digit number, and begin broadcasting a WiFi hotspot with that number (such as “Clock 1234”). Connect to this hotspot and browse to 192.168.1.1 to access the admin page – where, if you like, you can connect the clock to your WiFi network [which will discontinue the hotspot].

For security, the clock will stop serving the admin page (and hotspot if applicable) after 2 minutes of inactivity. To access it again, hold Select for 5 seconds as above. (It will stay connected to WiFi to continue requesting NTP syncs, weather forecasts, etc. I might add a password feature later, but I thought this physical limitation would be security enough for now.)

To change or forget the WiFi network the clock is connected to, use the admin page, or simply hold Select for 15 seconds (through the end of the IP address) to forget the network (the display will blink).
*/

////////// WIFI and NTP //////////
#include <SPI.h>
#include <WiFiNINA.h>
#include <WiFiUdp.h>

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

const unsigned long ADMIN_TIMEOUT = 120000;



void networkSetup(){
  //Check status of wifi module up front
  if(WiFi.status()==WL_NO_MODULE){ Serial.println("Communication with WiFi module failed!"); while(true); }
  else if(WiFi.firmwareVersion()<WIFI_FIRMWARE_LATEST_VERSION) Serial.println("Please upgrade the firmware");
  
  //Create the server and UDP objects before WiFi is even going
  server.begin();
  Udp.begin(localPort);
  
  //Start wifi
  networkStartWifi();
}
void networkLoop(){
  checkNTP();
  checkClients();
}



void networkStartWiFi(){
  WiFi.disconnect(); //if AP is going, stop it
  checkForWiFiStatusChange(); //just for serial logging
  rtcDisplayTime(false); //display time without seconds
  Serial.println(); Serial.print(millis()); Serial.print(F(" Attempting to connect to SSID: ")); Serial.println(ssid);
  WiFi.begin(ssid, pass); //hangs while connecting
  if(WiFi.status()==WL_CONNECTED){ //did it work?
    Serial.print(millis()); Serial.println(" Connected!");
    Serial.print("SSID: "); Serial.println(WiFi.SSID());
    Serial.print("Signal strength (RSSI):"); Serial.print(WiFi.RSSI()); Serial.println(" dBm");
    Serial.print("Access the admin page by browsing to http://"); Serial.println(WiFi.localIP());
    //server.begin() was formerly here
  }
  else Serial.println(" Wasn't able to connect.");
  checkForWiFiStatusChange(); //just for serial logging
} //end fn startWiFi

void networkStartAP(){
  WiFi.disconnect(); //if wifi is going, stop it
  checkForWiFiStatusChange(); //just for serial logging
  Serial.println(); Serial.print(millis()); Serial.println(" Creating access point");
  if(WiFi.beginAP("Clock")==WL_AP_LISTENING){ //Change "beginAP" if you want to create an WEP network
    Serial.print("SSID: "); Serial.println(WiFi.SSID());
    //by default the local IP address of will be 192.168.4.1 - override with WiFi.config(IPAddress(10, 0, 0, 1));
    Serial.print("Access the admin page by browsing to http://"); Serial.println(WiFi.localIP());
    //server.begin() was formerly here
  }
  else Serial.println(" Wasn't able to create access point.");
  checkForWiFiStatusChange(); //just for serial logging
} //end fn startAP

void networkDisconnectWiFi(){
  //Serial.println(F("Disconnecting WiFi - will try to connect at next NTP sync time"));
  WiFi.disconnect();
}


bool ntpStartLast = 0;
//TODO reinstitute ntpOK
void startNTP(){ //Called at intervals to check for ntp time
  if(WiFi.status()!=WL_CONNECTED) return;
  if(ntpStartLast!=0 && millis()-ntpStartLast < 3000) return; //do not send more than one request within 3 seconds
  Serial.println();
  ntpStartLast = millis();
  Udp.flush(); //in case of old data
  //Udp.stop() was formerly here
  Serial.print(millis()); Serial.println(" Sending UDP packet to NTP server.");
  memset(packetBuffer, 0, NTP_PACKET_SIZE); // set all bytes in the buffer to 0
  // Initialize values needed to form NTP request
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
  Udp.endPacket()
} //end fn startNTP

void checkNTP(){ //Called on every cycle to see if there is an ntp response to handle
  if(!ntpStartLast) return;
  if(!Udp.parsePacket()) return;
  // We've received a packet, read the data from it
  Udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer
  
  //https://forum.arduino.cc/index.php?topic=526792.0
  unsigned long ntpTime = (packetBuffer[40] << 24) | (packetBuffer[41] << 16) | (packetBuffer[42] << 8) | packetBuffer[43];
  unsigned long ntpFrac = (packetBuffer[44] << 24) | (packetBuffer[45] << 16) | (packetBuffer[46] << 8) | packetBuffer[47];
  unsigned int  ntpMils = (int32_t)(((float)ntpFrac / UINT32_MAX) * 1000);
  // //TODO if setting a real RTC, don't set it until the top of the next second
  // //TODO I suppose ntpTime has been adjusted for leap seconds?
  // ntpTime += OFFSET; //timezone correction
  //
  // unsigned long newtodMils = (ntpTime%86400)*1000+ntpMils+(requestTime/2); //ntpTime stamp, plus milliseconds, plus half the request time
  // int drift = todMils-newtodMils;
  // todMils = newtodMils;
  //
  // rtc.setEpoch(ntpTime-2208988800UL); //set the rtc just so we can get the date. subtract 70 years to get to unix epoch
  // rtcDate = rtc.getDay();
  //
  // Serial.print(F("NTP request took ")); Serial.print(requestTime,DEC); Serial.print(F("ms. "));
  // Serial.print(F("RTC is off by ")); Serial.print(drift,DEC); Serial.print(F("ms ±")); Serial.print(requestTime/2,DEC); Serial.print(F("ms. "));
  // Serial.print(F("RTC set to ")); printRTCTime(); Serial.println();
  // ntpChecking = 0;
  // ntpOK = 1;
  // checkRTC(true);
  
  Serial.print(millis()); Serial.print(" Received UDP packet from NTP server. Time is "); Serial.print(ntpTime); Serial.print(" / "); Serial.println(todMilsToHMS(ntpTime*1000+ntpMils));
  
  Udp.flush(); //in case of extraneous(?) data
  //Udp.stop() was formerly here
  ntpStartLast = 0;
} //end fn checkNTP




unsigned long adminInputLast = 0; //for noticing when the admin page hasn't been interacted with in 2 minutes, so we can time it (and AP if applicable) out

void networkStartAdmin(){
  adminInputLast = millis();
  if(WiFi.status()!=WL_CONNECTED) networkStartAP();
  //TODO display should handle its own code for displaying a type of stuff
  IPAddress theip = WiFi.localIP();
  displayByte(theip[0]); delay(2500);
  displayByte(theip[1]); delay(2500);
  displayByte(theip[2]); delay(2500);
  displayByte(theip[3]); delay(2500);
  displayClear();
}
void networkStopAdmin(){
  adminInputLast = 0; //TODO use a different flag from adminInputLast
  if(WiFi.status()==WL_AP_CONNECTED) networkStartWifi();
}

void checkClients(){
  //if(WiFi.status()!=WL_CONNECTED && WiFi.status()!=WL_AP_CONNECTED) return;
  if(adminInputLast && millis()-adminInputLast>ADMIN_TIMEOUT) networkStopAdmin();
  WiFiClient client = server.available();
  if(client) {
    if(adminInputLast==0) { client.flush(); client.stop(); Serial.print(F("Got a client but ditched it because adminInputLast=")); Serial.print(adminInputLast); Serial.print(F(" and millis()-aIL=")); Serial.println(millis()-adminInputLast); return; }
    
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
             if (currentLine.endsWith("GET /m")) networkChangeMinuteSync();
        else if (currentLine.endsWith("GET /n")) networkToggleNTPTest();
        else if (currentLine.endsWith("GET /b")) displayToggleBrightness();
      }
    }
    // close the connection:
    client.stop();
    Serial.println("client disconnected");
  }
}




void networkChangeMinuteSync(){ //m
  if(!TESTSyncEveryMinute) {
    Serial.println(F("Now syncing every minute"));
    TESTSyncEveryMinute = 1;
  } else {
    Serial.println(F("Now syncing every hour (or every minute when wifi/sync is bad)"));
    TESTSyncEveryMinute = 0;
  }
}
void networkToggleNTPTest(){ //n
  if(!TESTNTPfail) {
    Serial.println(F("Now preventing incoming NTP packets"));
    TESTNTPfail = 1;
  } else {
    Serial.println(F("Now allowing incoming NTP packets"));
    TESTNTPfail = 0;
  }
}


void todMilsToHMS(t){
  int todHrs = (t/1000)/3600;
  int todMin = ((t/1000)/60)%60;
  int todSec = (t/1000)%60;
  int todMil = t%1000;
  if(todHrs<10) Serial.print(F("0")); Serial.print(todHrs); Serial.print(F(":"));
  if(todMin<10) Serial.print(F("0")); Serial.print(todMin); Serial.print(F(":"));
  if(todSec<10) Serial.print(F("0")); Serial.print(todSec); Serial.print(F("."));
  if(todMil<100) Serial.print(F("0")); if(todMil<10) Serial.print(F("0")); Serial.print(todMil);
}

int statusLast;
void checkForWiFiStatusChange(){
  if(WiFi.status()!=statusLast){
    Serial.print(millis()); Serial.print(F(" WiFi status has changed to "));
    statusLast = WiFi.status();
    switch(statusLast){
      case WL_IDLE_STATUS: Serial.print(F("WL_IDLE_STATUS")); break;
      case WL_NO_SSID_AVAIL: Serial.print(F("WL_NO_SSID_AVAIL")); break;
      case WL_SCAN_COMPLETED: Serial.print(F("WL_SCAN_COMPLETED")); break;
      case WL_CONNECTED: Serial.print(F("WL_CONNECTED")); break;
      case WL_CONNECT_FAILED: Serial.print(F("WL_CONNECT_FAILED")); break;
      case WL_CONNECTION_LOST: Serial.print(F("WL_CONNECTION_LOST")); break;
      case WL_DISCONNECTED: Serial.print(F("WL_DISCONNECTED")); break;
      case WL_AP_LISTENING: Serial.print(F("WL_AP_LISTENING")); break;
      case WL_AP_CONNECTED: Serial.print(F("WL_AP_CONNECTED")); break;
      default: break;
    }
    Serial.print(F(" (")); Serial.print(WiFi.status()); Serial.println(F(")"));
  }
}