#include "Ethernet.h"
#include "sACN.h"
#include "DmxOutput.h"

// core 0 is responsible for DMX output
// core 1 is responsible for sACN input

// CHANGE HERE
uint8_t port1Uni = 3;
uint8_t port2Uni = 3;
uint8_t port3Uni = 3;
uint8_t port4Uni = 3;

// leds
#define RED 0
#define GREEN 1

// mac address
byte mac[] = {0x00, 0xC0, 0x16, 0xB7, 0xA3, 0xD1};

// DHCP or not
boolean DHCP = false;

// DEBUG mode
#define DEBUG false

// blink delay
uint8_t blinkInterval = 1000;

// static IP data
IPAddress IP(10, 101, 0, 15);
IPAddress netmask(255, 255, 0, 0);
IPAddress gateway(10, 101, 0, 1);
IPAddress dns(10, 101, 0, 1);

// four things for each port
// the actual Pico-DMX object, a UDP socket object, a buffer array, and a sACN object
// could this be a struct? maybe. Do i care? no.

// port 1
DmxOutput port1;
EthernetUDP sacn1;
uint8_t dmxBufferPort1[513];
Receiver recvPort1(sacn1, port1Uni);

// port 2
DmxOutput port2;
EthernetUDP sacn2;
uint8_t dmxBufferPort2[513];
Receiver recvPort2(sacn2, port2Uni);

// port 3
DmxOutput port3;
EthernetUDP sacn3;
uint8_t dmxBufferPort3[513];
Receiver recvPort3(sacn3, port3Uni);

// port 4
DmxOutput port4;
EthernetUDP sacn4;
uint8_t dmxBufferPort4[513];
Receiver recvPort4(sacn4, port4Uni);


// blink the green led
uint64_t lastGreen = 0;
boolean greenState = LOW;
void greenBlink(){
  // verbaitum from "blink without delay" example
  uint64_t currentGreen = millis();

  if(currentGreen - lastGreen >= blinkInterval){
    lastGreen = currentGreen;
    greenState = !greenState;
    digitalWrite(GREEN, greenState);   
  }
}

// blink the red led
uint64_t lastRed = 0;
boolean redState = LOW;
void redBlink(){
  // verbaitum from "blink without delay" example
  uint64_t currentRed = millis();

  if(currentRed - lastRed >= blinkInterval){
    lastRed = currentRed;
    redState = !redState;
    digitalWrite(RED, redState);   
  }
}


// force reset the ethernet chip (needed for when on a switched network)
void hardreset(uint8_t pinRST) {
  pinMode(pinRST, OUTPUT);
  digitalWrite(pinRST, HIGH);
  digitalWrite(pinRST, LOW);
  delay(1);
  digitalWrite(pinRST, HIGH);
  delay(150);
}

// callback method when new sACN packet is received
// write data into buffer, put debug info out on serial
void dmxReceivedPort1() {
  if (DEBUG) {
    Serial.println("New DMX data received for Port 1");
  }

  // copy to dmx buffer
  // note that Pico-DMX expects a 513 byte buffer, including the 0 for the start bit
  // sACN library only outputs the 512 data bytes (logically)
  // memcpy to save cycles, uses more ram, but we have plenty
  dmxBufferPort1[0] = 0;

  // bc we're doing multiple ports on one uni, this is a bodge
  dmxBufferPort2[0] = 0;
  dmxBufferPort3[0] = 0;
  dmxBufferPort4[0] = 0;
  
  memcpy(dmxBufferPort1 + sizeof(uint8_t), recvPort1.dmx(), 512);
  memcpy(dmxBufferPort2 + sizeof(uint8_t), recvPort1.dmx(), 512);
  memcpy(dmxBufferPort3 + sizeof(uint8_t), recvPort1.dmx(), 512);
  memcpy(dmxBufferPort4 + sizeof(uint8_t), recvPort1.dmx(), 512);
}

// need one for each receiver
// inefficent in regards to code size, but we have plenty of flash
void dmxReceivedPort2() {
  if (DEBUG) {
    Serial.println("New DMX data received for Port 2");
  }
  // reasoning explained above
  dmxBufferPort2[0] = 0;
  memcpy(dmxBufferPort2 + sizeof(uint8_t), recvPort2.dmx(), 512);
}

void dmxReceivedPort3() {
  if (DEBUG) {
    Serial.println("New DMX data received for Port 3");
  }
  // reasoning explained above
  dmxBufferPort3[0] = 0;
  memcpy(dmxBufferPort3 + sizeof(uint8_t), recvPort3.dmx(), 512);
}

void dmxReceivedPort4() {
  if (DEBUG) {
    Serial.println("New DMX data received for Port 4");
  }
  // reasoning explained above
  dmxBufferPort4[0] = 0;
  memcpy(dmxBufferPort4 + sizeof(uint8_t), recvPort4.dmx(), 512);
}


// set all ports to 0 if we have no signal
void timeOut() {
  if (DEBUG) {
    Serial.println("Timeout!");
  }
  uint8_t allZeros[513];
  for (int i = 0; i < 513; i++) {
    allZeros[i] = 0;
  }
  memcpy(dmxBufferPort1, allZeros, 513);
  memcpy(dmxBufferPort2, allZeros, 513);
  memcpy(dmxBufferPort3, allZeros, 513);
  memcpy(dmxBufferPort4, allZeros, 513);
}

//-------------------------------------------------------------
// debug function
void newSource() {
  Serial.print("new source name: ");
  Serial.println(recvPort1.name());
}

//--------------------------------------------------------------------------------------------
// core 0 setup
void setup() {
  // set onboard & status leds to output
  pinMode(25, OUTPUT);

  pinMode(GREEN, OUTPUT);
  pinMode(RED, OUTPUT);

  // init serial if we're in debug mode
  if (DEBUG) {
    Serial.begin(115200);
    while (!Serial) {}
  }

  // initialize the dmx outputs
  boolean dmxFailed = false;
  dmxFailed = port1.begin(2, pio0);
  dmxFailed = port2.begin(3, pio0);
  dmxFailed = port3.begin(4, pio0);
  dmxFailed = port4.begin(5, pio0);

  // debug
  if (!dmxFailed && DEBUG) {
    Serial.println("dmx output setup successfully");
  }
}

//----------------------------------------------------------------------------------------------------------------------------------
// core 1 setup
void setup1() {
  // wait for core 0 to configure serial if we're in debug mode
  if (DEBUG) {
    while (!Serial) {}
  }


  // init ethernet
  hardreset(20);
  Ethernet.init(17);


  // configure IP
  if (DHCP) {
    // if it fails to configure dhcp 3 times, fallback to static
    boolean dhcpSuccess = Ethernet.begin(mac);
    uint8_t failedTimes = 0;
    while (!dhcpSuccess && failedTimes < 3) {
      digitalWrite(RED, HIGH);
      failedTimes++;
      dhcpSuccess = Ethernet.begin(mac);
    }
    if (!dhcpSuccess) {
      if (DEBUG) {
        Serial.println("DHCP Failed. Falling back to Static IP");
      }
      Ethernet.begin(mac, IP, dns, gateway, netmask);
      DHCP = false;
    }
    else {
      if (DEBUG) {
        Serial.println("DHCP Configured successfully.");
      }
    }
  }

  else {
    Ethernet.begin(mac, IP, dns, gateway, netmask);
  }

  if (DEBUG) {
    Serial.println(Ethernet.localIP());
  }

  // set callback functions for sACN receivers
  recvPort1.callbackDMX(dmxReceivedPort1);
  //recvPort2.callbackDMX(dmxReceivedPort2);
  //recvPort3.callbackDMX(dmxReceivedPort3);
  //recvPort4.callbackDMX(dmxReceivedPort4);

  // timeout callback
  recvPort1.callbackTimeout(timeOut);
  //recvPort2.callbackTimeout(timeOut);
  //recvPort3.callbackTimeout(timeOut);
  //recvPort4.callbackTimeout(timeOut);

  // if we're in debug mode, enable the source and timeout callbacks for port 1
  if (DEBUG) {
    recvPort1.callbackSource(newSource);
  }

  // start sacn receivers
  recvPort1.begin();
  //recvPort2.begin();
  //recvPort3.begin();
  //recvPort4.begin();

  // debug print
  if (DEBUG) {
    Serial.println("sACN start");
  }
}

//-------------------------------------------------------------------------------------------------
// core 0 loop
void loop() {
  // send the output down the line
  // we need to pause the second core, to prevent data corruption during the update
  rp2040.idleOtherCore();

  // need to constantly send, cannot only send on new data (wowsers)
  port1.write(dmxBufferPort1, 513);
  port2.write(dmxBufferPort2, 513);
  port3.write(dmxBufferPort3, 513);
  port4.write(dmxBufferPort4, 513);

  // when done, release the second core
  rp2040.resumeOtherCore();

  // wait until done writing for stability purposes
  while (port1.busy() || port2.busy() || port3.busy() || port4.busy()){}

  // put addr 1 for port 1 on the onboard led for debug purposes
  if (DEBUG) {
    analogWrite(25, dmxBufferPort1[1]);
  }

  // stability delay (needed?)
  delay(1);
}


//--------------------------------------------------------------------------------------------------
// core 1 loop
void loop1() {
  // keep DHCP lease if we're using DHCP
  if (DHCP) {
    Ethernet.maintain();
  }

  // check to see if we've receieved any sACN packets (FOR DEBUG PURPOSES)
  boolean sacnReceived = false;

  sacnReceived = recvPort1.receive();
  //sacnReceived = recvPort2.receive();
  //sacnReceived = recvPort3.receive();
  //sacnReceived = recvPort4.receive();

  // blink output
  if(sacnReceived){
    greenBlink();
  }

  // debug output
  if (sacnReceived && DEBUG) {
    Serial.println("new sACN packet(s)");
  }
}
