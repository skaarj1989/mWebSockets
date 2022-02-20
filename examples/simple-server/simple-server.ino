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

#include <WebSocketServer.hpp>
using namespace net;

using MyWebSocket = WebSocket<NetClient>;
using MyWebSocketServer = WebSocketServer<NetServer, NetClient, 4>;

constexpr auto kPort = 3000;
MyWebSocketServer wss{kPort};

void setup() {
  _SERIAL.begin(115200);
  while (!_SERIAL)
    ;

  setupNetwork();

  _SERIAL.print(F("Server running on port: "));
  _SERIAL.println(kPort);

  wss.onConnection([](MyWebSocket &ws) {
    const auto protocol = ws.getProtocol();
    if (protocol) {
      _SERIAL.print(F("Client protocol: "));
      _SERIAL.println(protocol);
    }

    ws.onMessage([](MyWebSocket &ws, const WebSocketDataType dataType,
                    const char *message, uint16_t length) {
      switch (dataType) {
      case net::WebSocketDataType::TEXT:
        _SERIAL.print(F("Received: "));
        _SERIAL.println(message);
        break;
      case net::WebSocketDataType::BINARY:
        _SERIAL.println(F("Received binary data"));
        break;
      }

      ws.send(dataType, message, length);
    });

    ws.onClose(
        [](MyWebSocket &, const WebSocketCloseCode, const char *, uint16_t) {
          _SERIAL.println(F("Disconnected"));
        });

    _SERIAL.print(F("New client: "));
    _SERIAL.println(ws.getRemoteIP());

    const char message[]{"Hello from Arduino server!"};
    ws.send(WebSocketDataType::TEXT, message, strlen(message));
  });

  wss.begin();
}

void loop() { wss.listen(); }
