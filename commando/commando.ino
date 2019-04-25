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
 #
 #
 # */

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

//Define Relay Pins
byte pumpRelay(2,OUTPUT);
byte dischareRelay(7,OUTPUT);
byte fillRelay(8,OUTPUT);
//byte spareRelay(10,OUTPUT);

//Define Sensors and Logic input


void setup(){                     //hardware initialization
  Serial.begin(9600);            //enable serial port.
  Wire.begin();                  //enable I2C port.
  digitalWrite(pumpRelay,LOW);
  digitalWrite(dischargeRelay,LOW);
  digitalWrite(fillRelay,LOW);
//digitalWrite(spareRelay,LOW);


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


    //if (computerdata[0] == 'r') string_pars(); //uncomment this function if you would like to break up the comma separated string into its individual parts.

  }

}
void string_pars() {                  //this function will break up the CSV string into its 4 individual parts. EC|TDS|SAL|SG.
                                      //this is done using the C command “strtok”.

  ec = strtok(ec_data, ",");          //let's pars the string at each comma.
  tds = strtok(NULL, ",");            //let's pars the string at each comma.
  sal = strtok(NULL, ",");            //let's pars the string at each comma.
  sg = strtok(NULL, ",");             //let's pars the string at each comma.

  Serial.print("EC:");                //we now print each value we parsed separately.

  Serial.println(ec);                 //this is the EC value.

  Serial.print("TDS:");               //we now print each value we parsed separately.
  Serial.println(tds);                //this is the TDS value.

  Serial.print("SAL:");               //we now print each value we parsed separately.
  Serial.println(sal);                //this is the salinity value.

  Serial.print("SG:");               //we now print each value we parsed separately.
  Serial.println(sg);                //this is the specific gravity.
  Serial.println();                  //this just makes the output easier to read by adding an extra blank line 
    
  //uncomment this section if you want to take the values and convert them into floating point number.
    ec_float=atof(ec);
    tds_float=atof(tds);
    sal_float=atof(sal);
    sg_float=atof(sg);
}

void startPump(){

    digitalWrite(pumpRelay,HIGH);
    digitalWrite(dischargeRelay,HIGH);
    digitalWrite(fillRelay,LOW);


}



/*
 *
 *int led = 13;
// Pin 13 has an LED connected on most Arduino boards.

millisDelay ledDelay;

void setup() {

  // initialize the digital pin as an output.
  pinMode(led, OUTPUT);
  digitalWrite(led, HIGH); // turn led on

  // start delay
  ledDelay.start(10000);
}

void loop() {
  // check if delay has timed out
  if (ledDelay.isFinished()) {
    digitalWrite(led, LOW); // turn led off
  }
}

