//network-wifinina.h

#ifndef NETWORK_WIFININA_H
#define NETWORK_WIFININA_H

void networkSetup();
void networkLoop();
void networkStartWiFi();
void networkStartAP();
void networkDisconnectWiFi();
void startNTP();
void checkNTP();
bool networkNTPOK();
void networkStartAdmin();
void networkStopAdmin();
void checkClients();
void networkToggleNTPTest();
void checkForWiFiStatusChange();

#endif