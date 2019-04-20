/*
 # This code is for the automation of the Katadyn PowerSurvivor watermaker
 #
 # Editor   :  Vim
 # Date     :  2019.4.19
 # Ver      :  0.1
 # Product  :  SurvivalCommando
 # Author   :  Kyle Boblitt
 #
 # Hardware : 
 # 1. Arduino UNO
 # 2. 4 Channel Relay Shield
 # 3. Homebrew Electric Conductivity Meter
 #
 # */

 byte relayPin[4] = {2,7,8,10};
 //D2 -> RELAY1
 //D7 -> RELAY2
 //D8 -> RELAY3
 //D10-> RELAY4

 char input=0;
 int val;

void setup(){

