//network-none.h

#ifndef NETWORK_NONE_H
#define NETWORK_NONE_H

void networkSetup(){}
void networkLoop(){}
void networkStartWiFi(){}
void networkStartAP(){}
void networkDisconnectWiFi(){}
void startNTP(){}
void checkNTP(){}
bool networkNTPOK(){}
void networkStartAdmin(){}
void networkStopAdmin(){}
void checkClients(){}
void networkToggleNTPTest(){}
void printTODFromMils(unsigned long t){}
void checkForWiFiStatusChange(){}

#endif