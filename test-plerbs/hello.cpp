//hello.cpp

#include "Arduino.h"
#include "hello2.h"

void helloAgain(){
  //Serial.println("Called hello again");
  Serial.print("The thing is "); helloAgain2();
}