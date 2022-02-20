#include "NetworkControllers.hpp"
#define NETWORK_CONTROLLER ETHERNET_CONTROLLER_W5X00

#if NETWORK_CONTROLLER == NETWORK_CONTROLLER_WIFI
const char kSSID[]{"SKYNET"};
const char kPassword[]{"***"};
#else
byte mac[]{0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
const IPAddress ip{192, 168, 46, 180};
#endif

#include "ExampleBoilerplate.hpp"

#include <WebSocketClient.hpp>
using namespace net;

using MyWebSocket = WebSocket<NetClient>;
using MyWebSocketClient = WebSocketClient<NetClient>;

MyWebSocketClient client;

void setup() {
  _SERIAL.begin(115200);
  while (!_SERIAL)
    ;

  setupNetwork();

  client.onOpen([](MyWebSocket &ws) {
    _SERIAL.println(F("Connected"));
    const auto protocol = ws.getProtocol();
    if (protocol) {
      _SERIAL.print(F("Client protocol: "));
      _SERIAL.println(protocol);
    }

    const char message[]{"Hello from Arduino client!"};
    ws.send(WebSocketDataType::TEXT, message, strlen(message));
  });

  client.onMessage([](MyWebSocket &ws, const WebSocketDataType dataType,
                      const char *message, uint16_t length) {
    switch (dataType) {
    case WebSocketDataType::TEXT:
      _SERIAL.print(F("Received: "));
      _SERIAL.println(message);
      break;
    case WebSocketDataType::BINARY:
      _SERIAL.println(F("Received binary data"));
      break;
    }

    ws.send(dataType, message, length); // Echo back to server
  });

  client.onClose(
      [](MyWebSocket &, const WebSocketCloseCode, const char *, uint16_t) {
        _SERIAL.println(F("Disconnected\n"));
      });

  if (!client.open("192.168.46.4", 3000, "/")) {
    _SERIAL.println(F("Connection failed!"));
    while (true)
      ;
  }
}

void loop() { client.listen(); }
