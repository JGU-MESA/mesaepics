//Latest update: 26.04.2018
//Recently: Single command measurement
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
#include <math.h>

//==== Needed packages for ADC ADS1115 ====
#include <Wire.h>
#include <Adafruit_ADS1015.h>


//===== Needed Variables =====
#define MAX_CMD_LENGTH   25


//===== Static/DHCP =====
byte mac[] = {0x90, 0xA2, 0xDA, 0x11, 0x20, 0xA1}; // Enter a MAC address// abacus01: 90-A2-DA-10-5C-8F

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
int daccounter = 0;

//===== ADS1115 setup =====
Adafruit_ADS1115 ads;
float results_curr_mV = 0, results_curr_mA = 0, results_volt = 0;
float bits2volt = 0.0001875F; // Bits to V
/* ADS1115
    @ +/- 6.144V gain (16-bit results, 1 bit for sign),
    15bit available-> 1bit=0.1875mV=1.875e-4V
*/

//=============== Averaging parameter ===============
int numReadings = 32; // numReadings < 2^16

float meas_aver(char channel, float *mean, float *std, float *maxi, float *mini) {

  uint16_t arr[numReadings];
  uint32_t sum = 0;
  float sqDevSum = 0;

  for (int i = 0; i < numReadings ; i++) {
    // Measure channel data
    if (channel == 'c') arr[i] = ads.readADC_Differential_0_1(); // in bits
    else if (channel == 'v') arr[i] = ads.readADC_Differential_2_3(); // in bits
    else break;
    // Sum
    sum += arr[i]; // in bits
    // Maximum and minimum
    if (i == 1) {
      *maxi = arr[i];
      *mini = arr[i];
    }
    if (*maxi < arr[i]) *maxi = arr[i];
    if (*mini > arr[i]) *mini = arr[i];
  }
  *maxi = *maxi * bits2volt;
  *mini = *mini * bits2volt;
  *mean = (float)sum / numReadings * bits2volt; // Result in volt

  for (int i = 0; i < numReadings ; i++) {
    float d = ((float)sum / numReadings - arr[i]);
    sqDevSum += d * d;
  }
  *std = sqrt(sqDevSum / numReadings) * bits2volt; // Result in volt
}

//=============== Average all ===============
float meas_aver_all(float *mean_curr, float *std_curr, float *maxi_curr, float *mini_curr,
                    float *mean_volt, float *std_volt, float *maxi_volt, float *mini_volt) {

  uint16_t arrc[numReadings];
  uint16_t arrv[numReadings];
  uint32_t sumc = 0;
  uint32_t sumv = 0;
  float sqDevSumc = 0;
  float sqDevSumv = 0;

  for (int i = 0; i < numReadings ; i++) {
    // Measure channel data
    arrc[i] = ads.readADC_Differential_0_1(); // in bits
    arrv[i] = ads.readADC_Differential_2_3(); // in bits
    // Sum
    sumc += arrc[i]; // in bits
    sumv += arrv[i]; // in bits
    // Maximum and minimum
    if (i == 1) {
      *maxi_curr = arrc[i];
      *mini_curr = arrc[i];
      *maxi_volt = arrv[i];
      *mini_volt = arrv[i];
    }
    if (*maxi_curr < arrc[i]) *maxi_curr = arrc[i];
    if (*mini_curr > arrc[i]) *mini_curr = arrc[i];
    if (*maxi_volt < arrv[i]) *maxi_volt = arrv[i];
    if (*mini_volt > arrv[i]) *mini_volt = arrv[i];
  }
  *maxi_curr = *maxi_curr * bits2volt;
  *mini_curr = *mini_curr * bits2volt;
  *maxi_volt = *maxi_volt * bits2volt;
  *mini_volt = *mini_volt * bits2volt;
  *mean_curr = (float)sumc / numReadings * bits2volt; // Result in volt
  *mean_volt = (float)sumv / numReadings * bits2volt; // Result in volt

  for (int i = 0; i < numReadings ; i++) {
    float dc = ((float)sumc / numReadings - arrc[i]);
    float dv = ((float)sumv / numReadings - arrv[i]);
    sqDevSumc += dc * dc;
    sqDevSumv += dv * dv;
  }
  *std_curr = sqrt(sqDevSumc / numReadings) * bits2volt; // Result in volt
  *std_volt = sqrt(sqDevSumv / numReadings) * bits2volt; // Result in volt
}

//=============== Setup ===============
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

  //===== ADC setup ADS1115 =====
  ads.begin();

  //===== Serial communication on startup =====
  Serial.print("server is at ");
  Serial.println(Ethernet.localIP());

}

//=============== Loop ===============
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
      Serial.print("server is currently at "); Serial.println(Ethernet.localIP());
      Serial.print("ip_eeprom.dhcp_state: "); Serial.print(ip_eeprom.dhcp_state);
      if (ip_eeprom.dhcp_state)
        Serial.println("(DHCP)");
      else {
        Serial.println("(static)"); Serial.print("EEPROM ip ");
        for (int i = 0; i <= 2; i++) {
          Serial.print(ip_eeprom.ip_addr[i]); Serial.print(".");
          if (i == 2) Serial.println(ip_eeprom.ip_addr[3]);
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


//=============== Function that reads the commands via Ethernet ===============
void readTelnetCommand(char c, EthernetClient & client) {
  if (cmdstr.length() == MAX_CMD_LENGTH) {
    cmdstr = "";
  }

  if (c == '\n')
    parseCommand(client);
  else
    cmdstr += c;

  //Debug
  //Serial.print("cmdstr_read = ");
  //Serial.println(cmdstr);
}


//=============== Function that executes the read command ===============
void parseCommand(EthernetClient & client) {

  //===== Debug: Show received command on serial terminal =====
  //Serial.print("cmdstr = ");
  //Serial.println(cmdstr);

  //===== QUIT =====
  if (cmdstr.equals("quit")) {
    client.stop();
    connected = false;
  }
  /*
     !!!!!!!!!!!!!!!!!!!!!!!!!
     Change Code from here on
     !!!!!!!!!!!!!!!!!!!!!!!!!
  */

  //===== HELP =====
  else if (cmdstr.equals("help")) {
    client.println("--- Abacus Heinzinger Help ---");
    client.println("quit        : close the connection");
    client.println("ip?         : get ip address");
    client.println("meas:curr?  : measure current in uA (10V=10mA)");
    client.println("meas:volt?  : measure voltage in kV (10V=200kV)");
    client.println("meas:all?   : measure current and voltage");
    client.println("meas:num?   : show number of readings/samples");
  }

  //===== IP =====
  else if (cmdstr.equals("ip?"))
    client.println(Ethernet.localIP());

  //===== Measure current =====
  //Measure current in mA, maximal current 20 mA = 5V
  else if (cmdstr.equals("meas:curr?")) {
    float mean_curr = 0;
    float std_curr = 0;
    float max_curr = 0;
    float min_curr = 0;
    meas_aver('c', &mean_curr, &std_curr, &max_curr, &min_curr); // Results in V
    float i_scale_V_to_uA = 2000 ; // scale: 10 000 uA / 5 V;
    client.print("curr_get:"); client.println(mean_curr * i_scale_V_to_uA, 6);
    client.print("curr_std:"); client.println(std_curr * i_scale_V_to_uA, 6);
    client.print("curr_max:"); client.println(max_curr * i_scale_V_to_uA, 6);
    client.print("curr_min:"); client.println(min_curr * i_scale_V_to_uA, 6);
  }

  //===== Measure voltage =====
  else if (cmdstr.equals("meas:volt?")) {
    float mean_volt = 0;
    float std_volt = 0;
    float max_volt = 0;
    float min_volt = 0;
    meas_aver('v', &mean_volt, &std_volt, &max_volt, &min_volt); // Get results in volt
    float u_scale_V_to_kV = 200 / 5.0; // 200kV corresponds to 5V
    client.print("volt_get:"); client.println(mean_volt * u_scale_V_to_kV, 6);
    client.print("volt_std:"); client.println(std_volt * u_scale_V_to_kV, 6);
    client.print("volt_max:"); client.println(max_volt * u_scale_V_to_kV, 6);
    client.print("volt_min:"); client.println(min_volt * u_scale_V_to_kV, 6);
  }

  //===== Measure all =====
  else if (cmdstr.equals("meas:all?")) {
    float mean_curr = 0;
    float std_curr = 0;
    float max_curr = 0;
    float min_curr = 0;
    float mean_volt = 0;
    float std_volt = 0;
    float max_volt = 0;
    float min_volt = 0;
    meas_aver_all(&mean_curr, &std_curr, &max_curr, &min_curr,
                  &mean_volt, &std_volt, &max_volt, &min_volt); // Get results in volt
    float i_scale_V_to_uA = 2000 ; // scale: 10 000 uA / 5 V;
    float u_scale_V_to_kV = 200 / 5.0; // 200kV corresponds to 5V
    client.print("curr_get:"); client.println(mean_curr * i_scale_V_to_uA, 6);
    client.print("curr_std:"); client.println(std_curr * i_scale_V_to_uA, 6);
    client.print("curr_max:"); client.println(max_curr * i_scale_V_to_uA, 6);
    client.print("curr_min:"); client.println(min_curr * i_scale_V_to_uA, 6);
    client.print("volt_get:"); client.println(mean_volt * u_scale_V_to_kV, 6);
    client.print("volt_std:"); client.println(std_volt * u_scale_V_to_kV, 6);
    client.print("volt_max:"); client.println(max_volt * u_scale_V_to_kV, 6);
    client.print("volt_min:"); client.println(min_volt * u_scale_V_to_kV, 6);
  }

  //===== Show numReadings =====
  else if (cmdstr.equals("meas:num?")) {
    client.print("numReadings:"); client.println(numReadings);
  }

  //===== Set numReadings =====
  else if (cmdstr.startsWith("num ")) {
    int num = cmdstr.substring(3).toInt();
    if ((2 <= num) && (num <= 128))
    {
      numReadings = num;
      client.println("ok");
    }
    else {
      client.println("Number out of range");
    }
  }

  //===== Invalid Command, HELP =====
  else {
    client.println("Invalid command, type help");
  }

  cmdstr = "";
}







