//network-wifinina.cpp

#include "Arduino.h"
#include <SPI.h>
#include <WiFiNINA.h>
#include <WiFiUdp.h>
#include "config.h" //constants
//#include "arduino-ledclock.h" //has the version
#include "network-wifinina.h" //definitions for your own functions - needed only if calling functions before they're defined
#include DISPLAY_H //definitions for the display (as defined in config)
#include RTC_H //definitions for the RTC for calling the display

String wssid = "Riley";
String wpass = "5802301644"; //wpa pass or wep key
byte wki = 0; //wep key index - 0 if using wpa
//Stopping place: this should be empty and fillable via settings page. Then make it save in feeprom. Also ditch the fake rtc.

unsigned int localPort = 2390; // local port to listen for UDP packets
IPAddress timeServer(129, 6, 15, 28); // time.nist.gov NTP server
const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets
WiFiUDP Udp; // A UDP instance to let us send and receive packets over UDP

WiFiServer server(80);

const unsigned long ADMIN_TIMEOUT = 120000; //two minutes
const unsigned long NTPOK_THRESHOLD = 3600000; //if no sync within 60 minutes, the time is considered stale


void networkSetup(){
  //Check status of wifi module up front
  if(WiFi.status()==WL_NO_MODULE){ Serial.println(F("Communication with WiFi module failed!")); while(true); }
  else if(WiFi.firmwareVersion()<WIFI_FIRMWARE_LATEST_VERSION) Serial.println(F("Please upgrade the firmware"));
  
  //Create the server and UDP objects before WiFi is even going
  server.begin();
  Udp.begin(localPort);
  
  //Start wifi
  networkStartWiFi();
  
  startNTP();
}
void networkLoop(){
  checkNTP();
  checkClients();
  checkForWiFiStatusChange();
}



void networkStartWiFi(){
  WiFi.disconnect(); //if AP is going, stop it
  if(wssid==F("")) return; //don't try to connect if there's no data
  checkForWiFiStatusChange(); //just for serial logging
  rtcDisplayTime(false); //display time without seconds
  Serial.println(); Serial.print(millis()); Serial.print(F(" Attempting to connect to SSID: ")); Serial.println(wssid);
  if(wki) WiFi.begin(wssid.c_str(), wki, wpass.c_str()); //WEP - hangs while connecting
  else WiFi.begin(wssid.c_str(), wpass.c_str()); //WPA - hangs while connecting
  if(WiFi.status()==WL_CONNECTED){ //did it work?
    Serial.print(millis()); Serial.println(F(" Connected!"));
    Serial.print(F("SSID: ")); Serial.println(WiFi.SSID());
    Serial.print(F("Signal strength (RSSI):")); Serial.print(WiFi.RSSI()); Serial.println(F(" dBm"));
    Serial.print(F("Access the admin page by browsing to http://")); Serial.println(WiFi.localIP());
    //server.begin() was formerly here
  }
  else Serial.println(" Wasn't able to connect.");
  checkForWiFiStatusChange(); //just for serial logging
} //end fn startWiFi

void networkStartAP(){
  WiFi.disconnect(); //if wifi is going, stop it
  checkForWiFiStatusChange(); //just for serial logging
  Serial.println(); Serial.print(millis()); Serial.println(F(" Creating access point"));
  if(WiFi.beginAP("Clock")==WL_AP_LISTENING){ //Change "beginAP" if you want to create an WEP network
    Serial.print(F("SSID: ")); Serial.println(WiFi.SSID());
    //by default the local IP address of will be 192.168.4.1 - override with WiFi.config(IPAddress(10, 0, 0, 1));
    WiFi.config(IPAddress(7,7,7,7));
    Serial.print(F("Access the admin page by browsing to http://")); Serial.println(WiFi.localIP());
    //server.begin() was formerly here
  }
  else Serial.println(" Wasn't able to create access point.");
  checkForWiFiStatusChange(); //just for serial logging
} //end fn startAP

void networkDisconnectWiFi(){
  //Serial.println(F("Disconnecting WiFi - will try to connect at next NTP sync time"));
  WiFi.disconnect();
}


unsigned int ntpStartLast = -3000;
bool ntpGoing = 0;
unsigned int ntpSyncLast = 0;
void startNTP(){ //Called at intervals to check for ntp time
  if(WiFi.status()!=WL_CONNECTED) return;
  if(ntpGoing && millis()-ntpStartLast < 3000) return; //if a previous request is going, do not start another until at least 3sec later
  ntpGoing = 1;
  ntpStartLast = millis();
  Udp.flush(); //in case of old data
  //Udp.stop() was formerly here
  Serial.println(); Serial.print(millis()); Serial.println(F(" Sending UDP packet to NTP server."));
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
  Udp.endPacket();
} //end fn startNTP

bool TESTNTPfail = 0;
void checkNTP(){ //Called on every cycle to see if there is an ntp response to handle
  if(!ntpGoing) return;
  if(!Udp.parsePacket()) return;
  // We've received a packet, read the data from it
  ntpSyncLast = millis();
  unsigned int requestTime = ntpSyncLast-ntpStartLast;
  Udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer
  
  if(TESTNTPfail) { Udp.flush(); return; Serial.println(F("NTP came back but discarding")); } //you didn't see anything...
  
  //https://forum.arduino.cc/index.php?topic=526792.0
  unsigned long ntpTime = (packetBuffer[40] << 24) | (packetBuffer[41] << 16) | (packetBuffer[42] << 8) | packetBuffer[43];
  unsigned long ntpFrac = (packetBuffer[44] << 24) | (packetBuffer[45] << 16) | (packetBuffer[46] << 8) | packetBuffer[47];
  unsigned int  ntpMils = (int32_t)(((float)ntpFrac / UINT32_MAX) * 1000);
  
  ntpTime += OFFSET; //timezone correction
  unsigned long newtodMils = (ntpTime%86400)*1000+ntpMils+(requestTime/2); //ntpTime stamp, plus milliseconds, plus half the request time
  
  Serial.print(millis()); Serial.print(F(" Received UDP packet from NTP server. Time is ")); Serial.println(ntpTime);

  rtcSetTime(newtodMils,requestTime/2);
  
  Udp.flush(); //in case of extraneous(?) data
  //Udp.stop() was formerly here
  ntpGoing = 0;
} //end fn checkNTP

bool networkNTPOK(){
  //Serial.print(F("NTP is ")); Serial.print(millis()-ntpSyncLast < NTPOK_THRESHOLD?F("OK: "):F("stale: ")); Serial.print(millis()-ntpSyncLast); Serial.print(F("ms old (vs limit of ")); Serial.print(NTPOK_THRESHOLD); Serial.print(F("ms)")); Serial.println();
  return (millis()-ntpSyncLast < NTPOK_THRESHOLD);
}




unsigned long adminInputLast = 0; //for noticing when the admin page hasn't been interacted with in 2 minutes, so we can time it (and AP if applicable) out

void networkStartAdmin(){
  adminInputLast = millis();
  //TODO display should handle its own code for displaying a type of stuff
  if(WiFi.status()!=WL_CONNECTED){
    networkStartAP();
    displayInt(7777); delay(2500);
  } else { //use existing wifi
    IPAddress theip = WiFi.localIP();
    displayInt(theip[0]); delay(2500);
    displayInt(theip[1]); delay(2500);
    displayInt(theip[2]); delay(2500);
    displayInt(theip[3]); delay(2500);
  }
  displayClear();
}
void networkStopAdmin(){
  adminInputLast = 0; //TODO use a different flag from adminInputLast
  if(WiFi.status()!=WL_CONNECTED) networkStartWiFi();
}

void checkClients(){
  //if(WiFi.status()!=WL_CONNECTED && WiFi.status()!=WL_AP_CONNECTED) return;
  if(adminInputLast && millis()-adminInputLast>ADMIN_TIMEOUT) networkStopAdmin();
  WiFiClient client = server.available();
  if(client) {
    if(adminInputLast==0) { client.flush(); client.stop(); Serial.print(F("Got a client but ditched it because last admin input was over ")); Serial.print(ADMIN_TIMEOUT); Serial.println(F("ms ago.")); return; }
    else { Serial.print(F("Last admin input was ")); Serial.print(millis()-adminInputLast); Serial.print(F("ms ago which is under the limit of ")); Serial.print(ADMIN_TIMEOUT); Serial.println(F("ms.")); }
    
    adminInputLast = millis();
    
    String currentLine = ""; //we'll read the data from the client one line at a time
    int requestType = 0;
    bool newlineSeen = false;

    if(client.connected()){
      while(client.available()){ //if there's bytes to read from the client
        char c = client.read();
        Serial.write(c); //DEBUG
        
        if(c=='\n') newlineSeen = true;
        else {
          if(newlineSeen){ currentLine = ""; newlineSeen = false; } //if we see a newline and then something else: clear current line
          currentLine += c;
        }

        //Find the request type and path from the first line.
        if(!requestType){
          if(currentLine=="GET / ") { requestType = 1; break; } //Read no more. We'll render out the page.
          if(currentLine=="POST / ") requestType = 2; //We'll keep reading til the last line.
          if(c=='\n') break; //End of first line without matching the above: invalid request, return nothing.
        }
        
      } //end whie client available
    } //end if client connected
    
    if(requestType){
      // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
      // and a content-type so the client knows what's coming, then a blank line:
      client.println("HTTP/1.1 200 OK");
      client.println("Content-type:text/html");
      client.println("Access-Control-Allow-Origin:*");
      client.println();
      if(requestType==1){ //get
        client.print(F("<!DOCTYPE html><html><head><title>Clock Settings</title><style>body { background-color: #eee; color: #222; font-family: system-ui, -apple-system, sans-serif; font-size: 18px; margin: 1.5em; position: absolute; } a { color: #33a; } ul { padding-left: 9em; text-indent: -9em; list-style: none; } ul li { margin-bottom: 0.8em; } ul li * { text-indent: 0; padding: 0; } ul li label:first-child { display: inline-block; width: 8em; text-align: right; padding-right: 1em; font-weight: bold; } ul li.nolabel { margin-left: 9em; } input[type='text'],input[type='submit'],select { border: 1px solid #999; margin: 0.2em 0; padding: 0.1em 0.3em; font-size: 1em; font-family: system-ui, -apple-system, sans-serif; } @media only screen and (max-width: 550px) { ul { padding-left: 0; text-indent: 0; } ul li label:first-child { display: block; width: auto; text-align: left; padding: 0; } ul li.nolabel { margin-left: 0; }} .saving { color: #66d; } .ok { color: #3a3; } .error { color: #c53; } @media (prefers-color-scheme: dark) { body { background-color: #222; color: #ddd; } a { color: white; } #result { background-color: #373; color: white; } input[type='text'],select { background-color: #444; color: #ddd; } }</style><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'></head><body><h2 style='margin-top: 0;'>Clock Settings</h2><div id='content'><ul>"));
        client.print(F("<li><label>Wi-Fi</label><form id='wform' style='display: inline;' onsubmit='save(this); return false;'><select id='wtype' onchange='wformchg()'><option value=''>None</option><option value='wpa'>WPA</option><option value='wep'>WEP</option></select><span id='wa'><br/><input type='text' id='wssid' name='wssid' placeholder='SSID (Network Name)' autocomplete='off' onchange='wformchg()' onkeyup='wformchg()' value='")); client.print(wssid); client.print(F("' /><br/><input type='text' id='wpass' name='wpass' placeholder='Password/Key' autocomplete='off' onchange='wformchg()' onkeyup='wformchg()' value='")); client.print(wpass); client.print(F("' /></span><span id='wb'><br/><label for='wki'>Key Index</label> <select id='wki' onchange='wformchg()'>")); for(char i=0; i<=4; i++){ client.print(F("<option value='")); client.print(i,DEC); client.print(F("' ")); client.print(wki==i?F("selected"):F("")); client.print(F(">")); if(i==0) client.print(F("Select")); else client.print(i,DEC); client.print(F("</option>")); } client.print(F("</select></span><br/><input id='wformsubmit' type='submit' value='Save' style='display: none;' /></form></li>"));
        client.print(F("<li><label>Last sync</label>As of page load time: [sync state]</li>"));
        client.print(F("<li><label>Sync frequency</label><select id='syncfreq' onchange='save(this)'><option value='min'>Every minute</option><option value='hr'>Every hour (at min :59)</option></select></li>"));
        client.print(F("<li><label>NTP packets</label><select id='ntpok' onchange='save(this)'><option value='y'>Yes (normal)</option><option value='n'>No (for dev/testing)</option></select></li>"));
        client.print(F("<li><label>Brightness</label><select id='bright' onchange='save(this)'><option value='3'>High</option><option value='2'>Medium</option><option value='1'>Low</option></select></li>"));
        client.print(F("<li><label>Version</label>TBD</li>"));
        //After replacing the below from formdev.php, replace " with \"
        client.print(F("</ul></div><script type='text/javascript'>function e(id){ return document.getElementById(id); } function save(ctrl){ if(ctrl.disabled) return; ctrl.disabled = true; let ind = ctrl.nextSibling; if(ind && ind.tagName==='SPAN') ind.parentNode.removeChild(ind); ind = document.createElement('span'); ind.innerHTML = '&nbsp;<span class=\"saving\">Saving&hellip;</span>'; ctrl.parentNode.insertBefore(ind,ctrl.nextSibling); let xhr = new XMLHttpRequest(); xhr.onreadystatechange = function(){ if(xhr.readyState==4){ ctrl.disabled = false; if(xhr.status==200 && !xhr.responseText){ if(ctrl.id=='wform'){ e('content').innerHTML = '<p class=\"ok\">Wi-Fi changes applied.</p><p>' + (e('wssid').value? 'Now attempting to connect to <strong>'+e('wssid').value+'</strong>.</p><p>If successful, the clock will display its IP address. To access this settings page again, connect to <strong>'+e('wssid').value+'</strong> and visit that IP address. (If you miss it, hold Select for 5 seconds to see it again.)</p><p>If not successful, the clock will display <strong>7777</strong>. ': '') + 'To access this settings page again, (re)connect to Wi-Fi network <strong>Clock</strong> and visit <a href=\"http://7.7.7.7\">7.7.7.7</a>.</p>'; clearTimeout(timer); } else { ind.innerHTML = '&nbsp;<span class=\"ok\">OK!</span>'; setTimeout(function(){ if(ind.parentNode) ind.parentNode.removeChild(ind); },1500); } } else ind.innerHTML = '&nbsp;<span class=\"error\">'+xhr.responseText+'</span>'; timer = setTimeout(timedOut, 120000); } }; xhr.open('POST', './', true); xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded'); if(ctrl.id=='wform'){ switch(e('wtype').value){ case '': e('wssid').value = ''; e('wpass').value = ''; case 'wpa': e('wki').value = '0'; case 'wep': default: break; } xhr.send('wssid='+e('wssid').value+'&wpass='+e('wpass').value+'&wki='+e('wki').value); } else { xhr.send(ctrl.id+'='+ctrl.value); } } function wformchg(initial){ if(initial) e('wtype').value = (e('wssid').value? (e('wki').value!=0? 'wep': 'wpa'): ''); e('wa').style.display = (e('wtype').value==''?'none':'inline'); e('wb').style.display = (e('wtype').value=='wep'?'inline':'none'); if(!initial) e('wformsubmit').style.display = 'inline'; } function timedOut(){ e('content').innerHTML = 'Clock settings page has timed out. Please hold Select for 5 seconds to reactivate it, then <a href=\"./\">refresh</a>.'; } wformchg(true); let timer = setTimeout(timedOut, 120000);</script></body></html>"));
        //client.print(F(""));
      } //end get
      else { //requestType==2 - handle what was POSTed
        //client.print(currentLine);
        //syncfreq=hr
        //syncfreq=min
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
            client.print(F(" wssid="));
            client.print(wssid);
            client.print(F(" wpass="));
            client.print(wpass);
            client.print(F(" wki="));
            client.print(wki);
            client.println();
          requestType = 3; //triggers an admin restart after the client is closed, below
        } else {
          client.print(F("Not yet supported: "));
          client.println(currentLine);
        }
      } //end post
    } //end if requestType
    
    if(false){

        switch(requestType){
          case 0: //display the full page

            break;
          case 1: //sync frequency
            client.print(rtcChangeMinuteSync()? F("Now syncing every minute."): F("Now syncing every hour at minute 59."));
            break;
          case 2:
            client.print(networkToggleNTPTest()? F("Now preventing incoming NTP packets."): F("Now allowing incoming NTP packets."));
            break;
          case 3:
            client.print(F("Display brightness set to "));
            client.print(displayToggleBrightness());
            client.print(F("."));
            break;
          default:
            client.print(F("Error: unknown request.")); break;
        }
        // The HTTP response ends with another blank line:
        client.println();
      } //end empty line / end of request
    
    client.stop();
    Serial.println("client disconnected");
  }
}




int networkToggleNTPTest(){ //n
  if(!TESTNTPfail) {
    Serial.println(F("Now preventing incoming NTP packets"));
    TESTNTPfail = 1;
  } else {
    Serial.println(F("Now allowing incoming NTP packets"));
    TESTNTPfail = 0;
  }
  return TESTNTPfail;
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