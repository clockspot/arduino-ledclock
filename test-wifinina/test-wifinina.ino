//A test sketch, to be controlled via serial interface, to play with the various wifinina functions live
#include <SPI.h>
#include <WiFiNINA.h>
#include <WiFiUdp.h>

String wssid = "";
String wpass = ""; //wpa pass or wep key
byte wki = 0; //wep key index - 0 if using wpa

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
  
  Serial.println(F("Ready"));
}

void loop() {
  checkSerialInput();
  checkForNTPResponse();
}

//Serial input for control
String currentLine = "";
void checkSerialInput(){
  if(Serial.available()>0){
    currentLine = Serial.readString();
    currentLine = currentLine.substring(0,currentLine.length()-1);
    
    Serial.print(F("> ")); Serial.println(currentLine);
    
    if(currentLine.startsWith(F("wssid="))){ //wifi change
      //e.g. wssid=Network Name&wpass=qwertyuiop&wki=1
      //TODO since the values are not html-entitied (due to the difficulty of de-entiting here), this will fail if the ssid contains "&wssid=" or pass contains "&wpass="
      int startPos = 6;
      int endPos = currentLine.indexOf(F("&wpass="),startPos);
      wssid = currentLine.substring(startPos,endPos);
      startPos = endPos+7;
      endPos = currentLine.indexOf(F("&wki="),startPos);
      wpass = currentLine.substring(startPos,endPos);
      startPos = endPos+5;
      wki = currentLine.substring(startPos).toInt();
      Serial.print(F("Changed wifi creds: "));
      currentLine = "creds"; //to make it print out
    }
    
         if(currentLine==F("init")){ server.begin(); Udp.begin(localPort); }
    else if(currentLine==F("server")) server.begin();
    else if(currentLine==F("serverstatus")) Serial.println(server.status());
    else if(currentLine==F("client")) checkForClients();
    else if(currentLine==F("creds")) {
      Serial.print(F("wssid="));
      Serial.print(wssid);
      Serial.print(F(" wpass="));
      Serial.print(wpass);
      Serial.print(F(" wki="));
      Serial.print(wki);
      Serial.println();
    }
    else if(currentLine==F("wifi")) startWiFi();
    else if(currentLine==F("ap")) startAP();
    else if(currentLine==F("ntp")) startNTP();
    else if(currentLine==F("dis")) WiFi.disconnect();
    else if(currentLine==F("end")) WiFi.end();
    else { Serial.print("Unknown command \""); Serial.print(currentLine); Serial.println("\""); }
    checkForWiFiStatusChange();
  }
}



void startWiFi(){
  WiFi.end(); //if AP is going, stop it
  checkForWiFiStatusChange(); //just for serial logging
  Serial.print(millis()); Serial.print(F(" Attempting to connect to SSID: ")); Serial.println(wssid);
  if(wki) WiFi.begin(wssid.c_str(), wki, wpass.c_str()); //WEP - hangs while connecting
  else WiFi.begin(wssid.c_str(), wpass.c_str()); //WPA - hangs while connecting
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
  WiFi.end(); //if wifi is going, stop it
  checkForWiFiStatusChange(); //just for serial logging
  Serial.print(millis()); Serial.println(" Creating access point");
  if(WiFi.beginAP("Clock")==WL_AP_LISTENING){ //Change "beginAP" if you want to create an WEP network
    Serial.print("SSID: "); Serial.println(WiFi.SSID());
    //by default the local IP address of will be 192.168.4.1 - override with WiFi.config(IPAddress(10, 0, 0, 1));
    WiFi.config(IPAddress(7,7,7,7));
    Serial.print("Access the admin page by browsing to http://"); Serial.println(WiFi.localIP());
    //server.begin() was formerly here
  }
  else Serial.println(" Wasn't able to create access point.");
  checkForWiFiStatusChange(); //just for serial logging
} //end fn startAP

bool ntpStartLast = 0;
void startNTP(){ //Called at intervals to check for ntp time
  if(WiFi.status()!=WL_CONNECTED) { Serial.println(F("wifi not connected")); return; }
  if(ntpStartLast!=0 && millis()-ntpStartLast < 3000) { Serial.println(F("I'm busy! Try again in a few seconds")); return; } //do not send more than one request within 3 seconds
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
    client.println("HTTP/1.1 200 OK");
    client.println("Content-type:text/html");
    client.println("Access-Control-Allow-Origin:*");
    client.println();
    client.println("Hello world");
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
    Serial.print(F(" (")); Serial.print(WiFi.status()); Serial.print(F(") "));
    Serial.println(WiFi.localIP());
  }
}