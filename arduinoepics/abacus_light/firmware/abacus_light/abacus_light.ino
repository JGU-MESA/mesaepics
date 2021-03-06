//Arduino based control units: LIGHT
//Latest update: 02.08.2017
//Latest improvement: Dimmer added
const float version = 1.1;
String name = "Abacus Light";
/*
    Arduino_TCP

    A simple TCP Server that allows to talk to the Arduino via Telnet.
    It can process the given commands and execute them.

    This script is written for the Arduino Leonardo ETH.

    //TELNET-Version

*/

//========== Needed Packages ==========
#include <EEPROM.h>
#include <SPI.h>
#include <Ethernet2.h> //Maybe this package needs to be changed
#include <MsTimer2.h> //Needed to start a timer

//========== Define Constants ==========
#define MAX_CMD_LENGTH 25
// Note: All pins are at startup by default in INPUT-Mode (besides PIN 13!!)

//==========Static/DHCP===============
byte mac[] = {0x90, 0xA2, 0xDA, 0x11, 0x20, 0xAE}; // Enter a MAC address

struct IPObject {
  boolean dhcp_state; //static IP (false) or DHCP (true)
  int ip_addr[4]; //IP address
};

IPObject ip_eeprom;
int eeAddress = 0; // Defined address for EEPROM

//========== Ethernet server port ==========
// Initialize the Ethernet server library with a port you want to use
EthernetServer server(23); //TELNET defaults to port 23


//========== Needed variables or constants ==========
boolean connected = false; // whether or not the client was connected previously
String cmdstr; // command string that will be evaluated when received via ethernet
const byte light[]  = { A0, A1, A2, A3, A4, A5, 7, 8}; // Define analog pins for channel on off
int ll = sizeof(light) / sizeof(light[0]);
const byte dimmer[]  = { 3, 5, 6, 9, 10, 11, 13}; // Define analog pins for dimming (only 7 are available for dimming)
int ld = sizeof(dimmer) / sizeof(dimmer[0]);
uint8_t dimmerstate[]  = { 255, 255, 255, 255, 255, 255, 255};


//===== Timer variables and constants =====
boolean allowchange = false; // whether or not the status of a light may be changed
const uint16_t timeperiod = 100; // timer to wait until light status may be changed in ms

void timer() {
  allowchange = true;
}

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

  //===== Define Input and Output pins =====
  for (int i = 0; i < ll; i++) {
    pinMode(light[i], OUTPUT); // set pin i to output (Pin 0 is for VCC)
    digitalWrite(light[i], HIGH); // set pin i to HIGH (inverse logic for relais)
  }
  for (int i = 0; i < ld; i++) {
    pinMode(dimmer[i], OUTPUT); // Sets analog pins to output
    analogWrite(dimmer[i], dimmerstate[i]); // Sets analog pins to HIGH = 5V
  }

  //==== Timer =====
  MsTimer2::set(timeperiod, timer); //
  MsTimer2::start();

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


void readTelnetCommand(char c, EthernetClient & client) {
  if (cmdstr.length() == MAX_CMD_LENGTH) {
    cmdstr = "";
  }

  if (c == '\n')
    parseCommand(client);
  else
    cmdstr += c;
  //====Debug====
  //Serial.print("cmdstr_read = ");
  //Serial.println(cmdstr);

}

void parseCommand(EthernetClient & client) {

  //====Debug====
  //Serial.print("cmdstr = ");
  //Serial.println(cmdstr);

  //===== QUIT =====
  if (cmdstr.equals("quit")) {
    client.stop();
    connected = false;
  }

  //===== HELP =====
  else if (cmdstr.equals("help")) {
    client.print("--- ");
    client.print(name);
    client.print(" Version "); client.print(version, 1); client.println(" ---");
    client.println("quit                          : close the connection");
    client.println("ch {0|1..|7} {off|on|?}       : switch or read channel {0|1..|7} {off|on}");
    client.println("all ch off                    : switch all channels off");
    client.println("dimm {0|1..|6} {0..255|?}     : dimm or read dimm channel {0|1..|6}");
    client.println();
    client.println("By S.Friederich");
  }


  //===== Set light on/off ======
  else if (cmdstr.startsWith("ch ")) {
    int ch = cmdstr.substring(3, 4).toInt(); // e.g. "ch 1 on"
    if (0 <= ch && ch < ll) {
      byte channelnumber = light[ch];
      if (cmdstr.endsWith("on")) {
        if (allowchange == true && digitalRead(channelnumber) == HIGH) {
          digitalWrite(channelnumber, LOW); // Inverse logic (on=LOW)
          allowchange = false;
          MsTimer2::start();
        }
      }
      else if (cmdstr.endsWith("off")) {
        if (allowchange == true && digitalRead(channelnumber) == LOW) {
          digitalWrite(channelnumber, HIGH); // Inverse logic (on=LOW)
          allowchange = false;
          MsTimer2::start();
        }
      }
      //===== Read set channel state =====
      else if (cmdstr.endsWith("?")) {
        boolean channelstate = !digitalRead(channelnumber); // Inverse logic (1=on=LOW)
        client.print("current set state: ch "); client.print(ch);
        client.print(" "); client.println(channelstate);
      }
      else {
        client.println("Command invalid");
      }
    }
    else {
      client.println("Channel out of range!");
    }

  }

  //===== Set all channel off =====
  else if (cmdstr.equals("all ch off")) {
    if (allowchange == true) {
      for (int i = 0; i < ll; i++) {
        digitalWrite(light[i], HIGH); // set pin i to HIGH (inverse logic for relais)
      }
    }
    allowchange = false;
    MsTimer2::start();
  }

  //===== Dimmer =====
  else if (cmdstr.startsWith("dimm ")) {
    int ch = cmdstr.substring(5, 6).toInt(); // e.g. "dimm 1 255"; dimmer index starts with 0
    if (0 <= ch && ch < ld) {
      byte channelnumber = dimmer[ch];
      if (cmdstr.endsWith("?")) {
        client.print("current dimmer state: ch "); client.print(ch);
        client.print(" "); client.println(dimmerstate[ch]);
      }
      else {
        int dimmvalue = cmdstr.substring(7).toInt();
        if (0 <= dimmvalue && dimmvalue <= 255) {
          dimmerstate[ch] = dimmvalue; // Save new set dimmer state for readout
          analogWrite(channelnumber, dimmvalue); // set pin i to HIGH (inverse logic for relais)
        }
        else {
          client.println("Dimm value out of range");
        }
      }

    }
    else {
      client.println("Wrong dimmer channel selected");
    }
  }


  //===== Invalid Command, HELP =====
  else {
    client.println("Invalid command, type help");
  }

  cmdstr = "";
}

