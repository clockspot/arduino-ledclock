# arduino-ledclock

![MAX7219 clock](https://i.imgur.com/NtAKrS4.jpg)

Digital clock code with NTP sync for Arduino Nano 33 IoT and MAX7219 LED matrix

## Connecting to settings page

* Grab a device (computer/phone/tablet) with WiFi connectivity.
* Hold **Select** for 5 seconds (or enter 'a' in the serial monitor).
* If the clock **is not** connected to WiFi, it will display `7777` and broadcast a WiFi access point called “Clock”. Connect your device to “Clock” and browse to [7.7.7.7](http://7.7.7.7).
* If the clock **is** connected to WiFi, it will flash its IP address (as a series of three 4-digit numbers). With your device on the same WiFi network, browse to that IP address.
  * To disconnect the clock from WiFi, hold Select for 15 seconds; it will revert to the “Clock” access point.
* For security, the clock will stop serving the settings page (and “Clock” access point if applicable) after 2 minutes of inactivity.