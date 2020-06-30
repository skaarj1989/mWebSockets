# μWebSockets
[![arduino-library-badge](https://www.ardu-badge.com/badge/mWebSockets.svg?)](https://www.ardu-badge.com/mWebSockets)
[![Build Status](https://travis-ci.org/skaarj1989/mWebSockets.svg?branch=master)](https://travis-ci.org/skaarj1989/mWebSockets)
[![CodeFactor Grade](https://img.shields.io/codefactor/grade/github/skaarj1989/mWebSockets)](https://www.codefactor.io/repository/github/skaarj1989/mwebsockets/overview/master)
![GitHub](https://img.shields.io/github/license/skaarj1989/mWebSockets.svg)

Simple to use implementation of WebSockets for microcontrollers.

**List of supported IDEs:**
- [Arduino](https://www.arduino.cc/en/Main/Software)
- Visual Studio Code with [Platformio](https://platformio.org/)
- Microsoft Visual Studio with [Visual Micro](https://www.visualmicro.com/)

**List of supported MCUs:**
- ATmega328P
- ATmega2560
- SAMD21 (ARM Cortex-M0+)
- STM32 (ARM Cortex-M)
- ESP8266
- ESP32-WROOM-32D

**WebSocketServer compatible browsers:**
- Chrome
- Edge
- Firefox
- Opera

The **Autobahn**|Testsuite reports for [server](https://skaarj1989.github.io/mWebSockets/autobahn-testsuite/servers/index.html) and [client](https://skaarj1989.github.io/mWebSockets/autobahn-testsuite/clients/index.html)
<br><sup>Some tests will never pass just because of memory lack in ATmega family.</sup>

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
- [Known issues](#known-issues)
- [License](#license)

## Requirements

* Development board (confirmed working, other boards may or may not work):
  * Arduino Uno (ATmega328P)
  * Arduino Pro Mini (ATmega328P)
  * Arduino Mega2560
  * Arduino Zero / SAMD21 M0 (ARM Cortex-M0)
  * STM Nucleo-64 (ARM Cortex-M)
  * WeMos D1 mini (ESP8266)
  * NodeMCU v3 (ESP8266)
  * ESPDUINO-32 (ESP32-WROOM-32D)
* Ethernet module or shield (confirmed working):
  * Arduino Ethernet Shield (W5100)
  * Arduino Ethernet Shield 2 (W5500)
  * WizNet W5500 module
  * ENC28j60
* Libraries:
  * UIPEthernet [#1](https://github.com/ntruchsess/arduino_uip) or [#2](https://github.com/UIPEthernet/UIPEthernet) (the choice is yours) if you decide to use ENC28j60

## Installation

Use [Arduino Download Manager](https://www.ardu-badge.com/mWebSockets) or follow [this](https://www.arduino.cc/en/Guide/Libraries) guide.


### config.h

Change below definition if you use a different controller:

```cpp
...

#define NETWORK_CONTROLLER ETHERNET_CONTROLLER_W5X00
```

```
ETHERNET_CONTROLLER_W5X00
ETHERNET_CONTROLLER_ENC28J60
NETWORK_CONTROLLER_WIFI
```

``ETHERNET_CONTROLLER_W5X00`` stands for official Arduino Ethernet library.

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

| Ethernet Shield <br> (W5100/W5500) | Arduino <br> Pro Mini | WeMos D1 |
| :---: | :---: | :---: |
| (ICSP) MISO  | PIN 12 | D12 / MISO |
| (ICSP) MOSI  | PIN 11 | D11 / MOSI |
| (ICSP) SCK   | PIN 13 | D13 / SCK  |
| (SS) PIN 10  | PIN 10 | D10 / SS   |

| W5500 / <br> ENC28j60  | Arduino Uno / <br> Pro Mini | Arduino <br> Mega2560 |
| :---: | :---: | :---: | 
| MISO  | PIN 12  | PIN 50 |
| MOSI  | PIN 11  | PIN 51 |
| SCS   | PIN 10  | PIN 53 |
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
   <img src=https://github.com/skaarj1989/ArduinoWebSocketClient/blob/gh-pages/images/rpi-nodejs.png?raw=true">
</p>

###### Browser client (/node.js/chat-client.htm)

<p align="center">
   <img src=https://github.com/skaarj1989/ArduinoWebSocketClient/blob/gh-pages/images/browser-client.PNG?raw=true">
</p>

###### Arduino Uno client (/examples/chat/chat.ino)

<p align="center">
   <img src=https://github.com/skaarj1989/ArduinoWebSocketClient/blob/gh-pages/images/arduino-serial-monitor.png?raw=true">
</p>

##### More examples [here](examples)

## Approx memory usage

###### *simple-client.ino example (without debug output, 128 bytes data buffer)

### Ethernet.h (W5100 and W5500)

| Board  | Program space | Dynamic memory |
| :---: | :---: | :---: | 
| Arduino Uno  | 25 114 bytes (78%)  | 917 bytes (45%) |
| Arduino Mega2560  | 25 310 bytes (10%) | 945 bytes (12%) |
| Arduino Pro Mini | 25 114 bytes (82%) | 917 bytes (45%) |
| Arduino Zero | 31 128 bytes (12%) | 3 924 bytes |

### UIPEthernet.h (ENC28j60)
:warning: **This library is incompatibile with Arduino Zero!** :warning:

| Board  | Program space | Dynamic memory |
| :---: | :---: | :---: | 
| Arduino Uno  | 30 208 bytes (93%) | 1 519 bytes (74%) |
| Arduino Mega2560  | 30 356 bytes (11%) | 1 519 bytes (18%) |
| Arduino Pro Mini | 30 208 bytes (98%) | 1 519 bytes (74%) |

### WiFi

| Board  | Program space | Dynamic memory |
| :---: | :---: | :---: | 
| Generic ESP8266  | 286 916 bytes (30%) | 27 556 bytes (34%) |
| WeMos D1 mini | 286 916 bytes (27%) | 27 556 bytes (34%) |
| NodeMCU | 286 916 bytes (27%) | 27 556 (33%) |

## Known issues

1. ENC28j60 is slow, eats much more memory than W5100/W5500 and hangs on ``available()`` function

## License

* This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details
* [arduino-base64](https://github.com/adamvr/arduino-base64) licensed under licensed under MIT License
* [arduinolibs](https://github.com/rweather/arduinolibs) licensed under MIT
* [utf8_check](https://www.cl.cam.ac.uk/~mgk25/ucs/utf8_check.c) licensed under MIT
