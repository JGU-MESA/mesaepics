/*  Laser controller script
    Originally by S.Heidrich
    Edited by S.Friederich


    26.06.2018: Trigger signal before shot
    08.05.2018: default value of handshake (variable Thandshake) changed to 200,
    Because single shot did not work properly

*/
const float version = 1.5;
//========== Needed Packages ==========
#include <EEPROM.h>
#include <SPI.h>
#include <Ethernet2.h> //Maybe this package needs to be changed
#define MAX_CMD_LENGTH   25
#include <Wire.h>
#include <digitalWriteFast.h>

//========== Timer==========
#include <TimerOne.h>
#include <TimerThree.h>
unsigned long tnow;
// Prepare Shot
void prepare_shot(boolean high = false);
// Oneshot
volatile boolean os_enable_high = false;
volatile boolean os_enable_low = false;
void oneshot();

// Multishot
volatile boolean ms_enable = false;
long nshots = 10;
long ms_counter = 1;
void multishot();

//========== Trigger ===============
const byte p_trig = 4; // Send trigger as 5V signal from this pin
boolean trig_enable = false;
long trigger_width = 1000; // Trigger pulse width in us
long trigger_delay = 1800000; // Time between trigger falling flank and shot in us

//========== Laser ===============
int wert;
int binwert;
int printwert = 0;
int dataPin = 7;
int shiftPin = 8;
int storePin = 9;
int HandshakePin = 12;
int AmpPin = 11;
int PLPin = 13;
uint32_t Thandshake = 200; // Handshake
uint32_t Tpulselength = 100; // pulse length
uint32_t Nshot = 5; // Multishot: Number of shots
uint32_t Tshot = 200; // Multishot: Time between falling and rising flank
uint32_t amplitude = 1; // Amplitude

uint32_t value = 0;
const byte interruptPin = 2;
volatile byte TestVariable = 0;
boolean connected = false;
boolean on_state = false;
boolean dc_state = false;
//========== Ethernet ===============
EthernetServer server(23);
EthernetClient client;
String cmdstr;

//========== Static/DHCP ===============
byte mac[] = {0x90, 0xA2, 0xDA, 0x10, 0x15, 0x89}; // Enter a MAC address

struct IPObject {
  boolean dhcp_state; //static IP (false) or DHCP (true)
  int ip_addr[4]; //IP address
};

IPObject ip_eeprom;
int eeAddress = 0; // Defined address for EEPROM

//===== Delay function =====
int dly_func(long dly) {
  if (dly <= 16383) {
    delayMicroseconds(dly);
  }
  else {
    int new_dly = (long) dly / 1000;
    delay(new_dly);
  }
}

//===== Trigger =====
void trigger() {
  // Trigger signal
  digitalWrite(p_trig, HIGH);
  dly_func(trigger_width);
  digitalWrite(p_trig, LOW);

  // Delay between trigger signal and shot
  dly_func(abs(trigger_delay - trigger_width));
}


//========== SETUP ==========
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
  pinMode(interruptPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(interruptPin), confirm, FALLING);
  //Trigger
  pinMode(p_trig, OUTPUT);
  digitalWrite(p_trig, LOW);

  //Timer for oneshot
  Timer1.initialize(Tpulselength);
  Timer1.stop();
  Timer1.attachInterrupt(oneshot);

  //Timer for multishot
  Timer3.initialize(Tpulselength + Tshot);
  Timer3.stop();
  Timer3.attachInterrupt(multishot);

}

//========== LOOP ==========
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

  //Debug
  //Serial.print("cmdstr = ");
  //Serial.println(cmdstr);

  //===== QUIT =====
  if (cmdstr.equals("quit")) {
    client.stop();
    connected = false;
  }

  //===== HELP =====
  else if (cmdstr.equals("help")) {
    client.print("--- LASER Controller Version "); client.print(version, 1); client.println(" ---");
//        client.print("--- LASER Controller Version "); client.print(version, 1); client.println(" ---");
//        client.println("OUTP {1|0} = laser on|off");
//        client.println("OUTP? = read on-|off-state");
//        client.println("Only values between 0 and 65536 are permitted!");
//        client.println("p<?> <value>= Set<Read> pulse length");
//        client.println("a<?> <value>= Set<Read> amplitude");
//        client.println("n<?> <value>= Set<Read> number of shots in multiple shot mode");
//        client.println("t<?> <value>= Set<Read> time in microseconds between two pulses in multiple shot mode");
//        client.println("x<?> <value>= Set<Read> length for handshake (no change suggested)");
//        client.println("dc<?> {off|on} = Read|Stop|Start DC mode");
//        client.println("trig<?> {on|off} = Read|Enable|Disable trigger");
//        client.println("t_w|d <value> = Change trigger pulse width|delay");
//        client.println("sh = single shot (pew)");
//        client.println("mu on/off = multiple shots (pew,pew,pew)");
  }

  //===== IP =====
  else if (cmdstr.equals("ip?")) {
    client.println(Ethernet.localIP());
  }

  //===== ON/OFF =====
  else if (cmdstr.equals("outp 1")) {
    digitalWrite(5, LOW);
    delay(10);
    digitalWrite(5, HIGH);
    on_state = true;
  }
  else if (cmdstr.equals("outp 0")) {
    digitalWrite(6, LOW);
    delay(10);
    digitalWrite(6, HIGH);
    on_state = false;
    dc_state = false;
    ms_enable = false;
  }

  //===== Read ON-OFF-State =====
  else if (cmdstr.equals("outp?")) {
    client.print("outp: ");
    client.println(on_state);
  }


  //==================== LASER PARAMETER ====================
  //===== Set set-delay (handshake) =====
  else if (cmdstr.startsWith("x ")) {
    value = cmdstr.substring(1).toInt();
    if (value >= 0 && value < 65536) {
      Thandshake = value;
    }
  }

  //===== Read set-delay =====
  else if (cmdstr.equals("x?")) {
    client.print("Set-delay in mus: ");
    client.println(Thandshake);
  }


  //===== Set pulse length =====
  else if (cmdstr.startsWith("p ")) {
    value = cmdstr.substring(1).toInt();
    /*
       https://playground.arduino.cc/Code/Timer1
       maximum TimerOne period = 8388480us ~ 8.3s
    */
    if (value >= 10 && value < 8388479) {
      Tpulselength = value;
      Timer1.setPeriod(Tpulselength);
    }
  }

  //===== Read pulse length =====
  else if (cmdstr.equals("p?")) {
    client.print("Pulse length in mus: "); client.println(Tpulselength);
  }


  //===== Set amplitude =====
  else if (cmdstr.startsWith("a ")) {
    value = cmdstr.substring(1).toInt();
    if (value >= 0 && value < 65536) {
      amplitude = value;
    }
  }

  //===== Read amplitude =====
  else if (cmdstr.equals("a?")) {
    client.print("Amplitude: ");
    client.println(amplitude);
  }


  //===== Set cycle time =====
  else if (cmdstr.startsWith("t ")) {
    value = cmdstr.substring(1).toInt();
    if (value >= 10) {
      Tshot = value;
      Timer3.setPeriod(Tpulselength + Tshot);
    }
  }

  //===== Read cycle time =====
  else if (cmdstr.equals("t?")) {
    client.print("Cycle time in mus: ");
    client.println(Tshot);
  }


  //===== Set number of shots =====
  else if (cmdstr.startsWith("n ")) {
    value = cmdstr.substring(1).toInt();
    if (value >= 2) {
      Nshot = value;
    }
  }

  //===== Read number of shots =====
  else if (cmdstr.equals("n?")) {
    client.print("Number of shots: ");
    client.println(Nshot);
  }

  //===== TRIGGER =====
  else if (cmdstr.equals("trig?")) {
    client.print("Trigger: ");
    client.println(trig_enable);
  }

  else if (cmdstr.equals("trig on")) {
    trig_enable = true;
  }

  else if (cmdstr.equals("trig off")) {
    trig_enable = false;
  }

  else if (cmdstr.startsWith("tw ")) {
    value = cmdstr.substring(2).toInt();
    if (value >= 10 && value <= 10000000) trigger_width = value;
  }

  else if (cmdstr.equals("tw?")) {
    client.print("trigger_width: "); client.println(trigger_width);
  }

  else if (cmdstr.startsWith("td ")) {
    value = cmdstr.substring(2).toInt();
    if (value >= 10 && value <= 10000000) trigger_delay = value;
  }

  else if (cmdstr.equals("td?")) {
    client.print("trigger_delay: "); client.println(trigger_delay);
  }

  //==================== LASER OUTPUT ====================
  //===== DC on =====
  else if (cmdstr.equals("dc on")) {
    if (on_state == true) {
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
      dc_state = true;
      while (TestVariable == 0);
      TestVariable = 1;
    }
  }

  //===== DC off =====
  else if (cmdstr.equals("dc off")) {
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
    dc_state = false;
    while (TestVariable == 0);
    TestVariable = 1;
  }


  //===== Read DC state =====
  else if (cmdstr.equals("dc?")) {
    client.print("dc: ");
    client.println(dc_state);
  }

  //===== Single shot =====
  else if (cmdstr.equals("sh")) {
    if (on_state == true) {
      client.println("pew");
      prepare_shot(true);
      os_enable_high = true;
      if (trig_enable) trigger();
      Timer1.start();
    }
  }

  //===== Multiple shots =====
  else if (cmdstr.equals("mu on")) {
    if (on_state == true) {
      client.println("pew pew pew");
      prepare_shot(true);
      ms_enable = true;
      if (trig_enable) trigger();
      Timer3.start();
    }
  }

  else if (cmdstr.equals("mu off")) {
    ms_enable = false;
    Timer3.stop();
    prepare_shot();
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

//========== Shot-FUNCTION ==========
void prepare_shot(boolean high = false) {
  for (int n = 0; n < 16; n++) {
    digitalWriteFast(shiftPin, LOW);
    if (high) digitalWrite(dataPin, bitRead(amplitude, n) == 1 ? HIGH : LOW);
    else digitalWriteFast(dataPin, LOW);
    digitalWriteFast(shiftPin, HIGH);
  }
  digitalWriteFast(storePin, HIGH);
  digitalWriteFast(storePin, LOW);
  digitalWriteFast(HandshakePin, LOW);// Laser shoots if HandshakePing LOW->HIGH
  delay(1);
}


//========== ONESHOT ==========
void oneshot() {
  noInterrupts();
  if (os_enable_low) {
    //Serial.print("Timer1 off:");Serial.println(micros()-tnow);
    //digitalWriteFast(p_trig, LOW);
    digitalWriteFast(HandshakePin, LOW);// Laser shoots if HandshakePing LOW->HIGH
    if (ms_enable) ms_counter += 1;
    os_enable_low = false;
    Timer1.stop();
    if (!ms_enable) prepare_shot();
  }
  if (os_enable_high) {
    //digitalWriteFast(p_trig, HIGH);
    digitalWriteFast(HandshakePin, HIGH);// Laser shoots if HandshakePing LOW->HIGH
    os_enable_high = false;
    os_enable_low = true;
  }
  interrupts();
}

//========== MULTISHOT ==========
void multishot() {
  noInterrupts();
  if (ms_enable && ms_counter <= Nshot &&
      !os_enable_high && !os_enable_low) {
    os_enable_high = true;
    Timer1.start(); // Oneshot
    if (ms_counter == Nshot) {
      ms_enable = false;
      ms_counter = 1;
      Timer3.stop();
      prepare_shot();
    }
  }
  interrupts();
}






/* This worked
   void oneshot_test() {
  noInterrupts();
  if (os_enable_low) {
    //Serial.print("Timer1 off:");Serial.println(micros()-tnow);
    digitalWrite(p_trig, LOW);
    if (ms_enable) ms_counter += 1;
    os_enable_low = false;
    Timer1.stop();
  }
  if (os_enable_high) {
    //tnow = micros();
    //Serial.print("Timer1 on: ");Serial.println(tnow);
    //Serial.println("pew");
    digitalWrite(p_trig, HIGH);
    os_enable_high = false;
    os_enable_low = true;
  }
  interrupts();
  }
*/
