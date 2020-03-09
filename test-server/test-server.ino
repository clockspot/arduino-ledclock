#include <SPI.h>
#include <WiFiNINA.h>
#include <WiFiUdp.h>

#include "secrets.h" //supply your own
char ssid[] = SECRET_SSID; // your network SSID (name)
char pass[] = SECRET_PASS; // your network password (use for WPA, or use as key for WEP)

unsigned int localPort = 2395; // local port to listen for UDP packets
IPAddress timeServer(129, 6, 15, 28); // time.nist.gov NTP server
const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets
WiFiUDP Udp; // A UDP instance to let us send and receive packets over UDP

WiFiServer server(80);


void setup() {
  Serial.begin(9600);
  while(!Serial); //only works on 33 IOT

  //Check status of wifi module up front
  if(WiFi.status()==WL_NO_MODULE){ Serial.println("Communication with WiFi module failed!"); while(true); }
  else if(WiFi.firmwareVersion()<WIFI_FIRMWARE_LATEST_VERSION) Serial.println("Please upgrade the firmware");
  
  //Create the server and UDP objects before WiFi is even going
  server.begin();
  Udp.begin(localPort);
  Serial.println(F("Please enter 'a' for AP, 'w' for WiFi, 'd' to disconnect."));
}

unsigned long millisLast = 0;
void loop() {
  //Every 5 seconds, if WiFi (not AP) is connected, make a UDP call to NTP
  if(millis()-millisLast > 5000) { millisLast+=5000; if(WiFi.status()==WL_CONNECTED) startNTP(); }
  //Every loop, check for serial input, UDP response, client connection, or change in WiFi status
  checkSerialInput();
  checkForNTPResponse();
  checkForClients();
  checkForWiFiStatusChange(); //just for serial logging
}


void startWiFi(){
  WiFi.disconnect(); //if AP is going, stop it
  checkForWiFiStatusChange(); //just for serial logging
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

void startAP(){
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

bool ntpStartLast = 0;
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
  Serial.print("Udp.beginPacket: "); Serial.println(Udp.beginPacket(timeServer, 123)); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Serial.print("Udp.endPacket: "); Serial.println(Udp.endPacket());
} //end fn startNTP

void checkForNTPResponse(){ //Called on every cycle to see if there is an ntp response to handle
  if(!ntpStartLast) return;
  if(!Udp.parsePacket()) return;
  // We've received a packet, read the data from it
  Udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer
  unsigned long ntpTime = (packetBuffer[40] << 24) | (packetBuffer[41] << 16) | (packetBuffer[42] << 8) | packetBuffer[43];
  Serial.print(millis()); Serial.print(" Received UDP packet from NTP server. Time is "); Serial.println(ntpTime);
  Udp.flush(); //in case of extraneous(?) data
  //Udp.stop() was formerly here
  ntpStartLast = 0;
} //end fn checkNTP

void checkForClients(){
  if(WiFi.status()!=WL_CONNECTED && WiFi.status()!=WL_AP_CONNECTED) return;
  WiFiClient client = server.available();
  if(client){
    Serial.print(millis()); Serial.println(" Got client.");
    client.flush(); //in this test, ignore what the client has to say
    client.stop();
  }
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

//Serial input for control
int incomingByte   = 0;
void checkSerialInput(){
  if(Serial.available()>0){
    incomingByte   = Serial.read();
    switch(incomingByte){
      case 97: //a
        startAP(); break;
      case 119: //w
        startWiFi(); break;
      case 100: //d
        WiFi.disconnect(); break;  
      default: break;
    }
  }
}