//  Laser controller script
//  Originally by S.Heidrich
//  Edited by S.Friederich (IP configuration part)
//  latest update: 28.07.2017
const float version = 1.2;
//========== Needed Packages ==========
#include <EEPROM.h>
#include <SPI.h>
#include <Ethernet2.h> //Maybe this package needs to be changed
#define MAX_CMD_LENGTH   25
#include <Wire.h>
#include <digitalWriteFast.h>


int wert;
int binwert;
int printwert = 0;
int dataPin = 7;
int shiftPin = 8;
int storePin = 9;
int HandshakePin = 12;
int AmpPin = 11;
int PLPin = 13;
uint32_t D1 = 150;
uint32_t D2 = 100;
uint32_t Nshot = 1;
uint32_t Tshot = 200;
String cmdstr;
uint32_t amplitude = 1;
uint32_t value = 0;
uint32_t value0 = 0;
const byte interruptPin = 2;
volatile byte TestVariable = 0;
boolean connected = false;
boolean on_state = false;
//========== Ethernet ===============
EthernetServer server(23);
EthernetClient client;

//========== Static/DHCP ===============
byte mac[] = {0x90, 0xA2, 0xDA, 0x10, 0x15, 0x89}; // Enter a MAC address

struct IPObject {
  boolean dhcp_state; //static IP (false) or DHCP (true)
  int ip_addr[4]; //IP address
};

IPObject ip_eeprom;
int eeAddress = 0; // Defined address for EEPROM


//==============================

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


  //Shift-register:
  pinMode(shiftPin, OUTPUT); //Shift
  pinMode(storePin, OUTPUT); //Store
  pinMode(dataPin, OUTPUT); //Write
  //Laser:
  pinMode(5, OUTPUT); //On
  pinMode(6, OUTPUT); //Off
  pinMode(HandshakePin, OUTPUT); //Set DC amplitude
  pinMode(AmpPin, OUTPUT); //Set pulse amplitude
  pinMode(PLPin, OUTPUT); //Set pulse length
  digitalWrite(5, HIGH);
  digitalWrite(6, LOW); //Send off-command to laser at arduino-reset
  delay(10);
  digitalWrite(6, HIGH);
  digitalWrite(HandshakePin, HIGH);
  digitalWrite(AmpPin, HIGH);
  digitalWrite(PLPin, HIGH);
  //Confirmation (optional):
  //pinMode(interruptPin, INPUT_PULLUP);
  //attachInterrupt(digitalPinToInterrupt(interruptPin), confirm, FALLING);
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
      value0 = strtoul(cmdstr.c_str(), NULL, 10);
      if (value0 > 0 && value0 < 65536) {
        value = value0;
      }
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

void confirm() {
  TestVariable = 1;
}

void readTelnetCommand(char c, EthernetClient &client) {
  if (cmdstr.length() == MAX_CMD_LENGTH) {
    cmdstr = "";
  }
  if (c == '\n')
    parseCommand(client);
  else
    cmdstr += c;
}

void parseCommand(EthernetClient &client) {

  Serial.print("cmdstr = ");
  Serial.println(cmdstr);

  //===== QUIT =====
  if (cmdstr.equals("quit")) {
    client.stop();
    connected = false;
  }

  //===== HELP =====
  else if (cmdstr.equals("help")) {
    client.print("--- LASER Controller Version "); client.print(version, 1); client.println(" ---");
    client.println("on = value on");
    client.println("off = value off");
    client.println("state = read on-|off-state");
    client.println("Value between 0 and 65536 = value request");
    client.println("p<?> = Set<Read> pulse length");
    client.println("a<?> = Set<Read> amplitude");
    client.println("n<?> = Set<Read> number of shots in multiple shot mode");
    client.println("t<?> = Set<Read> time in microseconds between two pulses in multiple shot mode");
    client.println("x<?> = Set<Read> length for handshake (no change suggested)");
    client.println("dc {off|on} = Stop|Start DC mode");
    client.println("sh = Single shot (pew)");
    client.println("mu = multiple shots (pew,pew,pew)");
    client.println("list = a list of current values");
    client.println("-----");
  }

  //===== IP =====
  else if (cmdstr.equals("ip?")) {
    client.println(Ethernet.localIP());
    client.println("-----");
  }

  //===== ON =====
  else if (cmdstr.equals("on")) {
    digitalWrite(5, LOW);
    delay(10);
    digitalWrite(5, HIGH);
    on_state = true;
    client.println("power on");
    client.println("-----");
  }

  //===== OFF =====
  else if (cmdstr.equals("off")) {
    digitalWrite(6, LOW);
    delay(10);
    digitalWrite(6, HIGH);
    on_state = false;
    client.println("power off");
    client.println("-----");
  }

  //===== Read ON-OFF-State =====
  else if (cmdstr.equals("state?")) {
    if (on_state) client.println("power on");
    else if (!on_state) client.println("power off");
    client.println("-----");
  }

  //===== VALUE =====
  else if (value0 > 0) {
    client.print("Requested value = ");
    client.println(value);
    client.println("-----");
  }

  //===== Set set-delay (handshake) =====
  else if (cmdstr.equals("x")) {
    D1 = value;
    client.println("Set-delay is set to:");
    client.print(D1);
    client.println(" microseconds");
    client.println("-----");
  }

  //===== Read set-delay =====
  else if (cmdstr.equals("x?")) {
    client.println("Set-delay is set to:");
    client.print(D1);
    client.println(" microseconds");
    client.println("-----");
  }


  //===== Set pulse length =====
  else if (cmdstr.equals("p")) {
    D2 = value;
    client.println("Pulse length is set to:");
    client.print(D2);
    client.println(" microseconds");
    client.println("-----");
  }

  //===== Read pulse length =====
  else if (cmdstr.equals("p?")) {
    client.println("Pulse length is set to:");
    client.print(D2);
    client.println(" microseconds");
    client.println("-----");
  }

  //===== Set amplitude =====
  else if (cmdstr.equals("a")) {
    amplitude = value;
    client.println("Amplitude is set to:");
    client.println(amplitude);
    client.println("-----");
  }

  //===== Read amplitude =====
  else if (cmdstr.equals("a?")) {
    client.println("Amplitude is set to:");
    client.println(amplitude);
    client.println("-----");
  }

  //===== Set cycle time =====
  else if (cmdstr.equals("t")) {
    Tshot = value;
    client.println("Cycle time is set to:");
    client.print(Tshot);
    client.println(" microseconds");
    client.println("-----");
  }

  //===== Read cycle time =====
  else if (cmdstr.equals("t?")) {
    client.println("Cycle time is set to:");
    client.print(Tshot);
    client.println(" microseconds");
    client.println("-----");
  }

  //===== Set number of shots =====
  else if (cmdstr.equals("n")) {
    Nshot = value;
    client.println("Number of shots is set to:");
    client.println(Nshot);
    client.println("-----");
  }

  //===== Read number of shots =====
  else if (cmdstr.equals("n?")) {
    client.println("Number of shots is set to:");
    client.println(Nshot);
    client.println("-----");
  }

  //===== DC on =====
  else if (cmdstr.equals("dc on")) {
    client.println("Set request for dc on");
    client.println("-----");
    for (int n = 0; n < 16; n++) {
      digitalWrite(shiftPin, LOW);
      digitalWrite(dataPin, bitRead(amplitude, n) == 1 ? HIGH : LOW);
      digitalWrite(shiftPin, HIGH);
    }
    digitalWrite(storePin, HIGH);
    digitalWrite(storePin, LOW);
    digitalWrite(HandshakePin, LOW);
    delay(10);
    digitalWrite(HandshakePin, HIGH);
    //while (TestVariable == 0);
    //TestVariable = 1;
    client.println("Finished!");
    client.println("-----");
  }

  //===== DC off =====
  else if (cmdstr.equals("dc off")) {
    client.println("Set request for dc off");
    client.println("-----");
    for (int n = 0; n < 16; n++) {
      digitalWrite(shiftPin, LOW);
      digitalWrite(dataPin, LOW);
      digitalWrite(shiftPin, HIGH);
    }
    digitalWrite(storePin, HIGH);
    digitalWrite(storePin, LOW);
    digitalWrite(HandshakePin, LOW);
    delay(10);
    digitalWrite(HandshakePin, HIGH);
    //while (TestVariable == 0);
    //TestVariable = 1;
    client.println("Finished!");
    client.println("-----");
  }

  //===== Single shot =====
  else if (cmdstr.equals("sh")) {
    // message:
    client.println("Set request for single shot");
    client.println("-----");
    //Single shot:
    for (int n = 0; n < 16; n++) {
      digitalWriteFast(shiftPin, LOW);
      delayMicroseconds(1);
      digitalWrite(dataPin, bitRead(amplitude, n) == 1 ? HIGH : LOW);
      delayMicroseconds(1);
      digitalWriteFast(shiftPin, HIGH);
      delayMicroseconds(1);
    }
    digitalWriteFast(storePin, HIGH);
    delayMicroseconds(1);
    digitalWriteFast(storePin, LOW);
    delayMicroseconds(1);
    // Set-Pin:
    digitalWriteFast(HandshakePin, LOW);
    delayMicroseconds(D1);
    digitalWriteFast(HandshakePin, HIGH);
    // Pulse length:
    if (D2 < 16383) {
      delayMicroseconds(D2);
    };
    if (D2 >= 16383) {
      delay(D2 / 1000);
    };
    //Register is set to zero after single shot
    for (int n = 0; n < 16; n++) {
      digitalWriteFast(shiftPin, LOW);
      delayMicroseconds(1);
      digitalWriteFast(dataPin, LOW);
      delayMicroseconds(1);
      digitalWriteFast(shiftPin, HIGH);
      delayMicroseconds(1);
    }
    digitalWriteFast(storePin, HIGH);
    delayMicroseconds(1);
    digitalWriteFast(storePin, LOW);
    delayMicroseconds(1);
    // Set-Pin:
    digitalWriteFast(HandshakePin, LOW);
    delayMicroseconds(D1);
    digitalWriteFast(HandshakePin, HIGH);
    delayMicroseconds(1);
    //while (TestVariable == 0);//Handshake
    //TestVariable = 1;
    client.println("Shot was fired!");
    client.println("-----");
  }


  //===== Multiple shots =====
  else if (cmdstr.equals("mu")) {
    // message:
    client.println("Set request for multiple shots");
    /*client.print(Nshot);
      client.println(" shots");
      client.println("with distance ");
      client.print(Tshot);
      client.println(" microseconds");
      client.println("and amplitude ");
      client.print(amplitude);*/
    client.println("-----");
    // Multiple shots:
    for (int i = 0; i < Nshot; i++) {
      // Shot:
      for (int n = 0; n < 16; n++) {
        digitalWriteFast(shiftPin, LOW);
        delayMicroseconds(1);
        digitalWrite(dataPin, bitRead(amplitude, n) == 1 ? HIGH : LOW);
        delayMicroseconds(1);
        digitalWriteFast(shiftPin, HIGH);
        delayMicroseconds(1);
      }
      digitalWriteFast(storePin, HIGH);
      delayMicroseconds(1);
      digitalWriteFast(storePin, LOW);
      delayMicroseconds(1);
      // Set-Pin:
      digitalWriteFast(HandshakePin, LOW);
      delayMicroseconds(D1);
      digitalWriteFast(HandshakePin, HIGH);
      delayMicroseconds(1);
      // Pulse length:
      if (D2 < 16383) {
        delayMicroseconds(D2);
      };
      if (D2 >= 16383) {
        delay(D2 / 1000);
      };
      //Register is set to zero after single shot:
      for (int n = 0; n < 16; n++) {
        digitalWriteFast(shiftPin, LOW);
        delayMicroseconds(1);
        digitalWriteFast(dataPin, LOW);
        delayMicroseconds(1);
        digitalWriteFast(shiftPin, HIGH);
        delayMicroseconds(1);
      }
      digitalWriteFast(storePin, HIGH);
      delayMicroseconds(1);
      digitalWriteFast(storePin, LOW);
      delayMicroseconds(1);
      // Set-Pin:
      digitalWriteFast(HandshakePin, LOW);
      delayMicroseconds(D1);
      digitalWriteFast(HandshakePin, HIGH);
      // Distance between two positive flanks:
      if (Tshot < 16383) {
        delayMicroseconds(Tshot);
      };
      if (Tshot >= 16383) {
        delay(Tshot / 1000);
      };
    };
    client.println("Shots were fired!");
    client.println("-----");
  }
  /*
    //  //===== list all information =====
    //  else if (cmdstr.equals("list")) {
    //    client.print("amplitude: ");
    //    client.println(amplitude);
    //    client.print("pulse length: ");
    //    client.print(D2);
    //    client.println(" microseconds");
    //    client.print("Number of shots: ");
    //    client.println(Nshot);
    //    client.print("Cycle time: ");
    //    client.print(Tshot);
    //    client.println(" microseconds");
    //    client.print("Handshake length: ");
    //    client.print(D1);
    //    client.println(" microseconds");
    //    client.println("-----");
    //  }
  */

  //===== Invalid Command, HELP =====
  else {
    client.println("Invalid command, type help");
    client.println("-----");
  }

  cmdstr = "";
}




