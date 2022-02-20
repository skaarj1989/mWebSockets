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

constexpr auto kBufferSize = 64;
char message[kBufferSize]{};

using MyWebSocket = WebSocket<NetClient>;
using MyWebSocketClient = WebSocketClient<NetClient>;

MyWebSocketClient client;

void setup() {
  _SERIAL.begin(115200);
  while (!_SERIAL)
    ;

  setupNetwork();

  client.onOpen([](MyWebSocket &ws) {
    _SERIAL.println(F("Type a message in the following format: <text>"));
    _SERIAL.println(F("----------------------------------------------"));
  });

  client.onMessage([](MyWebSocket &ws, const WebSocketDataType,
                      const char *message, uint16_t) {
    _SERIAL.println(message);
  });
  client.onClose(
      [](MyWebSocket &, const WebSocketCloseCode, const char *, uint16_t) {
        _SERIAL.println(F("Disconnected"));
      });

  if (!client.open("192.168.46.4", 3000)) {
    _SERIAL.println(F("Connection failed!"));
    while (true)
      ;
  }
}

uint32_t previousTime{0};

void loop() {
  static auto recvInProgress = false;
  static byte idx{0};

  static auto newData = false;
  while (_SERIAL.available() > 0 && newData == false) {
    const auto c = _SERIAL.read();

    if (recvInProgress) {
      if (c != '>') {
        message[idx++] = c;

        if (idx >= kBufferSize) idx = kBufferSize - 1;
      } else {
        message[idx] = '\0';
        recvInProgress = false;
        idx = 0;
        newData = true;
      }
    } else if (c == '<')
      recvInProgress = true;
  }

  if (newData) {
    client.send(WebSocketDataType::TEXT, message, strlen(message));
    newData = false;
  }

  client.listen();

  uint32_t currentTime = millis();
  if (currentTime - previousTime > 1000) {
    previousTime = currentTime;

    client.ping();
  }
}
