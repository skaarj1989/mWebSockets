#include <WebSocketClient.h>
using namespace net;

#if PLATFORM_ARCH == PLATFORM_ARCHITECTURE_SAMD21
#  define _SERIAL SerialUSB
#else
#  define _SERIAL Serial
#endif

#if NETWORK_CONTROLLER == NETWORK_CONTROLLER_WIFI
constexpr char kSSID[]{"SKYNET"};
constexpr char kPassword[]{"***"};
#else
byte mac[]{0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
// IPAddress ip(192, 168, 46, 179);
#endif

WebSocketClient client;

void setup() {
  _SERIAL.begin(115200);
  while (!_SERIAL)
    ;

#if NETWORK_CONTROLLER == NETWORK_CONTROLLER_WIFI
  //_SERIAL.setDebugOutput(true);
  _SERIAL.printf("\nConnecting to %s ", kSSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(kSSID, kPassword);
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
    _SERIAL.println(F("Connected"));
    const auto protocol = ws.getProtocol();
    if (protocol) {
      _SERIAL.print(F("Client protocol: "));
      _SERIAL.println(protocol);
    }

    const char message[]{"Hello from Arduino client!"};
    ws.send(WebSocket::DataType::TEXT, message, strlen(message));
  });

  client.onMessage([](WebSocket &ws, const WebSocket::DataType dataType,
                     const char *message, uint16_t length) {
    switch (dataType) {
    case WebSocket::DataType::TEXT:
      _SERIAL.print(F("Received: "));
      _SERIAL.println(message);
      break;
    case WebSocket::DataType::BINARY:
      _SERIAL.println(F("Received binary data"));
      break;
    }

    ws.send(dataType, message, length); // Echo back to server
  });

  client.onClose([](WebSocket &, const WebSocket::CloseCode, const char *,
                   uint16_t) { _SERIAL.println(F("Disconnected\n")); });

  if (!client.open("192.168.46.31", 3000, "/")) {
    _SERIAL.println(F("Connection failed!"));
    while (true)
      ;
  }
}

void loop() { client.listen(); }
