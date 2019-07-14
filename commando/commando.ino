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
 # 3. Atlantic Scientific Conductivity Circuit
 # 4. Atlantic Scientific Conductivity Probe
 # 5. Universal 150PSI Pressure Transducer
 #
 # A0- Pressure Signal
 # A1
 # A2
 # A3
 # A4- SDA
 # A5- SCL
 #
 # D0
 # D1
 # D2-pumpRelay
 # D3
 # D4-awayModeSignal
 # D5-runStateSignal
 # D6
 # D7-dischargeRelay
 # D8-fillRelay
 # D9
 # D10-spareRelay
 # D11
 # D12 Pressure Led
 # D13 High TDS Led
 # GND Aref
 #
 */

#include <millisDelay.h>
#include <Wire.h>                //enable I2C.
#define address 100              //default I2C ID number for EZO EC Circuit.



char computerdata[20];           //we make a 20 byte character array to hold incoming data from a pc/mac/other.
byte received_from_computer = 0; //we need to know how many characters have been received.
byte code = 0;                   //used to hold the I2C response code.
char ec_data[48];                //we make a 48 byte character array to hold incoming data from the EC circuit.
byte in_char = 0;                //used as a 1 byte buffer to store inbound bytes from the EC Circuit.
byte i = 0;                      //counter used for ec_data array.
int time_ = 600;                 //used to change the delay needed depending on the command sent to the EZO Class EC Circuit.

char *ec;                        //char pointer used in string parsing.
char *tds;                       //char pointer used in string parsing.
char *sal;                       //char pointer used in string parsing.
char *sg;                        //char pointer used in string parsing.

float ec_float;                  //float var used to hold the float value of the conductivity.
float tds_float;                 //float var used to hold the float value of the TDS.
float sal_float;                 //float var used to hold the float value of the salinity.
float sg_float;                  //float var used to hold the float value of the specific gravity.

// Values for sensor readings
float potableWater = 1500;
const float offset = 0.483;
float voltage, pressure;

//Define Relay Pins
byte pumpRelay = 2;
byte dischargeRelay = 7;
byte fillRelay = 8;
//byte spareRelay(10,OUTPUT);    //Disabled

//Define Sensors and Logic input
byte runStateSignal = 5;
byte awayModeSignal = 4;

// State buffers
int running = 0;
int saturatedMembrane = 0;
int awayModeState = 0;
int shortStroke = 0;

//define timing variables
millisDelay runTime;
millisDelay highTDS;
millisDelay awayModeTimer;
millisDelay dayCycleTimer;
millisDelay shortStrokeTimer;
unsigned long maxRunTime = 14400000;
unsigned long highTdsDelay = 600000;
unsigned long restTime = 1200000;
unsigned long awayModeDelay = 259200000;
unsigned long shortStrokeDelay = 20000;
unsigned long dayCycle = 86400000;
unsigned long flushCycle = 600000;

// Define LED Output
byte tdsLED = 13;
byte pressureLED = 12;

void setup(){                    //hardware initialization
  Serial.begin(9600);            //enable serial port.
  Wire.begin();                  //enable I2C port.

pinMode(runStateSignal,INPUT);
pinMode(awayModeSignal,INPUT);

pinMode(pumpRelay,OUTPUT);
pinMode(dischargeRelay,OUTPUT);
pinMode(fillRelay,OUTPUT);

pinMode(tdsLED,OUTPUT);
pinMode(pressureLED,OUTPUT);

digitalWrite(pumpRelay,LOW);
digitalWrite(dischargeRelay,LOW);
digitalWrite(fillRelay,LOW);
//digitalWrite(spareRelay,LOW);    //Disable

running = 0;
saturatedMembrane = 0;
awayModeState = 0;
shortStroke = 0;

dayCycleTimer.start(dayCycle);

}

void loop() {                                                              //the main loop.

  if (Serial.available() > 0) {                                             //if data is holding in the serial buffer
    received_from_computer = Serial.readBytesUntil(13, computerdata, 20); //we read the data sent from the serial monitor(pc/mac/other) until we see a <CR>. We also count how many characters have been received.
    computerdata[received_from_computer] = 0;                             //stop the buffer from transmitting leftovers or garbage.
    computerdata[0] = tolower(computerdata[0]);                           //we make sure the first char in the string is lower case.
    if (computerdata[0] == 'c' || computerdata[0] == 'r')time_ = 600;     //if a command has been sent to calibrate or take a reading we wait 600ms so that the circuit has time to take the reading.
    else time_ = 300;


    Wire.beginTransmission(address);                                        //call the circuit by its ID number.
    Wire.write(computerdata);                                               //transmit the command that was sent through the serial port.
    Wire.endTransmission();                                                 //end the I2C data transmission.

    if (strcmp(computerdata, "sleep") != 0) {                               //if the command that has been sent is NOT the sleep command, wait the correct amount of time and request data.
                                                                            //if it is the sleep command, we do nothing. Issuing a sleep command and then requesting data will wake the EC circuit.

      delay(time_);                                                         //wait the correct amount of time for the circuit to complete its instruction.

      Wire.requestFrom(address, 48, 1);                                     //call the circuit and request 48 bytes (this is more than we need)
      code = Wire.read();                                                   //the first byte is the response code, we read this separately.


      switch (code) {                           //switch case based on what the response code is.
        case 1:                                 //decimal 1.
          Serial.println("Success");            //means the command was successful.
          break;                                //exits the switch case.

        case 2:                                 //decimal 2.
          Serial.println("Failed");             //means the command has failed.
          break;                                //exits the switch case.

        case 254:                               //decimal 254.
          Serial.println("Pending");            //means the command has not yet been finished calculating.
          break;                                //exits the switch case.

        case 255:                               //decimal 255.
          Serial.println("No Data");            //means there is no further data to send.
          break;                                //exits the switch case.
      }

      while (Wire.available()) {                 //are there bytes to receive.
        in_char = Wire.read();                   //receive a byte.
        ec_data[i] = in_char;                    //load this byte into our array.
        i += 1;                                  //incur the counter for the array element.
        if (in_char == 0) {                      //if we see that we have been sent a null command.
          i = 0;                                 //reset the counter i to 0.
          Wire.endTransmission();                //end the I2C data transmission.
          break;                                 //exit the while loop.
        }
      }


      Serial.println(ec_data);                  //print the data.
      Serial.println();                         //this just makes the output easier to read by adding an extra blank line 
    }


    if (computerdata[0] == 'r') string_pars(); //uncomment this function if you would like to break up the comma separated string into its individual parts.
  }

  if (dayCycleTimer.justFinished()) {
    dailyCycle();
    Serial.println("Daily Cycle Started");
  }

  if (shortStrokeTimer.justFinished()) {
    stopPump();
    Serial.println("Short stroke prevention timer finished");
  }

  if (digitalRead(awayModeSignal) == 1) {
    if (awayModeState == 0) {
        awayModeState = 1;
        awayModeTimer.start(awayModeDelay);
        Serial.println("Away mode timer started");
    }
  }

  if (digitalRead(awayModeSignal) == 0) {
    if (awayModeState == 1) {
       awayModeTimer.stop();
       Serial.println("Away mode timer stopped");
       awayModeState = 0;
    }
  }

  if (awayModeTimer.justFinished()) {
        delay(restTime);
        Serial.println("Away mode timer finished");
  }


  else if (digitalRead(runStateSignal) == 1) {

    // voltage = analogRead(A0) * 5.00 / 1024;
    // pressure = (voltage - offset) * 400;

    Wire.beginTransmission(address);
    Wire.write('r');                                               //transmit the command that was sent through the serial port.
    Wire.endTransmission();                                                 //end the I2C data transmission.

    delay(time_);                                                         //wait the correct amount of time for the circuit to complete its instruction.


    while (Wire.available()) {                 //are there bytes to receive.

       in_char = Wire.read();                   //receive a byte.
       ec_data[i] = in_char;                    //load this byte into our array.
       i += 1;                                  //incur the counter for the array element.
       if (in_char == 0) {                      //if we see that we have been sent a null command.
         i = 0;                                 //reset the counter i to 0.
         Wire.endTransmission();                //end the I2C data transmission.
         break;                                 //exit the while loop.
       }
     }

    string_pars();
    if (shortStroke == 1){
      shortStrokeTimer.stop();
      Serial.println("Short stroke timer stopped");
      shortStroke = 0;
    }

    if (running == 0) {
      startPump();
      delay(30000);
    }

    if (ec_float < potableWater) {
      fillTank();
      saturatedMembrane = 1;
    }

    else if (saturatedMembrane == 1) {
      dischargeProduct();
      highTDSState();
    }

    else {
      dischargeProduct();
    }

    if (runTime.justFinished()) {
      stopPump();
      delay(restTime);
      Serial.println("Max run time finished");
    }

    if (highTDS.justFinished()) {
      tdsAlarm();
    }

    delay(1000);

/*
    if (pressure > 345) {
      pressureAlert();
    }
*/

  }
  else if (running == 1 && shortStroke == 0) {
    shortStrokeTimer.start(shortStrokeDelay);
    Serial.println("Short stroke timer started");
    shortStroke = 1;
  }

}


void string_pars() {                  //this function will break up the CSV string into its 4 individual parts. EC|TDS|SAL|SG.
                                      //this is done using the C command “strtok”.

  ec = strtok(ec_data, ",");          //let's pars the string at each comma.
  tds = strtok(NULL, ",");            //let's pars the string at each comma.
  sal = strtok(NULL, ",");            //let's pars the string at each comma.
  sg = strtok(NULL, ",");             //let's pars the string at each comma.

    if (Serial.available() > 0) {                                             //if data is holding in the serial buffer

         Serial.print("EC:");                //we now print each value we parsed separately.
         Serial.println(ec);                 //this is the EC value.

         Serial.print("TDS:");               //we now print each value we parsed separately.
         Serial.println(tds);                //this is the TDS value.

         Serial.print("SAL:");               //we now print each value we parsed separately.
         Serial.println(sal);                //this is the salinity value.

         Serial.print("SG:");               //we now print each value we parsed separately.
         Serial.println(sg);                //this is the specific gravity.
         Serial.println();                  //this just makes the output easier to read by adding an extra blank line 
    }

    ec_float=atof(ec);
    tds_float=atof(tds);
    sal_float=atof(sal);
    sg_float=atof(sg);
}

void startPump(){
    // Set Relay States
    digitalWrite(pumpRelay,HIGH);
    digitalWrite(dischargeRelay,HIGH);
    digitalWrite(fillRelay,LOW);
    // Start Run Timer
    runTime.start(maxRunTime);
    running = 1;
    dayCycleTimer.stop();
    Serial.println("System started");
}

void stopPump(){
    // Set Relay State
    digitalWrite(pumpRelay,LOW);
    digitalWrite(fillRelay,LOW);
    digitalWrite(dischargeRelay,HIGH);
    delay(2000);                            //Delay 2 Seconds to release pressure.
    digitalWrite(dischargeRelay,LOW);
    runTime.stop();
    running = 0;
    saturatedMembrane = 0;
  //  digitalWrite(runStateSignal, LOW);
    dayCycleTimer.start(dayCycle);
    Serial.println("System Stopped");
}

void fillTank(){
    // Set Relay State
    digitalWrite(dischargeRelay,LOW);
    digitalWrite(fillRelay,HIGH);
    highTDS.stop();
    Serial.println("Fill tank");
}

void dischargeProduct(){
    digitalWrite(dischargeRelay,HIGH);
    digitalWrite(fillRelay,LOW);
    Serial.println("Discharge Product");
}


void highTDSState(){
    dischargeProduct();
    highTDS.start(highTdsDelay);
    Serial.println("High TDS State Called");
}

void tdsAlarm(){
    stopPump();
    digitalWrite(tdsLED, HIGH);
    delay(restTime);
    Serial.println("High TDS check membrane");
}

void pressureAlert(){
    stopPump();
    digitalWrite(pressureLED,HIGH);
    delay(restTime);
    Serial.println("High Pressure");
}

void dailyCycle(){
    startPump();
    delay(flushCycle);
    stopPump();
    Serial.println("Day Cycle Called");
}

