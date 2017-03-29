//Latest update: 16.02.17
//Recently: Calibration implemented
/*
  Arduino_TCP
 
A simple TCP Server that allows to talk to the Arduino via Telnet.
It can process the given commands and execute them.

This script is written for the Arduino Leonardo ETH.

//TELNET-Version

Update 12.10.16:
  abacus was updated with an additional card bought from adafruit:
  ADS1115 16-Bit ADC 4 channel (A0-A3), used as diferential comparator, where
  A0 measures current against A1 (grounded)
  A2 measures volt against A3 (grounded)
  By default the ADC input range is 6.144V, 1 bit = 0.1875mV
  More information to be found at https://learn.adafruit.com/adafruit-4-channel-adc-breakouts/programming
*/

// Needed Packages
#include <SPI.h>
#include <Ethernet2.h> //Maybe this package needs to be changed
#define MAX_CMD_LENGTH   25
//All pins are at startup by default in INPUT-Mode (besides PIN 13!!)
#define testled 13 //Select testled pin
#define pledpin 11 //Select preparation LED pin
#define voltsetpin 6 //Select voltout pin
#define currsetpin 9 //Select current pin
#define onoffpin 12 //Select onoff pin
#define curranalogreadpin A0
#define voltanalogreadpin A1

//Needed packages for ADS1115
#include <Wire.h>
#include <Adafruit_ADS1015.h>

//Needed packages for MCP4725
#include <Adafruit_MCP4725.h>

byte mac[] = {0x90, 0xA2, 0xDA, 0x10, 0x5C, 0x8F}; // Enter a MAC address
// abacus01: 90-A2-DA-10-5C-8F
byte ip[] = { 10, 32, 240, 71 }; // IP adress of mesa net

// Initialize the Ethernet server library with a port you want to use
EthernetServer server(23); //TELNET defaults to port 23

// Needed variables or constants

boolean connected = false; // whether or not the client was connected previously
String cmdstr;
float currout = 20.0;
float voltout = 0.0;

//ADS1115 setup
Adafruit_ADS1115 ads;
float results_curr_mV=0,results_curr_mA=0,results_volt=0;
float multiplier = 0.1875F; /* ADS1115  @ +/- 6.144V gain (16-bit results),1bit=0.1875mV */

//MCP4725
Adafruit_MCP4725 dac;

//Calibration Parameter current monitor i_mon=mi*i_adc+bi
//Measured current through R=100k with Peaktech dvm
float mi = 1.0012, bi=-10.7445/1000.0;

//Calibration Parameter voltage monitor v_mon=mv*v_adc+bv
//Measured voltage parallel at R=100k with Peaktech dvm
float mv = 0.9999, bv=-0.65;


void setup() {
  Serial.begin(9600);  // Open serial communications and wait for port to open:

  Ethernet.begin(mac,ip);  // start the Ethernet connection and the server using DHCP:
  // alternative: using a static ip Ethernet.begin(mac,ip)
  server.begin(); // start server
  pinMode(testled, OUTPUT); // set led pin to output
  pinMode(pledpin, OUTPUT); // set led pin to output
  pinMode(onoffpin, OUTPUT); // set onoffpin to output
  pinMode(voltsetpin, OUTPUT); // set voltsetpin to output
  pinMode(currsetpin, OUTPUT); // set currsetpin to output
  
  //Serial communication on startup
  Serial.print("server is at ");
  Serial.println(Ethernet.localIP());
  
  //Set Default Values of pins
//  analogWrite(voltsetpin, voltout*255.0/500.0);
  analogWrite(voltsetpin, voltout*255.0/500.0);
  analogWrite(currsetpin, currout*255.0/20.0);
  digitalWrite(onoffpin,LOW);
  digitalWrite(pledpin, LOW);
  
  //ADS1115-VDD
  ads.begin();
  
  pinMode(1,OUTPUT); //Arduino pin 1 is VDD of ADS1115; needs to be HIGH!!
  digitalWrite(1,HIGH); //Arduino pin 1 is VDD of ADS1115; needs to be HIGH!!
  
  //MCP4725
  dac.begin(0x62);
  dac.setVoltage(0, false);
  
}


void loop() {
  // Create a client connection
   EthernetClient client = server.available();
   if (Serial.available() > 0) {
                // read the incoming byte:
               String a = Serial.readString();
               a.trim();
               // while (a.endsWith('\n')|| a.endsWith('\r')){
               //              }
               Serial.println(a);
               if (a.equals("ip?")){
                  Serial.print("server is at ");
                  Serial.println(Ethernet.localIP());
                }
    }
  if (client) {
    if (!connected) {
      client.flush(); //Clear out the input buffer
      cmdstr=""; //clear the commandString variable
      connected = true;
      }
     
    if (client.available() > 0) {
      char zeichen = client.read();
      //convert all capitalized letters into small ones
      if ((65<= zeichen) && (zeichen <= 90))
         zeichen += 32;
      //Filter all unwanted characters and first character must not be space
      //only allowed: \n * : . ' ' ? a..z A..Z 0..9
      if (
        (zeichen == '\n') || (zeichen == '*') || (zeichen == ':') || (zeichen == '.') || ((zeichen == ' ') && cmdstr.length()) ||
        (zeichen == '?') || ((48<= zeichen) && (zeichen <= 57)) || ((97<= zeichen) && (zeichen <= 122))
        )
          readTelnetCommand(zeichen,client);
      }
    }

   
  }


void readTelnetCommand(char c, EthernetClient &client) {
  if(cmdstr.length() == MAX_CMD_LENGTH) {
    cmdstr = "";
  }
  
  if (c=='\n')
    parseCommand(client);
  else 
    cmdstr += c;
    
  //Serial.print("cmdstr_read = ");
  //Serial.println(cmdstr);
  
}

void parseCommand(EthernetClient &client) {
  
  Serial.print("cmdstr = ");
  Serial.println(cmdstr);
  
  //===== QUIT =====
  if(cmdstr.equals("quit")) {
      client.stop();
      connected = false;
  } 
  
  //===== HELP =====
  else if(cmdstr.equals("help")) {
      client.println("--- Telnet Server Help ---");
      client.println("on | off    : switch test led on/off");
      client.println("tled?       : read test led state");
      client.println("quit        : close the connection");
      client.println("ip?         : get ip address");
      client.println("volt xx.xx  : set voltage");
      client.println("curr xx.xx  : set maximal current");
      client.println("volt?       : reads set voltage");
      client.println("curr?       : reads set current");
      client.println("meas:volt?  : measures voltage in V");
      client.println("meas:curr?  : measures current in mA");
      client.println("outp {0|1}  : set output on off");
      client.println("pled {0|1}  : switch preparation led on/off");
      client.println("pled?       : read preparation led state");
  } 
  
  //===== IP =====
  else if(cmdstr.equals("ip?")) {
      client.println(Ethernet.localIP());
      Serial.println(Ethernet.localIP());
  } 
  
  //===== Test-LED on/off =====
  else if(cmdstr.equals("on")) {
      digitalWrite(testled, HIGH);
  } 
  
  else if(cmdstr.equals("off")) {
      digitalWrite(testled, LOW);
  } 
  
  //===== Read Test-LED state on/off =====
  else if(cmdstr.equals("tled?")){
       client.print("tled:");   
       client.println(digitalRead(testled));
  }  
  
  //===== Set output voltage at pin voltout =====
  else if(cmdstr.startsWith("volt ")){
      voltout = cmdstr.substring(4).toFloat();
      if ((0 <= voltout) && (voltout <= 500))
        {
        Serial.println(voltout,3);
        Serial.println(voltout*255.0/500.0,3);
        }
      else { 
        Serial.println("Set:Voltage out of range, reset to 0.");
        client.println("Error: Voltage out of range");
        voltout = 0;
      }
      analogWrite(voltsetpin, (voltout*255.0/500.0));
      dac.setVoltage(voltout*4095.0/500.0, false);
  } 
  
  //===== Set maximum output current at pin currout =====
  else if(cmdstr.startsWith("curr ")){
      currout = cmdstr.substring(4).toFloat();
      if ((0< currout) && (currout <= 20))
        {
        Serial.println(currout,3);
        Serial.println(currout*255.0/20.0,3);
        }
      else { 
        Serial.println("Set current out of range, reset to 0.");
        currout = 0;
      }
      analogWrite(currsetpin, (currout*255.0/20.0));
  } 
  
  //===== Show current set-volt parameter =====
  else if(cmdstr.equals("volt?")) {
      client.print("volt_set:");
      client.println(voltout);
  //!!Maximum possible voltage = 5V which is equal 500V for the HMC!!
  } 
  
  //===== Show current set-current parameter =====
  else if(cmdstr.equals("curr?")) {
      client.print("curr_set:");
      client.println(currout);
    //Measure current in mA, !!maximum current of HMC is 20mA = 5V!!;
    //Measuring using ADS1115 in differential mode A0 vs A1 (grounded);
  } 
  
  //===== Measure current =====
   //Measure current in mA, maximal current 20 mA = 5V
  else if(cmdstr.equals("meas:curr?")) {
        //client.println(analogRead(curranalogreadpin)*20.0/1023.0);
        //results_curr_mV=Read signal(ads.readADC_Differential_0_1()) and transfrom bits into mV (*multiplier)
        results_curr_mV = ads.readADC_Differential_0_1()*multiplier;
        //results_curr_mA=results_curr_mV*20mA/5000mV;
        results_curr_mA = results_curr_mV*20/5000;
        client.print("curr_get:");
        //Consider calibration values mi,bi
        //client.println(mi*results_curr_mA+bi,7);
        client.println(results_curr_mA,7);
        //Serial.println(results_curr_mA);
  } 
  
  //===== Measure voltage =====
   //Measure voltage in V, maximal voltage 500 V
  else if(cmdstr.equals("meas:volt?")) {
     // client.println(analogRead(voltanalogreadpin)*500.0/1023.0);
     //results_volt=Read signal(ads.readADC_Differential_2_3())
     //and transfrom bits into V (*multiplier*1000) and 500V HMC = 5V Arduino
     results_volt = ads.readADC_Differential_2_3()*multiplier/1000 * 500.0/5.0;
     client.print("volt_get:");
     //Consider calibration values mv,bv
     client.println(results_volt,7);
  }   
     

  //===== Set Output on|off (1|0) =====
  else if(cmdstr.startsWith("outp ")){
      if (cmdstr.endsWith("1")){
            digitalWrite(onoffpin,HIGH);
            }
      else {
            digitalWrite(onoffpin,LOW);
            }
  } 
  
  
  //===== Read output state on/off =====
  else if(cmdstr.equals("outp?")){
          client.print("outp:");
          client.println(digitalRead(onoffpin));
  } 
  
  
  //===== Set preparation led on/off =====
  else if(cmdstr.startsWith("pled ")){
      if (cmdstr.endsWith("1")){
            digitalWrite(pledpin,HIGH);
            }
      else {
            digitalWrite(pledpin,LOW);
            }
  } 
 
   //===== Read preparation led state on/off =====
  else if(cmdstr.equals("pled?")){
       client.print("pled:");
       client.println(digitalRead(pledpin));
  }  

  
  //===== Invalid Command, HELP =====
  else {
      client.println("Invalid command, type help");
  }
  
  cmdstr = "";
}


