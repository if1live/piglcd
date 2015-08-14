#include <stdio.h>
#include <wiringPi.h>
#include <stdlib.h>

// default
//#define PIN 14

// gpio
//#define PIN 11

// phy
#define PIN 23

int main (int argc, char **argv)
{
  if(argc != 2) {
    return -1;
  }
  int count = atoi(argv[1]);
  
  //if (wiringPiSetup() == -1) {
  //if (wiringPiSetupSys() == -1) {
  if (wiringPiSetupPhys() == -1) {         
  //if (wiringPiSetupGpio() == -1) { 
    return 1;
  }
  
  pinMode(PIN, OUTPUT);

  for(int i = 0 ; i < count ; ++i) {
    digitalWrite(PIN, 1);
    digitalWrite(PIN, 0);
  }

  return 0;
}
    
