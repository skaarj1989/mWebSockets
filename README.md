# μWebSockets
[![Build Status](https://travis-ci.org/skaarj1989/mWebSockets.svg?branch=master)](https://travis-ci.org/skaarj1989/mWebSockets)
![GitHub issues](https://img.shields.io/github/issues/skaarj1989/mWebSockets.svg)
![GitHub](https://img.shields.io/github/license/skaarj1989/mWebSockets.svg)

Simple to use implementation of WebSockets for microcontrollers.

**List of supported IDEs:**
- [Arduino](https://www.arduino.cc/en/Main/Software)
- Visual Studio Code with [Platformio](https://platformio.org/)
- Microsoft Visual Studio with [Visual Micro](https://www.visualmicro.com/)

**List of supported MCUs:**
- ATmega328P
- ATmega2560
- ARM Cortex M0
- ESP8266

**WebSocketServer compatible browsers:**
- Chrome
- Edge
- Firefox

The **Autobahn**|Testsuite reports for [server](https://skaarj1989.github.io/mWebSockets/autobahn-testsuite/servers/index.html) and [client](https://skaarj1989.github.io/mWebSockets/autobahn-testsuite/clients/index.html)

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
    + This library is deprecated you can use official Arduino Ethernet library.
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

``ETHERNET_CONTROLLER_W5100`` means official Arduino Ethernet library. It is possible to use this with both **W5100** and **W5500**.
``ETHERNET_CONTROLLER_W5500`` stands for **Ethernet "2"** library.

Uncomment these if you want additional informations in serial monitor:

```cpp
//#define _DEBUG
//#define _DUMP_HANDSHAKE
//#define _DUMP_FRAME_DATA
//#define _DUMP_HEADER
```

Increase below value if you expect big data frames (or decrease for devices with small amount of memory)

```cpp
constexpr uint16_t kBufferMaxSize = 256;
```

### Physical connection

If you have **WeMos D1** in size of **Arduino Uno** simply attaching shield does not work, you have to wire **ICSP** on **Ethernet Shield** to proper pins.

| W5100 Ethernet shield  | Arduino Uno / Mega2560 |  Arduino Pro Mini | Arduino Zero | WeMos D1 |
| :---: | :---: | :---: | :---: | :--- |
| (ICSP) MISO  | (ICSP) MISO  | PIN 12 | (ICSP) MISO | D12 / MISO |
| (ICSP) MOSI  | (ICSP) MOSI  | PIN 11 | (ICSP) MOSI | D11 / MOSI
| (ICSP) SCK  | (ICSP) SCK | PIN 13 | (ICSP) SCK | D13 / SCK
| (SS) PIN 10  | PIN 10 | PIN 10 | PIN 10 | D10 / SS

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
using namespace net;

WebSocketServer server(3000);

void setup() {
  // Ethernet/WiFi initialization goes here ...
  // ...

  server.onConnection([](WebSocket &ws) {
    const char message[]{ "Hello from Arduino server!" };
    ws.send(message, strlen(message));

    ws.onClose([](WebSocket &ws, const WebSocket::CloseCode &code,
                 const char *reason, uint16_t length) {
      // ...
    });
    ws.onMessage([](WebSocket &ws, const WebSocket::DataType &dataType,
                   const char *message, uint16_t length) {
      // ...
    });
  });

  server.begin();
}

void loop() {
  server.listen();
}
```

#### Verify clients

```cpp
void setup() {
  // ...

  // verifyClient callback is called for every header during handshake
  // (except for those required by protocol, like "Connection", "Upgrade" etc.)
  server.begin([](const IPAddress &ip, const char *header, const char *value) {
    // verify ip ...
    
    // verify "Origin" header:
    if (strcmp_P(header, (PGM_P)F("Origin")) == 0)
      if (strcmp_P(value, (PGM_P)F("file://")) == 0) return false;

    return true;
  });
}
```

##### Node.js server examples [here](node.js)

### Client

```cpp
#include <WebSocketClient.h>
using namespace net;

WebSocketClient client;

void setup() {
  // Ethernet/WiFi initialization goes here ...
  // ...

  client.onOpen([](WebSocket &ws) {
    const char message[]{ "Hello from Arduino client!" };
    ws.send(message, strlen(message));
  });
  client.onClose([](WebSocket &ws, const WebSocket::CloseCode &code,
                   const char *reason, uint16_t length) {
    // ...
  });
  client.onMessage([](WebSocket &ws, const WebSocket::DataType &dataType,
                     const char *message, uint16_t length) {
    // ...
  });

  client.open("echo.websocket.org", 80);
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

###### *simple-client.ino example (without debug output)

### Ethernet.h (W5100 and W5500)

| Board  | Program space | Dynamic memory |
| :---: | :---: | :---: | 
| Arduino Uno  | 24538 bytes (76%)  | 1012 bytes (49%) |
| Arduino Mega2560  | 24836 bytes (9%) | 1048 bytes (12%) |
| Arduino Pro Mini | 24538 bytes (79%) | 1012 bytes (49%) |
| Arduino Zero | 28308 bytes (10%) | |

### Ethernet2.h (W5500)

| Board  | Program space | Dynamic memory |
| :---: | :---: | :---: | 
| Arduino Uno  | 21592 bytes (66%) | 811 bytes (39%) |
| Arduino Mega2560  | 21900 bytes (8%) | 811 bytes (9%) |
| Arduino Pro Mini | 21592 bytes (70%) | 811 bytes (39%) |
| Arduino Zero | 25988 bytes (9%) | |

### UIPEthernet.h (ENC28j60)
:warning: **This library is incompatibile with Arduino Zero!** :warning:

| Board  | Program space | Dynamic memory |
| :---: | :---: | :---: | 
| Arduino Uno  | 29762 bytes (92%) | 1659 bytes (81%) :warning: |
| Arduino Mega2560  | 30172 bytes (11%) | 1659 bytes (20%) |
| Arduino Pro Mini | 29762 bytes (96%) | 1659 bytes (81%) :warning: |

### WiFi

| Board  | Program space | Dynamic memory |
| :---: | :---: | :---: | 
| Generic ESP8266  | 265864 bytes (53%) | 33644 bytes (41%) |
| WeMos D1 mini | 265864 bytes (25%) | 33644 bytes (41%) |
| NodeMCU | 265864 bytes (25%) | 33624 33644 (41%) |

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

Move files: ``Base64.h`` and ``Base64.cpp`` to ``src`` directory of ``mWebSockets`` library.
In ``Base64.cpp`` change:

```cpp
#include <avr/pgmspace.h>
```

to this:

```cpp
#include "platform.h"
#include "Base64.h"
#if PLATFORM_ARCH == PLATFORM_ARCHITECTURE_ESP8266
#include <pgmspace.h>
#else
#include <avr/pgmspace.h>
#endif
```

## Known issues		

1. ENC28j60 is slow, eats much more memory than W5100/W5500 and hangs on ``available()`` function

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details
