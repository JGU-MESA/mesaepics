//Latest update: 06.09.2018
//Recently:
/*
  Arduino_TCP

  A simple TCP Server that allows to talk to the Arduino via Telnet.
  It can process the given commands and execute them.

  This script is written for the Arduino Leonardo ETH.

  //TELNET-Version

*/
const float version = 1.0;
//===== Needed Packages =====
#include <EEPROM.h>
#include <SPI.h>
#include <Ethernet.h> //Maybe this package needs to be changed
#include <digitalWriteFast.h>


//==== Needed packages for DAC MCP4725 ====
#include <Wire.h>
#include <Adafruit_MCP4725.h>

//========== Timer==========
#include <TimerOne.h> //Timer1 handles Ethernet communication


//========== Ethernet ===============
EthernetServer server(23);
EthernetClient client;
String cmdstr;
void readTelnetCommand(char c, EthernetClient &client);
void evaluateClient();

//===== Needed Variables =====
#define MAX_CMD_LENGTH   25
#define testled 2 //Select testled pin

//===== Static/DHCP =====
byte mac[] = {0x90, 0xA2, 0xDA, 0x10, 0x5F, 0x8C}; // Enter a MAC address// abacus01: 90-A2-DA-10-5C-8F
struct IPObject {
  boolean dhcp_state; //static IP (false) or DHCP (true)
  int ip_addr[4]; //IP address
};
IPObject ip_eeprom;
int eeAddress = 0; // Defined address to read from and write to EEPROM

//===== Further variables or constants for communication =====
boolean connected = false; // whether or not the client was connected previously

//===== Laserdiode =====
uint32_t value = 0;
uint32_t Tpulselength = 100; // pulse length
uint32_t Tshot = 200; // multiShot: Time between falling and rising flank
uint32_t set_voltage = 0;  // Needed for dac.setVoltage(uint,false)
volatile boolean dc_enable = false;
volatile boolean ms_enable = false;



//===== MCP4725 =====
Adafruit_MCP4725 dac;

void MCP4725_setVoltage() {
  dac.setVoltage(set_voltage, false);
}
/*uint32_t set_voltage = (int) vltge * 4095.0 / 4.72;
  if ((0 <= set_voltage) && (set_voltage <= 4095)) {
  dac.setVoltage(set_voltage, false);
  }*/

//===== Delay function =====
int dly_func(long dly) {
  if (dly <= 16383) {
    delayMicroseconds(dly);
  }
  else {
    long new_dly = (long) dly / 1000;
    delay(new_dly);
  }
}


// ========== SerialIPConfigure ==========
void SerialIPConfigure() {
  String serialstring = Serial.readString(); // read the incoming byte:
  serialstring.trim(); //removes leading and trailing whitespace
  Serial.println(serialstring); // write the read String

  if (serialstring.equals("help")) {
    Serial.println("--- Serial connection Help ---");
    Serial.println("ip?                 : get current ip address and configuration");
    Serial.println("set ip 10.32.200.44 : set static ip address");
    Serial.println("ip: DHCP            : set ip to DHCP");
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

    serialstring.toCharArray(achar, sizeof(achar)); // Needed to use sscanf with String a
    //Error!//sscanf(achar, "set ip %3d.%3d.%3d.%3d", &ip_eeprom.ip_addr[0], &ip_eeprom.ip_addr[1], &ip_eeprom.ip_addr[3], &ip_eeprom.ip_addr[4]); // Write new ip to ip_eeprom //Leads to error!
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


void evaluateClient() {
  //==== Ethernet ====
  client = server.available();
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

// ========== readTelnetCommand ==========
void readTelnetCommand(char c, EthernetClient &client) {
  if (cmdstr.length() == MAX_CMD_LENGTH) {
    cmdstr = "";
  }
  if (c == '\n')
    parseCommand(client);
  else
    cmdstr += c;
}

// ========== parseCommand ==========
void parseCommand(EthernetClient &client) {
  //===== QUIT =====
  if (cmdstr.equals("quit")) {
    client.stop();
    connected = false;
  }

  //===== HELP =====
  else if (cmdstr.equals("help")) {
    client.print("--- LASER Diode Controller Version "); client.print(version, 1); client.println(" ---");
    client.println("tled<?> <0|1>");
    client.println("a<?> <0-4095>");
    client.println("p<int|?> - Pulse length in us");
    client.println("t<int|?> - Distance between falling and rising flank in us");
    client.println("dc<?> <on|off>");
    client.println("mu <0|1|?>");
  }

  //===== Test LED ON/OFF =====
  else if (cmdstr.equals("tled 1")) {
    digitalWrite(testled, HIGH);
  }
  else if (cmdstr.equals("tled 0")) {
    digitalWrite(testled, LOW);
  }
  //===== Read Test-LED state on/off =====
  else if (cmdstr.equals("tled?")) {
    client.print("tled: ");
    client.println(digitalRead(testled));
  }

  //===== Set output voltage =====
  else if (cmdstr.startsWith("a ")) {
    uint32_t readvoltage = cmdstr.substring(2).toInt();
    if ((0 <= readvoltage) && (readvoltage <= 4095)) {
      set_voltage = readvoltage;
    }

  }

  //===== Read current set-volt parameter =====
  else if (cmdstr.equals("a?")) {
    client.print("a:");
    client.println(set_voltage);
  }


  //===== Set pulse length =====
  else if (cmdstr.startsWith("p ")) {
    value = cmdstr.substring(1).toInt();
    if (0 <= value && value <= 2000000) {
      Tpulselength = value;
    }
  }

  //===== Read pulse length =====
  else if (cmdstr.equals("p?")) {
    // Measurements show actual emitted pulse length ~ Tpulselength + 160 us;
    client.print("Pulse length in mus: "); client.println(Tpulselength + 160);
  }


  //===== Time between two shots =====
  else if (cmdstr.startsWith("t ")) {
    value = cmdstr.substring(1).toInt();
    if (0 <= value && value <= 2000000) {
      Tshot = value;
    }
  }

  //===== Read cycle time =====
  else if (cmdstr.equals("t?")) {
    client.print("Cycle time in mus: ");
    client.println(Tshot);
  }

  //===== DC on =====
  else if (cmdstr.equals("dc on")) {
        ms_enable = false;
        dc_enable = true;
  }

  else if (cmdstr.equals("dc off")) {
      dc_enable = false;
      ms_enable = false;
  }


  //===== Read DC state =====
  else if (cmdstr.equals("dc?")) {
    client.print("dc: ");
    client.println(dc_enable);
  }


  //===== Multiple shots =====
  else if (cmdstr.equals("mu 1")) {
    dc_enable = false;
    ms_enable = true;
  }
  
  else if (cmdstr.equals("mu 0")) {
    dc_enable = false;
    ms_enable = false;
  }

  //===== Read ms_enable =====
  else if (cmdstr.equals("mu?")) {
    client.print("mu: ");
    client.println(ms_enable);
  }

  //===== Invalid Command, HELP =====
  else {
    client.println("Invalid command, type help");
  }

  cmdstr = "";
}

void setup() {
  //===== Serial =====
  Serial.begin(9600); // Open serial communications and wait for port to open

  //===== Ethernet =====
  Ethernet.begin(mac); // start the Ethernet connection and the server using DHCP
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

  //===== Default Value setup =====

  //===== DAC setup MCP4725 =====
  dac.begin(0x62);

  //Timer to evaluate ethernet communication
  Timer1.initialize(2000);
  Timer1.attachInterrupt(&evaluateClient);

}

//========== LOOP ==========
void loop() {
  if (Serial.available() > 0) {
    SerialIPConfigure();
  }
  if (dc_enable && !ms_enable) {
    dac.setVoltage(set_voltage, false);
  }
  if (ms_enable && !dc_enable) {
    dac.setVoltage(set_voltage, false);
    dly_func(Tpulselength);
    dac.setVoltage(0, false);
    dly_func(Tshot);
  }
  if (!dc_enable && !ms_enable) {
    dac.setVoltage(0, false);
  }
}







