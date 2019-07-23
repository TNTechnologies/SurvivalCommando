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
#include <Ezo_i2c.h> //include the EZO I2C library (EZO_i2c.h is customized header file for Atlas Scientific's EZO circuits in I2C mode. Link: https://github.com/Atlas-Scientific/Ezo_I2c_lib)
#define address 100              //default I2C ID number for EZO EC Circuit.

Ezo_board EC = Ezo_board(100, "EC");      //create an EC circuit object who's address is 100 and name is "EC"

bool reading_request_phase = true;        //selects our phase

uint32_t next_poll_time = 0;              //holds the next time we receive a response, in milliseconds
const unsigned int response_delay = 1000; //how long we wait to receive a response, in milliseconds

// Values for sensor readings
float potableWater = 500;
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
bool running = false;
bool saturatedMembrane = false;
bool awayModeState = false;
bool shortStroke = false;
bool startDelayFinished = false;
bool startSignalRecieved = false;

//define timing variables
millisDelay runTime;
millisDelay highTDS;
millisDelay awayModeTimer;
millisDelay dayCycleTimer;
millisDelay runningShortStrokeTimer;
millisDelay startShortStrokeTimer;
unsigned long maxRunTime = 21600000;
unsigned long highTdsDelay = 1800000;
unsigned long restTime = 1800000;
unsigned long awayModeDelay = 259200000;
unsigned long shortStrokeDelay = 600000;
unsigned long startShortStrokeDelay = 60000;
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

running = false;
saturatedMembrane = false;
awayModeState = false;
shortStroke = false;
startDelayFinished = false;
startSignalRecieved = false;

dayCycleTimer.start(dayCycle);

}

void loop() {                                                              //the main loop.



  if (dayCycleTimer.justFinished()) {
    dailyCycle();
    Serial.println("Daily Cycle Started");
  }

  if (runningShortStrokeTimer.justFinished()) {
    stopPump();
    Serial.println("Short stroke prevention timer finished");
  }

  if (startShortStrokeTimer.justFinished()) {
    Serial.println("Start Short Stroke Timer Finished");
    startDelayFinished = true;
  }

  if (digitalRead(awayModeSignal) == true) {
    if (awayModeState == false) {
        awayModeState = true;
        awayModeTimer.start(awayModeDelay);
        Serial.println("Away mode timer started");
    }
  }

  if (digitalRead(awayModeSignal) == false) {
    if (awayModeState == true) {
       awayModeTimer.stop();
       Serial.println("Away mode timer stopped");
       awayModeState = false;
    }
  }


  if (awayModeTimer.justFinished()) {
     Serial.println("Away mode timer finished");
     delay(restTime);
  }

  else if (digitalRead(runStateSignal) == 0 && startShortStrokeTimer.remaining() > 0) {
    startShortStrokeTimer.stop();
    startSignalRecieved = false;
    Serial.println("Start Short Stroke Time Stopped");

}

  else if(digitalRead(runStateSignal) == 1 && startSignalRecieved == false) {
    startShortStrokeTimer.start(startShortStrokeDelay);
    startSignalRecieved = true;
    Serial.println("Start Signal Recieved");

  }

  else if (digitalRead(runStateSignal) == 1 && startDelayFinished == true) {

  if (reading_request_phase) {                          //if were in the phase where we ask for a reading

      //send a read command. we use this command instead of PH.send_cmd("R");
      //to let the library know to parse the reading
      EC.send_read();

      next_poll_time = millis() + response_delay;         //set when the response will arrive
      reading_request_phase = false;                      //switch to the receiving phase
  }
    else {                                                //if were in the receiving phase
      if (millis() >= next_poll_time) {                   //and its time to get the response

        receive_reading(EC);                              //get the reading from the EC circuit
        if (EC.get_reading() < potableWater) {                  //test condition against EC reading
          fillTank();
          saturatedMembrane = true;
        }

        else if (saturatedMembrane == true) {
          dischargeProduct();
          highTDSState();
        }

        else {
          dischargeProduct();
        }

      reading_request_phase = true;                     //switch back to asking for readings
    }
  }
    // voltage = analogRead(A0) * 5.00 / 1024;
    // pressure = (voltage - offset) * 400;

    if (shortStroke == true){
      runningShortStrokeTimer.stop();
      Serial.println("Short stroke timer stopped");
      shortStroke = false;
    }

    if (running == false) {
      startPump();
      delay(30000);
    }

    if (runTime.justFinished()) {
      stopPump();
      Serial.println("Max run time finished");
      delay(restTime);
      startPump();
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
  else if (running == true && shortStroke == false) {
    runningShortStrokeTimer.start(shortStrokeDelay);
    Serial.println("Short stroke timer started");
    shortStroke = true;
  }

}

void startPump(){
    // Set Relay States
    digitalWrite(pumpRelay,HIGH);
    digitalWrite(dischargeRelay,HIGH);
    digitalWrite(fillRelay,LOW);
    // Start Run Timer
    runTime.start(maxRunTime);
    running = true;
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
    running = false;
    saturatedMembrane = false;
  //  digitalWrite(runStateSignal, LOW);
    dayCycleTimer.start(dayCycle);
    Serial.println("System Stopped");
    startDelayFinished = false;
}

void fillTank(){
    // Set Relay State
    digitalWrite(dischargeRelay,LOW);
    digitalWrite(fillRelay,HIGH);
    highTDS.stop();
    Serial.println("Fill tank - EC:");
    Serial.println(EC.get_reading());
}

void dischargeProduct(){
    digitalWrite(dischargeRelay,HIGH);
    digitalWrite(fillRelay,LOW);
    Serial.println("Discharge Product - EC:");
    Serial.println(EC.get_reading());
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


void receive_reading(Ezo_board &Sensor) {               // function to decode the reading after the read command was issued

  Serial.print(Sensor.get_name()); Serial.print(": ");  // print the name of the circuit getting the reading

  Sensor.receive_read();                                //get the response data and put it into the [Sensor].reading variable if successful

  switch (Sensor.get_error()) {                         //switch case based on what the response code is.
    case Ezo_board::SUCCESS:
      Serial.print(Sensor.get_reading());               //the command was successful, print the reading
      break;

    case Ezo_board::FAIL:
      Serial.print("Failed ");                          //means the command has failed.
      break;

    case Ezo_board::NOT_READY:
      Serial.print("Pending ");                         //the command has not yet been finished calculating.
      break;

    case Ezo_board::NO_DATA:
      Serial.print("No Data ");                         //the sensor has no data to send.
      break;
  }
}


