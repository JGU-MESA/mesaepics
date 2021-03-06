/*  Laser controller script
    Originally by S.Heidrich
    Edited by S.Friederich

    12.02.2019: M. W. Bruker
      - Interrupt-based timing to get rid of timing jitter
      - minor optimisations
    
    01.07.2018:
    - Trigger implemented
    - Upgraded ethernet communication via TimerOne
    - Simplified loop() function
    - Multishot can now be aborted

    08.05.2018: default value of handshake (variable Thandshake) changed to 200,
    Because single shot did not work properly
*/
static const uint8_t versionMajor = 2;
static const uint8_t versionMinor = 1;
//========== Needed Packages ==========
#include <EEPROM.h>
#include <SPI.h>
#include <Ethernet2.h> //Maybe this package needs to be changed to <Ethernet.h>
//#include <Wire.h>
#include <digitalWriteFast.h>

// Shot functions
void writeLaserPower(uint16_t amp = 0);
void updateLaserPower();
void oneShot();
void singleShot();
void multiShot();

// Booelans singleShot
//volatile boolean os_enable = false;

// Booelans multiShot
volatile boolean ms_enable = false;
long ms_counter = 0;


//========== Trigger ===============
#define p_trig2 3 // high = pulse active
#define p_trig  4 // Send trigger as 5V signal from this pin
// Booelans trigger
boolean trig_enable = false;
volatile boolean trigger_shot_enable = false;

// Times trigger
long trigger_width = 1000; // Trigger pulse width in us
long trigger_delay = 2000000; // Time between trigger falling edge and shot in us

//========== Laser ===============
#define dataPin 7
#define shiftPin 8
#define storePin 9
#define HandshakePin 12
#define AmpPin 11
#define PLPin 13

// Shot parameter
uint16_t Thandshake = 200; // Handshake (200 should be optimal)
#define Thandshake_lower_limit 100
uint32_t Tpulselength = 100; // pulse length
volatile uint16_t pulseLengthH = 0;
volatile uint16_t pulseLengthHCounter = 0;
volatile uint16_t pulseLengthL = 400;
uint32_t nshots = 5; // multiShot: Number of shots
uint32_t Tshot = 200; // multiShot: Time between falling and rising edge
volatile uint16_t periodLengthH = 0;
volatile uint16_t periodLengthHCounter = 0;
volatile uint16_t periodLengthL = 400;
uint32_t amplitude = 1; // Amplitude
uint32_t value = 0;

// Miscellanous booleans
boolean connected = false;
boolean on_state = false;
boolean dc_state = false;

//========== Ethernet ===============
EthernetServer server(23);
EthernetClient client;
String cmdstr;
#define MAX_CMD_LENGTH   25
void readTelnetCommand(char c, EthernetClient &client);
void evaluateClient();

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
    long new_dly = (long) dly / 1000;
    delay(new_dly);
  }
}

//========== Trigger ==========
void trigger_shot() {
  if (trig_enable && trigger_shot_enable) {
    trigger_shot_enable = false;
    sendTriggerSignal();
    dly_func(trigger_delay);
    singleShot();
    if (ms_enable) dly_func(Tshot);
  }
}

void sendTriggerSignal() {
  digitalWriteFast(p_trig, HIGH);
  delayMicroseconds(trigger_width);
  digitalWriteFast(p_trig, LOW);
}

// ========== SerialIPConfigure ==========
void SerialIPConfigure() {
  String serialstring = Serial.readString(); // read the incoming byte:
  serialstring.trim(); //removes leading and trailing whitespace
  Serial.println(serialstring); // write the read String

  if (serialstring.equals("help")) {
    Serial.print("--- Serial connection Help --- Version "); Serial.print(versionMajor); Serial.print("."); Serial.println(versionMinor);
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
    client.print("--- LASER Controller Version "); client.print(versionMajor); client.print("."); client.print(versionMinor); client.println(" ---");
    client.println("outp {1|0} = laser on|off");
    client.println("outp? = read on-|off-state");
    client.println("p<?> <value>= Set<Read> pulse length");
    client.println("a<?> <value>= Set<Read> amplitude");
    client.println("n<?> <value>= Set<Read> number of shots in multiple shot mode");
    client.println("t<?> <value>= Set<Read> time in microseconds between two pulses in multiple shot mode");
    client.println("x<?> <value>= Set<Read> length for handshake (no change suggested)");
    client.println("dc<?> {off|on} = Read|Stop|Start DC mode");
    client.println("trig<?> {on|off} = Read|Enable|Disable trigger");
    client.println("t_w|d <value> = Change trigger pulse width|delay");
    client.println("sh = single shot");
    client.println("mu<?> {0|1} = multiple shots");
    client.println("trigger = Emit trigger signal");
  }

  //===== ON/OFF =====
  else if (cmdstr.equals("outp 1")) {
    digitalWriteFast(5, LOW);
    delay(100);
    digitalWriteFast(5, HIGH);
    on_state = true;
  }
  else if (cmdstr.equals("outp 0")) {
    digitalWriteFast(6, LOW);
    delay(100);
    digitalWriteFast(6, HIGH);
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
    if (value >= Thandshake_lower_limit && value <= 1000) {
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
    //Tpulselength > 500ms can be achieved on channel access
    if (value >= Thandshake_lower_limit && value <= 500000) {
      noInterrupts();
      Tpulselength = value;
      // calculate total number of clock pulses; 2 pulses per us @ 16 MHz
      pulseLengthH = value >> 15;
      pulseLengthL = ((uint16_t) value) << 1;
      interrupts();
    }
  }

  //===== Read pulse length =====
  else if (cmdstr.equals("p?")) {
    // Measurements show actual emitted pulse length ~ Tpulselength + 160 us;
    client.print("Pulse length in mus: "); client.println(Tpulselength + 160);
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


  //===== Time between two shots =====
  else if (cmdstr.startsWith("t ")) {
    value = cmdstr.substring(1).toInt();
    if (Thandshake_lower_limit <= value && value <= 500000) {
      Tshot = value;
      // calculate total number of clock pulses; 2 pulses per us @ 16 MHz
      periodLengthH = value >> 15;
      periodLengthL = ((uint16_t) value) << 1;
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
//    if (value >= 2) {
      nshots = value;
//    }
  }

  //===== Read number of shots =====
  else if (cmdstr.equals("n?")) {
    client.print("Number of shots: ");
    client.println(nshots);
  }

  //===== TRIGGER =====
  else if (cmdstr.equals("trig?")) {
    client.print("Trigger: ");
    client.println(trig_enable);
  }

  else if (cmdstr.equals("trig 1")) {
    trig_enable = true;
  }

  else if (cmdstr.equals("trig 0")) {
    trig_enable = false;
    trigger_shot_enable = false;
  }

  else if (cmdstr.startsWith("tw ")) {
    value = cmdstr.substring(2).toInt();
    if (value >= 20 && value <= 100000) trigger_width = value;
  }

  else if (cmdstr.equals("tw?")) {
    client.print("trigger_width: "); client.println(trigger_width);
  }

  else if (cmdstr.startsWith("td ")) {
    value = cmdstr.substring(2).toInt();
    if (value >= 20 && value <= 10000000) {
      trigger_delay = value;
    }
  }

  else if (cmdstr.equals("td?")) {
    client.print("trigger_delay: "); client.println(trigger_delay);
  }

  else if (cmdstr.equals("trigger")) {
    sendTriggerSignal();
  }

  //==================== LASER OUTPUT ====================
  //===== DC on =====
  else if (cmdstr.equals("dc on")) {
    if (on_state == true) {
      writeLaserPower(amplitude);
      updateLaserPower();
      dc_state = true;
    }
  }

  //===== DC off =====
  else if (cmdstr.equals("dc off")) {
    writeLaserPower(0);
    updateLaserPower();
    dc_state = false;
  }


  //===== Read DC state =====
  else if (cmdstr.equals("dc?")) {
    client.print("dc: ");
    client.println(dc_state);
  }

  //===== Single shot =====
  else if (cmdstr.equals("sh")) {
    if (on_state == true) {
      if (trig_enable)
        trigger_shot_enable = true;
      else
        singleShot();
    }
  }

  //===== Multiple shots =====
  else if (cmdstr.equals("mu 1")) {
    if (on_state == true) {
      noInterrupts();
      if (trig_enable) {
        trigger_shot_enable = true;
        ms_counter = 1;
      } else
        ms_counter = 0;
      ms_enable = true;
      singleShot();
      interrupts();
    }
  }

  else if (cmdstr.equals("mu 0")) {
    noInterrupts();
    ms_enable = false;
    ms_counter = 0;
    interrupts();
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


ISR(TIMER1_COMPA_vect)
{
  uint8_t done = 0;
  if (periodLengthH == 0)
    done = 1;
  else if (++periodLengthHCounter == periodLengthH) {
    if (periodLengthL == 0)
      done = 1;
    else {
      OCR1A = periodLengthL;
      TCNT1 = 0;
    }
  } else if (periodLengthHCounter > periodLengthH)
    done = 1;

  if (done) {
    TIMSK1 = 0;
    singleShot();
  }
}


ISR(TIMER3_COMPA_vect)
{
  uint8_t done = 0;
  if (pulseLengthH == 0)
    done = 1;
  else if (++pulseLengthHCounter == pulseLengthH) {
    if (pulseLengthL == 0)
      done = 1;
    else {
      OCR3A = pulseLengthL;
      TCNT3 = 0;
    }
  } else if (pulseLengthHCounter > pulseLengthH)
    done = 1;
  
  if (done) {
    TIMSK3 = 0;

    // end trigger pulse
    digitalWriteFast(p_trig2, LOW);

    updateLaserPower();

    if (ms_enable && (++ms_counter == nshots) && nshots) {
      ms_enable = false;
    }
    if (ms_enable && !trigger_shot_enable) {
      TCNT1 = 0;
      TIFR1 |= (1 << OCF1A);
      TIMSK1 = (1 << OCIE1A);
      periodLengthHCounter = 0;
      if (periodLengthH == 0)
        OCR1A = periodLengthL;
      else
        OCR1A = 0xFFFF;
    }
  }
}


//========== Shot-FUNCTION ==========
void writeLaserPower(uint16_t amp = 0) {
  for (uint8_t n = 0; n < 16; n++) {
    digitalWriteFast(shiftPin, LOW);
    digitalWrite(dataPin, ((uint8_t) amp) & 0x01);
    amp >>= 1;
    digitalWriteFast(shiftPin, HIGH);
  }
  digitalWriteFast(storePin, HIGH);
  digitalWriteFast(storePin, LOW);
}

void updateLaserPower() {
  digitalWriteFast(HandshakePin, LOW);
  delayMicroseconds(Thandshake);
  digitalWriteFast(HandshakePin, HIGH);
}


void singleShot() {
  noInterrupts();
  writeLaserPower(amplitude);
  updateLaserPower();
  
  // start trigger pulse
  digitalWriteFast(p_trig2, HIGH);

  // Reset timer and enable timer compare interrupt
  TCNT3 = 0;
  TIFR3 |= (1 << OCF3A);
  TIMSK3 = (1 << OCIE3A);
  pulseLengthHCounter = 0;
  if (pulseLengthH == 0)
    OCR3A = pulseLengthL;
  else
    OCR3A = 0xFFFF;

  writeLaserPower(0);
  interrupts();
}
/*
//========== multiShot ==========
void multiShot() {
  if (ms_enable && (ms_counter < nshots) && !trigger_shot_enable) {
    singleShot();
    ms_counter += 1;
    dly_func(Tshot);
  }
  if (ms_enable && ms_counter == nshots) {
    ms_enable = false;
  }
}
*/
//========== EthernetCommunication ==========
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
  digitalWriteFast(5, HIGH);
  digitalWriteFast(6, LOW); //Send off-command to laser at arduino-reset
  delay(10);
  digitalWriteFast(6, HIGH);

  digitalWriteFast(HandshakePin, HIGH);
  digitalWriteFast(AmpPin, HIGH);
  digitalWriteFast(PLPin, HIGH);


  //Trigger
  pinMode(p_trig, OUTPUT);
  digitalWriteFast(p_trig, LOW);

  // Trigger 2
  pinMode(p_trig2, OUTPUT);
  digitalWriteFast(p_trig2, LOW);

  // Timer 1: CTC, fclk/8 prescaler: 2 ticks per microsecond; used for periodic pulses
  TCCR1A = 0;
  TCCR1B = (1 << WGM12) | (1 << CS11);
  TIMSK1 = 0;
  // Timer 3: CTC, fclk/8 prescaler: 2 ticks per microsecond; used for pulse length
  TCCR3A = 0;
  TCCR3B = (1 << WGM32) | (1 << CS31);
  TIMSK3 = 0;
}


//========== LOOP ==========
void loop() {
  if (Serial.available() > 0) {
    SerialIPConfigure();
  }
  evaluateClient();
  
  trigger_shot();
//  oneShot();
//  multiShot();
}
