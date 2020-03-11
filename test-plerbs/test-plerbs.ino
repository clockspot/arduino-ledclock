#include "hello.h"
//#include "hello2.h"
void setup(){
  Serial.begin(9600); while(!Serial); //only works on 33 IOT
  Serial.println(F("Hello world!"));
  //Serial.print(F("The thing is ")); Serial.println(helloAgain(),DEC);
  helloAgain();
}
void loop(){}