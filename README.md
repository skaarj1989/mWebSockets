# ArduinoWebSocketClient

This library allows you to connect Arduino to WebSocket server

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
- [License](#license)

## Requirements

* Development board (Confirmed working. Other boards may or may not work):
  * Arduino Uno (ATmega328P)
  * Arduino Mega2560
  * Arduino Pro Mini (ATmega328P)
* Ethernet module or shield (Confirmed working):
  * Arduino Ethernet Shield W5100
  * WizNet W5500 module
* Library: CryptoLegacy from [arduinolibs](https://github.com/rweather/arduinolibs), you can grab it [here](CryptoLegacy.zip)

*Additionally if you are using W5500 module you will also need [Ethernet "2"](https://github.com/adafruit/Ethernet2) library*

## Installation

Install this library and CryptoLegacy in Arduino libraries directory, which is (for Windows):

```
/Documents/Arduino/libraries
```

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

#### Ethernet shield W5100

| Ethernet shield  | Arduino Uno / Mega2560 |  Arduino Pro Mini
| :---: | :---: | :---: |
| (ICSP) MISO  | (ICSP) MISO  | PIN 12 |
| (ICSP) MOSI  | (ICSP) MOSI  | PIN 11 |
| (ICSP) SCK  | (ICSP) SCK | PIN 13 |
| (SS) PIN 10  | PIN 10 | PIN 10 |

##### ... Or just attach the shield if you can

#### W5500 module

| W5500  | Arduino Uno / Pro Mini | Arduino Mega2560 |
| :---: | :---: | :---: | 
| MISO  | PIN 12  | PIN 50 |
| MOSI  | PIN 11  | PIN 51 |
| SCS  | PIN 10  | PIN 53  |
| SCLK  | PIN 13  | PIN 52 |

## Usage example

### Server

```js
const WebSocket = require('ws');
const wss = new WebSocket.Server({ host: 'ip', port: 3000 });

wss.on('connection', function connection(ws, req) {
  // set callbacks here ...
  
  ws.send('Hello from Node.js!');
});
```

##### More node.js server examples [here](node.js)

### Client

```cpp
#include <WebSocketClient.h>

WebSocketClient client;

void onOpen(WebSocketClient &ws) {
  char message[] = "Hello from Arduino!";
  ws.send(message, strlen(message));
}

void onClose(WebSocketClient &ws, eWebSocketCloseEvent code, const char *reason, uint16_t length) { /* ... */ }

void onMessage(WebSocketClient &ws, const char *message, uint16_t length) { /* */ }

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

## Ethernet "2" library modification

### Fix for Arduino Mega

#### Ethernet2.h

```cpp
#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__)
  EthernetClass() { _dhcp = NULL; w5500_cspin = _10; }
  void init(uint8_t _cspin) { w5500_cspin = _cspin; }
#elif defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
  EthernetClass() { _dhcp = NULL; w5500_cspin = 53; }
  void init(uint8_t _cspin = 53) { w5500_cspin = _cspin; }
#endif
```

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details
