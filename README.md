# ArduinoWebSockets
[![Build Status](https://travis-ci.org/skaarj1989/ArduinoWebSockets.svg?branch=master)](https://travis-ci.org/skaarj1989/ArduinoWebSockets)
![GitHub issues](https://img.shields.io/github/issues/skaarj1989/ArduinoWebSockets.svg)
![GitHub](https://img.shields.io/github/license/skaarj1989/ArduinoWebSockets.svg)

Simple to use implementation of WebSocket client and server for Arduino.

**List of supported MCUs:**
- ATmega328P
- ATmega2560
- ARM Cortex M0
- ESP8266

The **Autobahn**|Testsuite reports for [server](https://skaarj1989.github.io/ArduinoWebSockets/autobahn-testsuite/servers/index.html) and [client](https://skaarj1989.github.io/ArduinoWebSockets/autobahn-testsuite/clients/index.html)

Some tests will never pass just because of memory lack in ATmega family.

## Table of contents

- [Requirements](#requirements)
- [Installation](#installation)
  * [config.h](#configh)
  * [Physical connection](#physical-connection)
- [Usage examples](#usage-examples)
  * [Server](#server)
  * [Client](#client)
  * [Chat](#chat)
- [Approx memory usage](#approx-memory-usage)
- [Ethernet "2" library modification](#ethernet-2-library-modification)
  * [Fix for Arduino Mega](#fix-for-arduino-mega)
- [Fix for arduino-base64 with ESP8266](#fix-for-arduino-base64-with-esp8266)
- [Known issues](#known-issues)
- [License](#license)

## Requirements

* Development board (confirmed working, other boards may or may not work):
  * Arduino Uno (ATmega328P)
  * Arduino Pro Mini (ATmega328P)
  * Arduino Mega2560
  * Arduino Zero / SAMD21 M0 (ARM Cortex M0)
  * WeMos D1 mini (ESP8266)
  * NodeMCU v3 (ESP8266)
* Ethernet module or shield (confirmed working):
  * Arduino Ethernet Shield W5100
  * WizNet W5500 module
  * ENC28j60
* Libraries:
  * [arduino-base64](https://github.com/adamvr/arduino-base64)
  * CryptoLegacy from [arduinolibs](https://github.com/rweather/arduinolibs), you can grab it from [here](CryptoLegacy.zip)
  * [Ethernet "2"](https://github.com/adafruit/Ethernet2) for W5500
    + Even though this library may be deprecated/archived it still works better with W5500 than official Arduino Ethernet library.
  * UIPEthernet [#1](https://github.com/ntruchsess/arduino_uip) or [#2](https://github.com/UIPEthernet/UIPEthernet) (the choice is yours) for ENC28j60

## Installation

Install this library, **arduino-base64** and **CryptoLegacy** in Arduino libraries directory, follow [this](https://www.arduino.cc/en/Guide/Libraries) guide if you don't know how to do it.

### config.h

Change below definition if you use a different controller:

```cpp
...

#define NETWORK_CONTROLLER ETHERNET_CONTROLLER_W5100
```

```
ETHERNET_CONTROLLER_W5100
ETHERNET_CONTROLLER_W5500
ETHERNET_CONTROLLER_ENC28J60
NETWORK_CONTROLLER_WIFI
```

Uncomment these if you want additional informations in serial monitor:

```cpp
//#define _DEBUG
//#define _DUMP_HANDSHAKE
//#define _DUMP_FRAME_DATA
//#define _DUMP_HEADER
```

Increase below value if you expect big data frames (or decrease for devices with small amount of memory)

```cpp
#define BUFFER_MAX_SIZE 256
```

### Physical connection

| W5100 Ethernet shield  | Arduino Uno / Mega2560 |  Arduino Pro Mini | Arduino Zero |
| :---: | :---: | :---: | :---: |
| (ICSP) MISO  | (ICSP) MISO  | PIN 12 | (ICSP) MISO |
| (ICSP) MOSI  | (ICSP) MOSI  | PIN 11 | (ICSP) MOSI |
| (ICSP) SCK  | (ICSP) SCK | PIN 13 | (ICSP) SCK |
| (SS) PIN 10  | PIN 10 | PIN 10 | PIN 10 |

| W5500 / ENC28j60  | Arduino Uno / Pro Mini | Arduino Mega2560 |
| :---: | :---: | :---: | 
| MISO  | PIN 12  | PIN 50 |
| MOSI  | PIN 11  | PIN 51 |
| SCS  | PIN 10  | PIN 53  |
| SCLK  | PIN 13  | PIN 52 |

## Usage examples

### Server

```cpp
#include <WebSocketServer.h>

WebSocketServer server(3000);

void onOpen(WebSocket &ws) {
  char message[] = "Hello from Arduino server!";
  ws.send(message, strlen(message));
}

void onClose(WebSocket &ws, const eWebSocketCloseEvent code, const char *reason, uint16_t length) { /* ... */ }
void onMessage(WebSocket &ws, const char *message, uint16_t length) { /* */ }

void setup() {
  // Ethernet/WiFi initialization goes here ...
  // ...

  server.setOnOpenCallback(onOpen);
  server.setOnCloseCallback(onClose);
  server.setOnMessageCallback(onMessage);
  
  server.begin();
}

void loop() {
  server.listen();
}
```

##### Node.js server examples [here](node.js)

### Client

```cpp
#include <WebSocketClient.h>

WebSocketClient client;

void onOpen(WebSocket &ws) {
  char message[] = "Hello from Arduino client!";
  ws.send(message, strlen(message));
}

void onClose(WebSocket &ws, const eWebSocketCloseEvent code, const char *reason, uint16_t length) { /* ... */ }
void onMessage(WebSocket &ws, const char *message, uint16_t length) { /* */ }

void setup() {
  // Ethernet/WiFi initialization goes here ...
  // ...
  
  client.setOnOpenCallback(onOpen);
  client.setOnCloseCallback(onClose);
  client.setOnMessageCallback(onMessage);
  
  client.open("host", 3000);
}

void loop() {
  client.listen();
}
```

### Chat

Following screenshots shows Rasperry Pi server, browser client and Arduino client in action:

###### Node.js server on Raspberry Pi (/node.js/chat.js)

<p align="center">
   <img src=https://github.com/skaarj1989/ArduinoWebSocketClient/blob/master/images/rpi-nodejs.png?raw=true">
</p>

###### Browser client (/node.js/chat-client.htm)

<p align="center">
   <img src=https://github.com/skaarj1989/ArduinoWebSocketClient/blob/master/images/browser-client.PNG?raw=true">
</p>

###### Arduino Uno client (/examples/chat/chat.ino)

<p align="center">
   <img src=https://github.com/skaarj1989/ArduinoWebSocketClient/blob/master/images/arduino-serial-monitor.png?raw=true">
</p>

##### More examples [here](examples)

## Approx memory usage

###### *simple-client.ino example

### Ethernet.h (W5100)

| Board  | Program space | Dynamic memory |
| :---: | :---: | :---: | 
| Arduino Uno  | 24416 bytes (75%)  | 1141 bytes (55%) |
| Arduino Mega2560  | 24704 bytes (9%) | 1177 bytes (14%) |
| Arduino Pro Mini | 24416 bytes (79%) | 1141 bytes (55%) |
| Arduino Zero | 28180 bytes (11%) | |

### Ethernet2.h (W5500)

| Board  | Program space | Dynamic memory |
| :---: | :---: | :---: | 
| Arduino Uno  | 21448 bytes (66%)  | 940 bytes (45%) |
| Arduino Mega2560  | 21746 bytes (8%) | 940 bytes (11%) |
| Arduino Pro Mini | 21448 bytes (69%) | 940 bytes (45%) |
| Arduino Zero | 25852 bytes (9%) | |

### UIPEthernet.h (ENC28j60)
:warning: **This library is incompatibile with Arduino Zero!** :warning:

| Board  | Program space | Dynamic memory |
| :---: | :---: | :---: | 
| Arduino Uno  | 29626 bytes (91%)  | 1788 bytes (87%) :warning: |
| Arduino Mega2560  | 30032 bytes (11%) | 1788 bytes (21%) |
| Arduino Pro Mini | 29626 bytes (96%) | 1788 bytes (87%) :warning: |

### WiFi

| Board  | Program space | Dynamic memory |
| :---: | :---: | :---: | 
| Generic ESP8266  | 265108 bytes (53%)  | 33624 bytes (41%) |
| WeMos D1 | 265108 bytes (25%) | 33624 bytes (41%) |
| NodeMCU | 265108 bytes (25%) | 33624 bytes (41%) |

## Ethernet "2" library modification

### Fix for Arduino Mega

#### Ethernet2.h

```cpp
#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__)
  EthernetClass() { _dhcp = NULL; w5500_cspin = 10; }
  void init(uint8_t _cspin) { w5500_cspin = _cspin; }
#elif defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
  EthernetClass() { _dhcp = NULL; w5500_cspin = 53; }
  void init(uint8_t _cspin = 53) { w5500_cspin = _cspin; }
#endif
```

## Fix for arduino-base64 with **ESP8266**

Move files: ``Base64.h`` and ``Base64.cpp`` to ``src`` directory of ``ArduinoWebSockets`` library.
In ``Base64.cpp`` change:

```
#include <avr/pgmspace.h>
```

to this:

```
#include "platform.h"
#include "Base64.h"
#if PLATFORM_ARCH == PLATFORM_ARCHITECTURE_ESP8266
#include <pgmspace.h>
#else
#include <avr/pgmspace.h>
#endif
```

## Known issues		

1. It seems that both official Arduino Ethernet and UIPEthernet library doesn't like *octet-wise chops* and fails following **Autobahn**|Testsuite cases:
   * Ping/Pongs
     + 2.6
     + 2.11
   * Reserved Bits
     + 3.4
   * Opcodes
     + 4.1.5
     + 4.2.5
   * Fragmentation
     + 5.5
     + 5.8
     + 5.11
2. ENC28j60 is slow and eats much more memory than W5100/W5500
3. For now **ESP8266** works only with **WebSocketClient**

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details
