This Arduino demo code accepts and executes G-code commands from host. We refer to Lamjonah's ThisArm_Firmware work(https://github.com/lamjonah/ThisArm_Firmware) to implement G-code processing flow. York Hackspace's MeArm library(https://github.com/yorkhackspace/meArm) is used to drive meArm to a specified position. The code was tested on a STM32F103 blue pill board and an ESP32 D1 R32 board. You need to modify the definition of pins if you want to use this code on other boards.

The acceptable commands are almost the same as the commands used in the ThisArm_Firmware except that claw only has open state(angle is larger than zero) and close state(angle equals zero):

|   Command| Description  |
| ------------ | ------------ |
| G00 [X/Y/Z/E(mm)] | Instance move |
| G01 [X/Y/Z/E(mm)] [F(mm/s)]  | linear move |
| G04 P[seconds]  | delay  |
| G90 | absolute mode |
| G91 | relative mode |
| M100 | help message |
| M106 [S(Claw state)] | 0: close; 1: open |
| M114 | report position and feedrate |
| M555 P1 | resetting wifi credentials and restarting ESP32 |

In this implementation, the communication between host and device can be achieved by a USB serial port(for STM32 and ESP32) or WiFi connections(only for ESP32). In the case of using Wifi communication, a TCP server run on ESP32 to listen for connections from host applications. A mDNS responder also run on ESP32 so that host applications can find the address and port number of that TCP server by using the mDNS-SD protocol with a specific service type "_mearm._tcp". A WiFiManager library(https://github.com/zhouhan0126/WIFIMANAGER-ESP32) is used to process ESP32 wifi configuration matters. It will set ESP32 to AP mode with SSID "MeARM_XXXX" and start a captive portal for handling configuration requests when auto connection fails after boot-up. In that configuration period(180s in our settings), you can set a new configuration using a web browser on a smartphone/tablet/PC machine which associates to this ESP32 AP.
