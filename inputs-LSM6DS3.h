//inputs-LSM6DS3.h

#ifndef INPUTS_LSM6DS3_H
#define INPUTS_LSM6DS3_H

void inputsSetup();
void inputsLoop();
bool readInput(byte btn);
void checkBtn(byte btn);
void btnStop();

#endif