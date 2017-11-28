#include <SPI.h>
#include <Ethernet2.h> //Maybe this package needs to be changed
#define MAX_CMD_LENGTH   25
#include <Wire.h>
#include <digitalWriteFast.h>

byte mac[] = {0x90, 0xA2, 0xDA, 0x10, 0x15, 0x89}; // Enter a MAC address
int wert;
int binwert;
int printwert = 0;
int dataPin = 7;
int shiftPin = 8;
int storePin = 9;
int DCPin = 12;
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
EthernetServer server(23);
EthernetClient client;

void setup() {
  //TCP:
  Serial.begin(9600);
  Ethernet.begin(mac);
  server.begin();
  //Address
  Serial.print("server is at ");
  Serial.println(Ethernet.localIP());
  //Shift-register:
  pinMode(shiftPin, OUTPUT); //Shift
  pinMode(storePin, OUTPUT); //Store
  pinMode(dataPin, OUTPUT); //Write
  //Laser:
  pinMode(5, OUTPUT); //On
  pinMode(6, OUTPUT); //Off
  pinMode(DCPin, OUTPUT); //Set DC amplitude
  pinMode(AmpPin, OUTPUT); //Set pulse amplitude
  pinMode(PLPin, OUTPUT); //Set pulse length
  digitalWrite(5, HIGH);
  digitalWrite(6, HIGH);
  digitalWrite(DCPin, HIGH);
  digitalWrite(AmpPin, HIGH);
  digitalWrite(PLPin, HIGH);
  //Confirmation:
  pinMode(interruptPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(interruptPin), confirm, FALLING);
}

void loop() {
  // Create a client connection
  EthernetClient client = server.available();
  if (Serial.available() > 0) {
    // read the incoming byte:
    String a = Serial.readString();
    a.trim();
    Serial.println(a);
    if (a.equals("ip?")) {
      Serial.print("server is at ");
      Serial.println(Ethernet.localIP());
    }
  }

  if (client) {
    if (!connected) {
      client.flush(); //Clear out the input buffer
      cmdstr = ""; //clear the commandString variable
      connected = true;
    }

    if (client.available() > 0) {
      char zeichen = client.read();
      value0 = strtoul(cmdstr.c_str(), NULL, 10);
      if (value0 > 0) {
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
    client.println("on = value on");
    client.println("off = value off");
    client.println("Value between 0 and 65536 = value request");
    client.println("p = Set pulse length");
    client.println("a = Set amplitude");
    client.println("n = Set number of shots in multiple shot mode");
    client.println("t = Set time in microseconds between two pulses in multiple shot mode");
    client.println("x = Set length for handshake (no change suggested)");
    client.println("dc = Start DC mode");
    client.println("sh = Single shot (pew)");
    client.println("mu = multiple shots (pew,pew,pew)");
    client.println("list = a list of current values");

    client.println("-----");
  }

  //===== IP =====
  else if (cmdstr.equals("ip?")) {
    Serial.println(Ethernet.localIP());
    client.println(Ethernet.localIP());
    client.println("-----");
  }

  //===== ON =====
  else if (cmdstr.equals("on")) {
    digitalWrite(5, LOW);
    delay(10);
    digitalWrite(5, HIGH);
    client.println("power on");
    client.println("-----");
  }

  //===== OFF =====
  else if (cmdstr.equals("off")) {
    digitalWrite(6, LOW);
    delay(10);
    digitalWrite(6, HIGH);
    client.println("power off");
    client.println("-----");
  }

  //===== VALUE =====
  else if (value0 > 0) {
    client.print("Requested value = ");
    client.println(value);
    client.println("-----");
  }

  //===== Set-delay =====
  else if (cmdstr.equals("x")) {
    D1 = value;
    client.println("Set-delay is set to:");
    client.print(value);
    client.println(" microseconds");
    client.println("-----");
  }

  //===== Pulse length =====
  else if (cmdstr.equals("p")) {
    D2 = value;
    client.println("Pulse length is set to:");
    client.print(value);
    client.println(" microseconds");
    client.println("-----");
  }

  //===== Amplitude =====
  else if (cmdstr.equals("a")) {
    amplitude = value;
    client.println("Amplitude is set to:");
    client.println(value);
    client.println("-----");
  }

  //===== Cycle time =====
  else if (cmdstr.equals("t")) {
    Tshot = value;
    client.println("Cycle time is set to:");
    client.print(value);
    client.println(" microseconds");
    client.println("-----");
  }

  //===== Number of shots =====
  else if (cmdstr.equals("n")) {
    Nshot = value;
    client.println("Number of shots is set to:");
    client.println(value);
    client.println("-----");
  }

  //===== DC =====
  else if (cmdstr.equals("dc")) {
    client.println("Set request for dc value:");
    client.println(amplitude);
    client.println("-----");
    for (int n = 0; n < 16; n++) {
      digitalWrite(shiftPin, LOW);
      digitalWrite(dataPin, bitRead(amplitude, n) == 1 ? HIGH : LOW);
      digitalWrite(shiftPin, HIGH);
    }
    digitalWrite(storePin, HIGH);
    digitalWrite(storePin, LOW);
    digitalWrite(DCPin, LOW);
    delay(10);
    digitalWrite(DCPin, HIGH);
    while (TestVariable == 0);
    TestVariable = 1;
    client.println("fertig!");
    client.println("-----");
  }

  //===== Single shot =====
  else if (cmdstr.equals("sh")) {
    // message:
    client.println("Set request for single shot with amplitude");
    client.println(amplitude);
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
    digitalWriteFast(DCPin, LOW);
    delayMicroseconds(D1);
    digitalWriteFast(DCPin, HIGH);
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
    digitalWriteFast(DCPin, LOW);
    delayMicroseconds(D1);
    digitalWriteFast(DCPin, HIGH);
    delayMicroseconds(1);
    //Handshake
    while (TestVariable == 0);
    TestVariable = 1;
    client.println("Shot was fired!");
    client.println("-----");
  }


  //===== Multiple shots =====
  else if (cmdstr.equals("mu")) {
    // message:
    client.println("Set request for ");
    client.print(Nshot);
    client.println(" shots");
    client.println("with distance ");
    client.print(Tshot);
    client.println(" microseconds");
    client.println("and amplitude ");
    client.print(amplitude);
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
      digitalWriteFast(DCPin, LOW);
      delayMicroseconds(D1);
      digitalWriteFast(DCPin, HIGH);
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
      digitalWriteFast(DCPin, LOW);
      delayMicroseconds(D1);
      digitalWriteFast(DCPin, HIGH);
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

  //===== Cycle time =====
  else if (cmdstr.equals("list")) {
    client.print("amplitude: ");
    client.println(amplitude);
    client.print("pulse length: ");
    client.print(D2);
    client.println(" microseconds");
    client.print("Number of shots: ");
    client.println(Nshot);
    client.print("Cycle time: ");
    client.print(Tshot);
    client.println(" microseconds");
    client.print("Handshake length: ");
    client.print(D1);
    client.println(" microseconds");

    client.println("-----");
  }


  //===== Invalid Command, HELP =====
  else {
    client.println("Invalid command, type help");
    client.println("-----");
  }

  cmdstr = "";
}




