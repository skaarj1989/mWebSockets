# ArduinoWebSockets

Simple to use implementation of WebSocket client and server for Arduino

Autobahn test suite results for [server](https://skaarj1989.github.io/ArduinoWebSocket/autobahn-testsuite/servers/index.html) ... some tests will never pass just because of memory lack in ATmega family.

###### client tests in progress ...

## Table of contents

- [Requirements](#requirements)
- [Installation](#installation)
  * [config.h](#configh)
  * [Physical connection](#physical-connection)
- [Usage example](#usage-example)
  * [Server (Node.js)](#server)
  * [Client (Arduino)](#client)
  * [Chat](#chat)
- [Approx memory usage](#approx-memory-usage)
- [Ethernet "2" library modification](#ethernet-2-library-modification)
  * [Fix for Arduino Mega](#fix-for-arduino-mega)
- [Known issues](#known-issues)
- [License](#license)

## Requirements

* Development board (Confirmed working. Other boards may or may not work):
  * Arduino Uno (ATmega328P)
  * Arduino Mega2560
  * Arduino Pro Mini (ATmega328P)
* Ethernet module or shield (Confirmed working):
  * Arduino Ethernet Shield W5100
  * WizNet W5500 module
  * ENC28j60 module
* Libraries:
  * CryptoLegacy from [arduinolibs](https://github.com/rweather/arduinolibs), you can grab it [here](CryptoLegacy.zip)
  * [Ethernet "2"](https://github.com/adafruit/Ethernet2) for W5500
  * [UIPEthernet](https://github.com/ntruchsess/arduino_uip) for ENC28j60

## Installation

Install this library and CryptoLegacy in Arduino libraries directory, follow [this](https://www.arduino.cc/en/Guide/Libraries) guide if you don't know how to do it.

### config.h

Change below definition if needed:

```cpp
#define ETHERNET_CONTROLLER   W5100
```

Uncomment these if you want additional informations in serial monitor:

```
//#define _DEBUG
//#define _DUMP_HANDSHAKE
//#define _DUMP_FRAME
```

### Physical connection

| W5100 Ethernet shield  | Arduino Uno / Mega2560 |  Arduino Pro Mini
| :---: | :---: | :---: |
| (ICSP) MISO  | (ICSP) MISO  | PIN 12 |
| (ICSP) MOSI  | (ICSP) MOSI  | PIN 11 |
| (ICSP) SCK  | (ICSP) SCK | PIN 13 |
| (SS) PIN 10  | PIN 10 | PIN 10 |

| W5500 / ENC28j60  | Arduino Uno / Pro Mini | Arduino Mega2560 |
| :---: | :---: | :---: | 
| MISO  | PIN 12  | PIN 50 |
| MOSI  | PIN 11  | PIN 51 |
| SCS  | PIN 10  | PIN 53  |
| SCLK  | PIN 13  | PIN 52 |

## Usage example

### Server

```cpp
#include <WebSocketServer.h>

WebSocketServer server(3000);

void onOpen(WebSocket &ws) {
  char message[] = "Hello from Arduino server!";
  ws.send(message, strlen(message));
}

void onClose(WebSocket &ws, eWebSocketCloseEvent code, const char *reason, uint16_t length) { /* ... */ }
void onMessage(WebSocket &ws, const char *message, uint16_t length) { /* */ }

void setup() {
  // Ethernet initialization goes here ...
  // ...

  server.setOnMessageCallback(onMessage);
  server.begin();
  
  client.setOnOpenCallback(onOpen);
  client.setOnCloseCallback(onClose);
  client.setOnMessageCallback(onMessage);
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

void onClose(WebSocket &ws, eWebSocketCloseEvent code, const char *reason, uint16_t length) { /* ... */ }
void onMessage(WebSocket &ws, const char *message, uint16_t length) { /* */ }

void setup() {
  // Ethernet initialization goes here ...
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

Following screenshots shows Rasperry Pi server and clients (browser and Arduino) in action:

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

###### *simple.ino example

### Ethernet.h (W5100)

| Board  | Program space | Dynamic memory |
| :---: | :---: | :---: | 
| Arduino Uno  | 21500 bytes (64%)  | 886 bytes (43%) |
| Arduino Mega2560  | 21754 bytes (8%) | 886 bytes (10%) |
| Arduino Pro Mini | 21500 bytes (69%) | 886 bytes (43%) |

### Ethernet2.h (W5500)

| Board  | Program space | Dynamic memory |
| :---: | :---: | :---: | 
| Arduino Uno  | 21104 bytes (65%)  | 779 bytes (38%) |
| Arduino Mega2560  | 21258 bytes (8%) | 779 bytes (9%) |
| Arduino Pro Mini | 21104 bytes (68%) | 779 bytes (38%) |

### UIPEthernet.h (ENC28j60)

| Board  | Program space | Dynamic memory |
| :---: | :---: | :---: | 
| Arduino Uno  | 29442 bytes (91%)  | 1574 bytes (76%) |
| Arduino Mega2560  | 29658 bytes (11%) | 1574 bytes (19%) |
| Arduino Pro Mini | 29442 bytes (95%) | 1574 bytes (76%) |

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

## Known issues

Currently **UIPEthernet** library causes some problems with **ENC28j60** on Arduino **Mega2560**:
Client can't connect to server but after reset everything is ok (every second reset, which is weird and I don't know reason of this behavior).

Nonetheless on Arduino Uno everything works fine.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details
