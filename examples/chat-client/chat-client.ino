#include <WebSocketClient.h>
using namespace net;

#if PLATFORM_ARCH == PLATFORM_ARCHITECTURE_SAMD21
#  define _SERIAL SerialUSB
#else
#  define _SERIAL Serial
#endif

#if NETWORK_CONTROLLER == NETWORK_CONTROLLER_WIFI
const char SSID[]{"SKYNET"};
const char password[]{"***"};
#else
byte mac[]{0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
// IPAddress ip(192, 168, 46, 179);
#endif

constexpr auto kBufferSize = 64;
char message[kBufferSize]{};

WebSocketClient client;

void setup() {
  _SERIAL.begin(115200);
  while (!_SERIAL)
    ;

#if NETWORK_CONTROLLER == NETWORK_CONTROLLER_WIFI
  //_SERIAL.setDebugOutput(true);
  _SERIAL.printf("\nConnecting to %s ", SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    _SERIAL.print(F("."));
  }

  _SERIAL.println(F(" connected"));

  WiFi.printDiag(_SERIAL);

  _SERIAL.print(F("Device IP: "));
  _SERIAL.println(WiFi.localIP());
#else
  _SERIAL.println(F("Initializing ... "));

  // Ethernet.init(10);
  Ethernet.init(53); // Mega2560
  // Ethernet.init(5); // ESPDUINO-32
  // Ethernet.init(PA4); // STM32

  Ethernet.begin(mac); //, ip);

  _SERIAL.print(F("Device IP: "));
  _SERIAL.println(Ethernet.localIP());
#endif

  client.onOpen([](WebSocket &ws) {
    _SERIAL.println(F("Type a message in the following format: <text>"));
    _SERIAL.println(F("----------------------------------------------"));
  });

  client.onMessage(
    [](WebSocket &ws, const WebSocket::DataType, const char *message,
      uint16_t) { _SERIAL.println(message); });
  client.onClose([](WebSocket &, const WebSocket::CloseCode, const char *,
                   uint16_t) { _SERIAL.println(F("Disconnected")); });

  if (!client.open("192.168.46.31", 3000)) {
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
    client.send(WebSocket::DataType::TEXT, message, strlen(message));
    newData = false;
  }

  client.listen();

  uint32_t currentTime = millis();
  if (currentTime - previousTime > 1000) {
    previousTime = currentTime;

    client.ping();
  }
}
