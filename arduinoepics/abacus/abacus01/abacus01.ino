//Latest update: 04.04.2017
//Recently: DHCP / static ip implemented
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

//===== Needed Packages =====
#include <SPI.h>
#include <Ethernet2.h> //Maybe this package needs to be changed
#include <EEPROM.h>

//==== Needed packages for ADC ADS1115 ====
#include <Wire.h>
#include <Adafruit_ADS1015.h>

//==== Needed packages for DAC MCP4725 ====
#include <Adafruit_MCP4725.h>


//===== Needed Variables =====
#define MAX_CMD_LENGTH   25
#define testled 13 //Select testled pin
#define pledpin 11 //Select preparation LED pin
#define voltsetpin 6 //Select voltout pin
#define currsetpin 9 //Select current pin
#define outputonoffpin 12 //Select output on/off pin


//===== Static/DHCP =====
byte mac[] = {0x90, 0xA2, 0xDA, 0x10, 0x5C, 0x8F}; // Enter a MAC address// abacus01: 90-A2-DA-10-5C-8F

struct IPObject {
    boolean dhcp_state; //static IP (false) or DHCP (true)
    int ip_addr[4]; //IP address
};

IPObject ip_eeprom;
int eeAddress = 0; // Defined address to read from and write to EEPROM


//===== EthernetServer =====
// Initialize the Ethernet server library with a port you want to use
EthernetServer server(23); //TELNET defaults to port 23


//===== Further variables or constants for communication =====
boolean connected = false; // whether or not the client was connected previously
String cmdstr;
float currout = 20.0;
float voltout = 0.0;
int daccounter = 0;

//===== ADS1115 setup =====
Adafruit_ADS1115 ads;
float results_curr_mV=0,results_curr_mA=0,results_volt=0;
float multiplier = 0.1875F; /* ADS1115  @ +/- 6.144V gain (16-bit results),1bit=0.1875mV */

//===== MCP4725 =====
Adafruit_MCP4725 dac;

void setup() {
    //===== Serial =====
    Serial.begin(9600); // Open serial communications and wait for port to open

    //===== Ethernet =====
    EEPROM.get(eeAddress, ip_eeprom); // Get last written information about DHCP or static IP and save it to ip_eeprom
    if (ip_eeprom.dhcp_state) 
        Ethernet.begin(mac); // start the Ethernet connection and the server using DHCP
    else {
        byte ip[4];
        for (int i = 0; i <= 3; i++) ip[i] = ip_eeprom.ip_addr[i];
        Ethernet.begin(mac, ip); // start the Ethernet connection and the server using static IP written to EEPROM
    }

    server.begin(); // start server
    
    //===== Default Pin mode setup =====
    //All pins are at startup by default in INPUT-Mode (besides PIN 13!!)
    pinMode(testled, OUTPUT); // set led pin to output
    pinMode(pledpin, OUTPUT); // set led pin to output
    pinMode(outputonoffpin, OUTPUT); // set outputonoffpin to output
    pinMode(voltsetpin, OUTPUT); // set voltsetpin to output
    pinMode(currsetpin, OUTPUT); // set currsetpin to output
  
    //===== Default Value setup =====
    analogWrite(voltsetpin, voltout*255.0/500.0);
    analogWrite(currsetpin, currout*255.0/20.0);
    digitalWrite(outputonoffpin,LOW);
    digitalWrite(pledpin, LOW);
  
    //===== ADC setup ADS1115 =====
    ads.begin();
    pinMode(1,OUTPUT); //Arduino pin 1 is VDD of ADS1115; needs to be HIGH!!
    digitalWrite(1,HIGH); //Arduino pin 1 is VDD of ADS1115; needs to be HIGH!!
  
    //===== DAC setup MCP4725 =====
    dac.begin(0x62);
    dac.setVoltage(0, false); // Leads to problem with serial communication

    //===== Serial communication on startup =====
    Serial.print("server is at ");
    Serial.println(Ethernet.localIP());

}

void loop() {
    //===== Ethernet: Create a client connection =====
    EthernetClient client = server.available();
    EEPROM.get(eeAddress, ip_eeprom); // Get last written information about DHCP or static IP and save it to ip_eeprom

    //==== Evaluate Serial input ====
    if (Serial.available() > 0) {
        Serial.print("server is at ");
        Serial.println(Ethernet.localIP());
        String serialstring = Serial.readString(); // read the incoming byte:
        serialstring.trim(); //removes leading and trailing whitespace
        Serial.println(serialstring); // write the read String

        if (serialstring.equals("help")) {
            Serial.println("--- Serial connection Help ---");
            Serial.println("ip?                             : get current ip address and configuration");
            Serial.println("set ip 10.32.200.44             : set static ip address");
            Serial.println("ip: DHCP                        : set ip to DHCP");
        }

        else if (serialstring.equals("ip?")) {
            Serial.print("server is currently at ");Serial.println(Ethernet.localIP());
            Serial.print("ip_eeprom.dhcp_state: ");Serial.print(ip_eeprom.dhcp_state);
            if (ip_eeprom.dhcp_state) 
                Serial.println("(DHCP)");
            else {
                Serial.println("(static)"); Serial.print("EEPROM ip ");
                for (int i = 0; i <= 2;i++){
                        Serial.print(ip_eeprom.ip_addr[i]);Serial.print(".");
                        if (i==2) Serial.println(ip_eeprom.ip_addr[3]);
                    }
            }
        }
        
        else if (serialstring.startsWith("set ip ")) {
            char achar[23]; // Needed to use sscanf with input serialstring
            int intarray[4]; // Intermediate integer array needed to use sscanf
            
            serialstring.toCharArray(achar, 23); // Needed to use sscanf with String a
            //sscanf(achar, "set ip %3d.%3d.%3d.%3d", &(ip_eeprom.ip_addr[0]), &(ip_eeprom.ip_addr[1]), &(ip_eeprom.ip_addr[3]), &(ip_eeprom.ip_addr[4])); // Write new ip to ip_eeprom //Leads to error!
            sscanf(achar, "set ip %3d.%3d.%3d.%3d", &intarray[0], &intarray[1], &intarray[2], &intarray[3]); //Assign new ip to intermediate array            Serial.print("read ip: ");
            for (int i = 0; i <= 3; i++) {
                ip_eeprom.ip_addr[i] = intarray[i]; // Assign read ip to EEPROM structure
            }
            for (int i = 0; i <= 2; i++) {
                Serial.print(ip_eeprom.ip_addr[i]); Serial.print(".");
                if (i == 2) {
                    Serial.print(ip_eeprom.ip_addr[3]); Serial.println();
                    Serial.println("IP was written to EEPROM (restart necessary)");
                }
            }
            ip_eeprom.dhcp_state = false; // Set IP dhcp_state to 0 for static
            EEPROM.put(eeAddress, ip_eeprom);
            }
            
        else if (serialstring.equals("ip: DHCP")) {
            Serial.println("Unset static ip, set DHCP (restart necessary)");
            ip_eeprom.dhcp_state = true; // Set IP dhcp_state to 1 for DHCP
            EEPROM.put(eeAddress, ip_eeprom);
        }
        
        else Serial.println("Invalid command, type help");
    }
        

    //==== Ethernet ====
    if (client) {
        if (!connected) {
            client.flush(); //Clear out the input buffer
            cmdstr = ""; //clear the commandString variable
            connected = true;
        }

        if (client.available() > 0) {
            char zeichen = client.read();
            //convert all capitalized letters into small ones
            if ((65 <= zeichen) && (zeichen <= 90))
                zeichen += 32;
            //Filter all unwanted characters and first character must not be space
            //only allowed: \n * : . ' ' ? a..z A..Z 0..9
            if (
                (zeichen == '\n') || (zeichen == '*') || (zeichen == ':') || (zeichen == '.') || ((zeichen == ' ') && cmdstr.length()) ||
                (zeichen == '?') || ((48 <= zeichen) && (zeichen <= 57)) || ((97 <= zeichen) && (zeichen <= 122))
            )
                readTelnetCommand(zeichen, client);
        }
    }
}


//===== Function that reads the commands via Ethernet =====
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

//===== Function that executes the read command =====
void parseCommand(EthernetClient &client) {

    //===== Debug: Show received command on serial terminal =====
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
    else if(cmdstr.equals("ip?"))
        client.println(Ethernet.localIP());
    
    //===== Test-LED on/off =====
    else if(cmdstr.equals("on"))
        digitalWrite(testled, HIGH); 
    
    else if(cmdstr.equals("off"))
        digitalWrite(testled, LOW);
    
    //===== Read Test-LED state on/off =====
    else if(cmdstr.equals("tled?")) {
        client.print("tled:");   
        client.println(digitalRead(testled));
    }  
    
    //===== Set output voltage at pin voltout =====
    else if(cmdstr.startsWith("volt ")) {
        voltout = cmdstr.substring(4).toFloat();
        if ((0 <= voltout) && (voltout <= 500)) {
            Serial.println(voltout,3);
            Serial.println(voltout*255.0/500.0,3);
        }
        else { 
            Serial.println("Error: Voltage out of range, reset to 0.");
            client.println("Error: Voltage out of range, reset to 0.");
            voltout = 0;
        }
        dac.setVoltage(voltout*4095.0/500.0, false);//DAC has 12 bit = 4095 steps
    } 
    
    //===== Set maximum output current at pin currout =====
    else if(cmdstr.startsWith("curr ")) {
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
        analogWrite(currsetpin, (currout*255.0/20.0)); //Internal DAC has only 8 bit
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
        results_curr_mV = ads.readADC_Differential_0_1()*multiplier; //results_curr_mV=Read signal(ads.readADC_Differential_0_1()) and transfrom bits into mV (*multiplier)
        results_curr_mA = results_curr_mV*20/5000; //results_curr_mA=results_curr_mV*20mA/5000mV;
        client.print("curr_get:"); client.println(results_curr_mA,7);
    } 
    
    //===== Measure voltage =====
    //Measure voltage in V, maximal voltage 500 V
    else if(cmdstr.equals("meas:volt?")) {
        results_volt = ads.readADC_Differential_2_3()*multiplier/1000 * 500.0/5.0; //results_volt=Read signal(ads.readADC_Differential_2_3()) and transfrom bits into V (*multiplier*1000) and 500V HMC = 5V Arduino
        client.print("volt_get:"); client.println(results_volt,7);
    }   
    
    
    //===== Set Output on|off (1|0) =====
    else if(cmdstr.startsWith("outp ")) {
        if (cmdstr.endsWith("1"))
            digitalWrite(outputonoffpin,HIGH);
        else
            digitalWrite(outputonoffpin,LOW);
    } 
    
    
    //===== Read output state on/off =====
    else if(cmdstr.equals("outp?")) {
            client.print("outp:");
            client.println(digitalRead(outputonoffpin));
    } 
    
    
    //===== Set preparation led on/off =====
    else if(cmdstr.startsWith("pled ")) {
        if (cmdstr.endsWith("1"))
            digitalWrite(pledpin,HIGH);
        else
            digitalWrite(pledpin,LOW);
    } 
    
    //===== Read preparation led state on/off =====
    else if(cmdstr.equals("pled?")) {
        client.print("pled:");
        client.println(digitalRead(pledpin));
    } 
    
    //===== Invalid Command, HELP =====
    else {
        client.println("Invalid command, type help");
    }
    
    cmdstr = "";
}







