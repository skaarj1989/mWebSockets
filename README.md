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

> The **Autobahn**|Testsuite reports for [server](https://skaarj1989.github.io/mWebSockets/autobahn-testsuite/servers/index.html) and [client](https://skaarj1989.github.io/mWebSockets/autobahn-testsuite/clients/index.html) <br><sup>Some tests will never pass just because of memory lack in ATmega family.</sup>

## Table of contents

- [μWebSockets](#μwebsockets)
  - [Table of contents](#table-of-contents)
  - [Requirements](#requirements)
  - [Installation](#installation)
    - [Config.h](#configh)
    - [Physical connection](#physical-connection)
  - [Prerequisite](#prerequisite)
  - [Usage examples](#usage-examples)
    - [Server](#server)
      - [Verify clients](#verify-clients)
      - [Subprotocol negotiation](#subprotocol-negotiation)
    - [Client](#client)
    - [Chat](#chat)
  - [Approx memory usage](#approx-memory-usage)
    - [Ethernet.h (W5100 and W5500)](#etherneth-w5100-and-w5500)
    - [EthernetENC.h (ENC28j60)](#ethernetench-enc28j60)
    - [WiFi](#wifi)
  - [Known issues](#known-issues)
  - [License](#license)

## Requirements

- Development board (confirmed working, other boards may or may not work):
  - Arduino Uno (ATmega328P)
  - Arduino Pro Mini (ATmega328P)
  - Arduino Mega2560
  - Arduino Zero / SAMD21 M0 (ARM Cortex-M0)
  - STM Nucleo-64 (ARM Cortex-M)
  - WeMos D1 mini (ESP8266)
  - NodeMCU v3 (ESP8266)
  - ESPDUINO-32 (ESP32-WROOM-32D)
- Ethernet module or shield (confirmed working):
  - Arduino Ethernet Shield (W5100)
  - Arduino Ethernet Shield 2 (W5500)
  - WizNet W5500 module
  - ENC28j60
- Libraries:
  - [EthernetENC](https://github.com/jandrassy/EthernetENC) if you decide to use ENC28j60

## Installation

Use [Arduino Download Manager](https://www.ardu-badge.com/mWebSockets) or follow [this](https://www.arduino.cc/en/Guide/Libraries) guide.

### Config.h

Uncomment following defines if you want additional information on the serial monitor:

```cpp
//#define _DEBUG
//#define _DUMP_HANDSHAKE
//#define _DUMP_FRAME_DATA
//#define _DUMP_HEADER
```

Increase the following value if you expect big data frames (or decrease for devices with a small amount of memory).

```cpp
constexpr uint16_t kBufferMaxSize{ 256 };
```

### Physical connection

If you have a **WeMos D1** in the size of **Arduino Uno** simply attaching a shield does not work. You have to wire the **ICSP** on an **Ethernet Shield** to proper pins.

| Ethernet Shield <br> (W5100/W5500) | Arduino <br> Pro Mini |  WeMos D1  |
| :--------------------------------: | :-------------------: | :--------: |
|            (ICSP) MISO             |        PIN 12         | D12 / MISO |
|            (ICSP) MOSI             |        PIN 11         | D11 / MOSI |
|             (ICSP) SCK             |        PIN 13         | D13 / SCK  |
|            (SS) PIN 10             |        PIN 10         |  D10 / SS  |

| W5500 / <br> ENC28j60 | Arduino Uno / <br> Pro Mini | Arduino <br> Mega2560 |
| :-------------------: | :-------------------------: | :-------------------: |
|         MISO          |           PIN 12            |        PIN 50         |
|         MOSI          |           PIN 11            |        PIN 51         |
|          SCS          |           PIN 10            |        PIN 53         |
|         SCLK          |           PIN 13            |        PIN 52         |

## Prerequisite

You need to provide your own implementation of `fetchRemoteIp`:

```cpp
// This is necessary because:
// 1. Not every implementation of {Etherent/Wifi}Client implements remoteIP() function
// 2. Arduino does not have <type_traits> which could allow use of SFINAE

IPAddress fetchRemoteIp(const EthernetClient &client) {
  return client.remoteIP();
}
```

## Usage examples

```cpp
// For WiFi
#include <WiFi.h> // or ESP8266WiFi.h

using MyWebSocket = WebSocket<WiFiClient>;
using MyWebSocketClient = WebSocketClient<WiFiClient>;
using MyWebSocketServer = WebSocketServer<WiFiServer, WiFiClient, /*maxConnections = */4>;
```

### Server

```cpp
#include <Ethernet.h>
#include <WebSocketServer.hpp>
using namespace net;

using MyWebSocket = WebSocket<EthernetClient>;
using MyWebSocketServer = WebSocketServer<EthernetServer, EthernetClient, 4>;
MyWebSocketServer server{3000};

void setup() {
  // Ethernet/WiFi initialization goes here ...
  // ...

  server.onConnection([](MyWebSocket &ws) {
    const char message[]{ "Hello from Arduino server!" };
    ws.send(message, strlen(message));

    ws.onClose([](MyWebSocket &ws, const WebSocketCloseCode code,
                  const char *reason, uint16_t length) {
      // ...
    });
    ws.onMessage([](MyWebSocket &ws, const WebSocketDataType dataType,
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
// verifyClient callback is called for every header during handshake
// (except for those required by protocol, like "Connection", "Upgrade" etc.)
server.begin([](const IPAddress &ip, const char *header, const char *value) {
  // verify ip ...

  // verify "Origin" header:
  if (strcmp_P(header, (PGM_P)F("Origin")) == 0)
    if (strcmp_P(value, (PGM_P)F("file://")) == 0) return false;

  return true;
});
```

#### Subprotocol negotiation

```cpp
// If you won't pass callback for `protocolHandler` then server will use the
// first requested subprotocol if any
wss.begin(nullptr, [](const char *protocols) {
  // iterate csv protocols and return the one that is supported by your server
  // or nullptr to ignore
});

// You can check client protocol in other callbacks
wss.onConnection([](MyWebSocket &ws) {
  const auto protocol = ws.getProtocol();
  // ...
  }
});
```

> Node.js server examples [here](https://github.com/skaarj1989/mWebSockets/tree/master/node.js)

### Client

```cpp
#include <Ethernet.h>
#include <WebSocketClient.hpp>
using namespace net;

using MyWebSocket = WebSocket<EthernetClient>;
using MyWebSocketClient = WebSocketClient<EthernetClient>;
MyWebSocketClient client;

void setup() {
  // Ethernet/WiFi initialization goes here ...
  // ...

  client.onOpen([](MyWebSocket &ws) {
    const char message[]{ "Hello from Arduino client!" };
    ws.send(message, strlen(message));
  });
  client.onClose([](MyWebSocket &ws, const WebSocketCloseCode code,
                    const char *reason, uint16_t length) {
    // ...
  });
  client.onMessage([](MyWebSocket &ws, const WebSocketDataType dataType,
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

> Node.js server on Raspberry Pi (/node.js/chat.js)

<p align="center">
   <img src=https://github.com/skaarj1989/mWebSockets/blob/gh-pages/images/rpi-nodejs.png?raw=true">
</p>

> Browser client (/node.js/chat-client.htm)

<p align="center">
   <img src=https://github.com/skaarj1989/mWebSockets/blob/gh-pages/images/browser-client.PNG?raw=true">
</p>

> Arduino Uno client (/examples/chat/chat.ino)

<p align="center">
   <img src=https://github.com/skaarj1989/mWebSockets/blob/gh-pages/images/arduino-serial-monitor.png?raw=true">
</p>

> More examples [here](examples)

## Approx memory usage

> `simple-client.ino` example (without debug output, 128 bytes data buffer)

### Ethernet.h (W5100 and W5500)

|      Board       |   Program space    |  Dynamic memory   |
| :--------------: | :----------------: | :---------------: |
|   Arduino Uno    | 24 732 bytes (76%) |  799 bytes (39%)  |
| Arduino Mega2560 | 25 690 bytes (10%) |  827 bytes (10%)  |
| Arduino Pro Mini | 24 732 bytes (80%) |  799 bytes (39%)  |
|   Arduino Zero   | 30 512 bytes (11%) | 3 848 bytes (11%) |

### EthernetENC.h (ENC28j60)

|      Board       |    Program space    |  Dynamic memory   |
| :--------------: | :-----------------: | :---------------: |
|   Arduino Uno    | 31 062 bytes (96%)  | 1 406 bytes (68%) |
| Arduino Mega2560 | 32 074 bytes (12%)  | 1 406 bytes (17%) |
| Arduino Pro Mini | 31 062 bytes (101%) | 1 406 bytes (68%) |
|   Arduino Zero   | 36 796 bytes (14%)  | 3 684 bytes (11%) |

### WiFi

|           Board           |    Program space    |   Dynamic memory   |
| :-----------------------: | :-----------------: | :----------------: |
| Generic ESP8266 \ NodeMCU | 288 493 bytes (30%) | 28 492 bytes (34%) |
|       WeMos D1 mini       | 293 137 bytes (28%) | 28 500 bytes (34%) |
|          NodeMCU          | 288 493 bytes (27%) | 28 492 bytes (34%) |

## Known issues

...

## License

- This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details
- [arduino-base64](https://github.com/adamvr/arduino-base64) licensed under licensed under MIT License
- [arduinolibs](https://github.com/rweather/arduinolibs) licensed under the MIT
- [utf8_check](https://www.cl.cam.ac.uk/~mgk25/ucs/utf8_check.c) licensed under the MIT
