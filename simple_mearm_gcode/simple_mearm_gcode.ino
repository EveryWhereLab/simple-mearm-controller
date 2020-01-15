/* This Arduino demo code accepts and executes G-code commands from host.
 * We refer to Lamjonah's ThisArm_Firmware work(https://github.com/lamjonah/ThisArm_Firmware)
 * to implement G-code processing flow. York Hackspace's MeArm library is used to drive meArm
 * to a specified position. The code was tested on a STM32F103 blue pill board and an ESP32 D1 R32 board.
 * You need to modify the definition of pins if you want to use this code on other boards.
 * Pins used in our tests for the STM32F103 blue pill board:
 * Arduino    Base   Shoulder    Elbow    Claw
 *    GND    Brown      Brown    Brown   Brown
 *     5V      Red        Red      Red     Red
 *    PB7   Yellow
 *    PB6              Yellow
 *    PB5                       Yellow
 *    PB4                               Yellow
 * Pins used in our tests for the ESP32 D1 R32 board:
 * Arduino    Base   Shoulder    Elbow    Claw
 *    GND    Brown      Brown    Brown   Brown
 *     5V      Red        Red      Red     Red
 *     12   Yellow
 *     13              Yellow
 *     14                       Yellow
 *     27                               Yellow
 *
 * Created by James.Cheng@everywherelab.com October 2019
 */

#include "meArm.h"                          // https://github.com/yorkhackspace/meArm
#define MAX_BUF              (64)

const char crlf[] = "\r\n";
#if defined(ARDUINO_ARCH_ESP32)
const char helpText[] PROGMEM = "simple_mearm_gcode Commands:\r\n\
G00 [X/Y/Z/E(mm)] ; - Instance move\r\n\
G01 [X/Y/Z/E(mm)] [F(mm/s)]; - linear move\r\n\
G04 P[seconds]; - delay\r\n\
G90; - absolute mode\r\n\
G91; - relative mode\r\n\
M100; - this help message\r\n\
M106 [S(Claw angle)]; - set angle of Claw\r\n\
M114; - report position and feedrate\r\n\
M555 P1; - resetting wifi credentials and restarting ESP32\r\n\
All commands must end with a newline.\r\n";
#else
const char helpText[] PROGMEM = "simple_mearm_gcode Commands:\r\n\
G00 [X/Y/Z/E(mm)] ; - Instance move\r\n\
G01 [X/Y/Z/E(mm)] [F(mm/s)]; - linear move\r\n\
G04 P[seconds]; - delay\r\n\
G90; - absolute mode\r\n\
G91; - relative mode\r\n\
M100; - this help message\r\n\
M106 [S(Claw angle)]; - set angle of Claw\r\n\
M114; - report position and feedrate\r\n\
All commands must end with a newline.\r\n";
#endif

#define FP(x)     (__FlashStringHelper*)(x)         // Flash String Helper

#if defined(ARDUINO_ARCH_ESP32)

#include <WiFiManager.h>                    // https://github.com/zhouhan0126/WIFIMANAGER-ESP32
#include <ESPmDNS.h>

uint16_t chipid;
char hostString[16] = {0};
WiFiServer server(8998);
WiFiClient client;

#define PRINT(x)             \
  if(client.connected())     \
    client.print(x);         \
  Serial.print(x)
#define PRINTLN(x)           \
  if(client.connected())     \
    client.println(x);       \
  Serial.println(x)
#define WRITE_STRING(x)      \
  if(client.connected())     \
    client.write(x.c_str(),x.length()); \
  Serial.write(x.c_str())

#else

#define PRINT(x) Serial.print(x)
#define PRINTLN(x) Serial.println(x)
#define WRITE_STRING(x) Serial.write(x.c_str())

#endif

// Setting up GPIOs here
#if defined(ARDUINO_ARCH_STM32)
const int basePin = PB7;
const int shoulderPin = PB6;
const int elbowPin = PB5;
const int clawPin = PB4;
#else
const int basePin = 12;
const int shoulderPin = 13;
const int elbowPin = 14;
const int clawPin = 27;
#endif

float FeedRate=1000.0;
char mode_abs=1;  // absolute mode, default yes
int baseBias = -10;
int shoulderBias = -5;
int elbowBias = 0;
int clawOffest = 0;
char requestBuffer[MAX_BUF];  // where we store the request
String strResponse;  //  to collect results for answering the request
int sofar=0;  // how much is in the requestBuffer

// Cartesian control, in XYZ coordinate
float X=0.0;
float Y=100.0;
float Z=50.0;
float C=60.0;

meArm arm(135 + baseBias,45 + baseBias,-pi/4,pi/4,
  135 + shoulderBias, 45 + shoulderBias, pi/4, 3*pi/4,
  135 + elbowBias, 45 + elbowBias, pi/4, -pi/4,
  0, 60, pi/2, 0);

void setup() {
  Serial.begin(115200);
  while(!Serial) ;
#if defined(ARDUINO_ARCH_ESP32)
  chipid = (uint16_t)ESP.getEfuseMac();
  sprintf(hostString, "MeARM_%04X",chipid);
  WiFi.setHostname(hostString);
  WiFiManager wifiManager;
  // sets timeout until configuration portal gets turned off
  wifiManager.setTimeout(180);
  // fetches ssid and pass and tries to connect
  // if it does not connect it starts an access point with the specified host name
  // with password and goes into a blocking loop awaiting configuration
  // Request packets from associated machines will be redirected to this captive portal(192.168.4.1).
  if (!wifiManager.autoConnect(hostString, "12345678")) {
     Serial.println(F("failed to connect and hit timeout"));
  }
  else {
    Serial.println(F("setting up mdns responder"));
    // Set up mDNS responder
    MDNS.begin(hostString);
    // Add service to MDNS-SD
    MDNS.addService(F("mearm"), F("tcp"), 8998);
    // Start the TCP server
    server.begin();
  }
#endif
  arm.begin(basePin, shoulderPin, elbowPin, clawPin);
  arm.gotoPoint(X,Y,Z);
  arm.openGripper();
}

float parsenumber(char code,float val) {
  char *ptr=requestBuffer;
  while(ptr && *ptr && ptr<requestBuffer+sofar) {
    if(*ptr==code) {
      return atof(ptr+1);
    }
    ptr=strchr(ptr,' ');
    if(ptr != NULL)
      ptr+=1;
  }
  return val;
}

void output(char *code,float val) {
  char buff[7];
  strResponse+=code;
  dtostrf(val, 3, 1, buff);
  strResponse+=buff;
  strResponse+=" ";
}

void where() {
  output("X",X);
  output("Y",Y);
  output("Z",Z);
  output("C",C);
  output("F",FeedRate);
  strResponse+= (mode_abs? "ABS":"REL");
  strResponse+=crlf;
}

void pause(long ms) {
  strResponse += String(ms, DEC); 
  strResponse +=crlf;
  delay(ms);
}

int LinearSetpoint (double NewX, double NewY, double NewZ, double MMpM){  // feed rate in MM per Minute
  int num_of_Loop;
  int msPerLoop=10;  // around 100 Hz
  int i=0;
  double deltaX, deltaY, deltaZ;
  double Distance;
  int Startmillis;
  deltaX=NewX-X;
  deltaY=NewY-Y;
  deltaZ=NewZ-Z;
  Distance=sqrt(deltaX*deltaX+deltaY*deltaY+deltaZ*deltaZ);
  num_of_Loop=Distance/MMpM*60*1000/msPerLoop;
  for (i=1; i<=num_of_Loop;i++){
    Startmillis=millis();
    arm.gotoPoint(X+deltaX/num_of_Loop,Y+deltaY/num_of_Loop,Z+deltaZ/num_of_Loop);
    while (millis()-Startmillis<msPerLoop){ };
  }
  arm.gotoPoint(NewX,NewY,NewZ);
}

void processCommand() {
  float NewX, NewY, NewZ;
  int cmd = parsenumber('G',-1);
  switch(cmd) {
    case  0:{
      if (mode_abs==1){
        NewX=parsenumber('X',X);
        NewY=parsenumber('Y',Y);
        NewZ=parsenumber('Z',Z);
      }else {
        NewX=X+parsenumber('X',0);
        NewY=Y+parsenumber('Y',0);
        NewZ=Z+parsenumber('Z',0);
      }
      X = NewX;
      Y = NewY;
      Z = NewZ;
      arm.gotoPoint(X,Y,Z);
      where();
      break;
    }
    case  1: {  // line
      if (mode_abs==1){
        NewX=parsenumber('X',X);
        NewY=parsenumber('Y',Y);
        NewZ=parsenumber('Z',Z);
      }else {
        NewX=X+parsenumber('X',0);
        NewY=Y+parsenumber('Y',0);
        NewZ=Z+parsenumber('Z',0);
      }
      FeedRate=parsenumber('F',FeedRate);
      LinearSetpoint(NewX, NewY, NewZ,FeedRate);
      where();
      break;
    }
    case  4:  pause(parsenumber('P',0)*1000);  break;  // dwell
    case 90:  mode_abs=1;  break;  // absolute mode
    case 91:  mode_abs=0;  break;  // relative mode
    default:  break;
  }
  cmd = parsenumber('M',-1);
  switch(cmd) {
    case 100: strResponse = FP(helpText);  break;
    case 106: {
      C = parsenumber('S',70);
      if(C > 0.0)
        arm.openGripper();
      else
        arm.closeGripper();
      break;
    }
    case 114: where();  break;
#if defined(ARDUINO_ARCH_ESP32)
    case 555:
      C = parsenumber('P',0);
      if(C == 1.0) {
        Serial.println(F("Resetting wifi credentials and restarting ESP32..."));
        WiFi.disconnect(true,true);
        ESP.restart();
      }
#endif
    default:  break;
  }
}

void ready() {
  delay(10);
  sofar=0;  // clear input requestBuffer
  strResponse += "ok";  // signal ready to receive input
  strResponse += crlf;  // signal ready to receive input
  strResponse += ">";
}

void GCodeControl(){
  strResponse="";
  // listen for serial commands
#if defined(ARDUINO_ARCH_ESP32)
  while(client.available() > 0 || Serial.available() > 0) {  // if something is available
    char c;
    if(client.available() > 0)
      c=client.read();  // get it
    else if(Serial.available() > 0)
      c=Serial.read();
#else
  while(Serial.available() > 0) {  // if something is available
    char c;
    c=Serial.read();
#endif
    if(sofar<MAX_BUF-1) requestBuffer[sofar++]=c;  // store it
    if(c=='\n') {
      // entire message received
      requestBuffer[sofar]=0;  // end the requestBuffer so string functions work right
      strResponse = requestBuffer;  // repeat it back so I know you got the message
      strResponse += crlf;  // echo a return character for humans
      WRITE_STRING(strResponse);
      //Serial.print ("sofar Size: ");
      //Serial.println (sofar);
      strResponse="";
      processCommand();  // do something with the command
      ready();
      WRITE_STRING(strResponse);
    }
  }
}

void loop() {
#if defined(ARDUINO_ARCH_ESP32)
  if(!client.connected() && server.hasClient()) {
    // try to connect to the client
    client = server.available();
    client.setNoDelay(true);
  }
#endif
  GCodeControl();
}
