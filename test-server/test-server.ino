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
  startWiFi();
}

unsigned long millisLast = 0;
void loop() {
  //Every 5 seconds, make a UDP call to NTP
  if(millis()-millisLast > 5000) { millisLast+=5000; startNTP(); }
  //Every loop, check for a UDP response and a client connection
  checkForNTPResponse();
  checkForServerClients();
}


void startWiFi(){
  if(WiFi.status()==WL_NO_MODULE) Serial.println("Communication with WiFi module failed!");
  else if(WiFi.firmwareVersion()<WIFI_FIRMWARE_LATEST_VERSION) Serial.println("Please upgrade the firmware");
  else { //hardware is ok
    Serial.print(F("Attempting to connect to SSID: ")); Serial.println(ssid);
    WiFi.begin(ssid, pass); //hangs while connecting
    if(WiFi.status()==WL_CONNECTED){ //did it work?
      Serial.println("Connected!");
      Serial.print("SSID: "); Serial.println(WiFi.SSID());
      Serial.print("IP address: "); Serial.println(WiFi.localIP());
      Serial.print("Signal strength (RSSI):"); Serial.print(WiFi.RSSI()); Serial.println(" dBm");
      server.begin();
    } else { //it didn't work
      Serial.println("Wasn't able to connect.");
    } //end it didn't work
  } //end hardware is ok
} //end fn startWiFi

bool ntpStartLast = 0;
void startNTP(){ //Called at intervals to check for ntp time
  if(WiFi.status()!=WL_CONNECTED) return;
  if(ntpStartLast!=0 && millis()-ntpStartLast < 3000) return; //do not send more than one request within 3 seconds
  Serial.println();
  ntpStartLast = millis();
  Udp.flush(); //does this do any good?
  Udp.stop();
  Serial.print(millis()); Serial.println(" Sending UDP packet to NTP server.");
  Serial.print("Udp.begin: "); Serial.println(Udp.begin(localPort)); //open connection
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
  Udp.stop();
  ntpStartLast = 0;
} //end fn checkNTP


void checkForServerClients(){
  if(WiFi.status()!=WL_CONNECTED) return;
  WiFiClient client = server.available();
  if(client){
    Serial.println(); Serial.print(millis()); Serial.println(" Got client.");
    client.flush(); //does this do any good?
    client.stop();
  }
}