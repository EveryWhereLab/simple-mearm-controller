/* This demo accepts commands from host to calibrate angle offsets
 * in a specific posture of meArm. The code was tested on a STM32F103
 * blue pill board. You need to modify the definition of pins if you
 * want to use this code on other boards.
 * Pins:
 * Arduino    Base   Shoulder  Elbow     Claw
 *    GND    Brown     Brown   Brown     Brown
 *     5V      Red       Red     Red       Red
 *     PB7    Yellow
 *     PB6             Yellow
 *     PB5                      Yellow
 *     PB4                               Yellow
 *
 * Created by James.Cheng@everywherelab.com October 2019
 */
 
#include <Servo.h>
Servo Servos[4];  // create four servo objects to control servos
const int basePin = PB7;
const int shoulderPin = PB6;
const int elbowPin = PB5;
const int clawPin = PB4;

typedef enum {
    NoMenu,
    RootMenu,
    CalirationMenu
} Menu_t;

int offsetDegree = 0;
int currentMenu = NoMenu;
int needMenu = RootMenu;

const int jointAnglesToBeCalibrated[] = {
  90,
  45,
  90,
  45
};

int selectedJoint = -1;

const char jointDescriptions[][32] = {
  "base at 90 deg.",
  "elbow at 45 deg.",
  "shoulder at 90 deg.",
  "claw at 45 deg."
};

void setup() {
  Serial.begin(115200);
      while(!Serial) ;
  Servos[0].attach(basePin);
  Servos[0].write(jointAnglesToBeCalibrated[0]);
  Servos[1].attach(elbowPin);
  Servos[1].write(jointAnglesToBeCalibrated[1]);
  Servos[2].attach(shoulderPin);
  Servos[2].write(jointAnglesToBeCalibrated[2]);
  Servos[3].attach(clawPin);
  Servos[3].write(jointAnglesToBeCalibrated[3]);
}

static void process_command(void) {
  int receivedChar;
  for (;;) {
    if(needMenu != currentMenu) {
      if(needMenu == RootMenu) {
        Serial.println(F("MeArm Calib. Menu:"));
        for(int i=0;i< 4;i++) {
          Serial.print(jointDescriptions[i][0]);
          Serial.print(F(" or "));
          Serial.print(char(jointDescriptions[i][0]& ~(0x20)));
          Serial.print(F(" ... to calibrate "));
          Serial.println(jointDescriptions[i]);
        }
        Serial.println(F("Press ? to go back to this menu."));
        currentMenu = RootMenu;
      }
      else if(needMenu == CalirationMenu && selectedJoint >= 0 && selectedJoint <4){
        Serial.print(F("Calibrating "));
        Serial.println(jointDescriptions[selectedJoint]);
        Serial.println(F("Pressing i to increase angle offset \
or pressing d to decrease angle offset."));
        currentMenu = CalirationMenu;
      }
    }
    receivedChar = Serial.read();
    receivedChar = tolower(receivedChar);
    for(int i = 0; i< 4;i++) {
      if(receivedChar == jointDescriptions[i][0]) {
        selectedJoint = i;
        needMenu = CalirationMenu;
        offsetDegree = 0;
        break;
      }
    }
    if(needMenu != currentMenu)
       break;
    if ( receivedChar == '?') {
       needMenu = RootMenu;
       break;
    }
    if( receivedChar == 'i'|| receivedChar == 'd') {
       offsetDegree+= (receivedChar == 'i'? 1:-1);
       Servos[selectedJoint].write(jointAnglesToBeCalibrated[selectedJoint] + offsetDegree);
       Serial.print(F("Check if "));
       Serial.print(offsetDegree);
       Serial.print(F(" deg. can compensate the bias of "));
       Serial.print(jointDescriptions[selectedJoint]);
       Serial.println(F(" or not."));
    }
  }
}

void loop() {
  process_command();
}
