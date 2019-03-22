#include <WebSocketClient.h>
using namespace net;

#if PLATFORM_ARCH == PLATFORM_ARCHITECTURE_SAMD21
# define _SERIAL SerialUSB
#else
# define _SERIAL Serial
#endif

#if NETWORK_CONTROLLER == NETWORK_CONTROLLER_WIFI
const char *SSID = "SKYNET";
const char *password = "***";
#else
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
//IPAddress ip(192, 168, 46, 179);
#endif

WebSocketClient client;

void setup() {
  _SERIAL.begin(115200);
  while (!_SERIAL);

#if NETWORK_CONTROLLER == NETWORK_CONTROLLER_WIFI
  //_SERIAL.setDebugOutput(true);
  _SERIAL.printf("\nConnecting to %s ", SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); _SERIAL.print(F("."));
  }

  _SERIAL.println(F(" connected"));

  WiFi.printDiag(_SERIAL);

  _SERIAL.print(F("Device IP: "));
  _SERIAL.println(WiFi.localIP());
#else
  _SERIAL.println(F("Initializing ... "));

# if NETWORK_CONTROLLER == ETHERNET_CONTROLLER_W5100
  //Ethernet.init(53);
# endif

  Ethernet.begin(mac); //, ip);

  _SERIAL.print(F("Device IP: "));
  _SERIAL.println(Ethernet.localIP());
#endif

  client.onOpen([](WebSocket &ws) {
    _SERIAL.println(F("Connected"));

    char message[] = "Hello from Arduino client!";
    ws.send(TEXT, message, strlen(message));
  });

  client.onMessage([](WebSocket &ws, WebSocketDataType dataType, const char *message, uint16_t length) {
    switch (dataType) {
      case TEXT:
        _SERIAL.print(F("Received: ")); _SERIAL.println(message);
        break;
      case BINARY:
        _SERIAL.println(F("Received binary data"));
        break;
    }

    // echo back to server
    ws.send(dataType, message, length);
  });

  client.onClose([](WebSocket &ws, const WebSocketCloseCode code, const char *reason, uint16_t length) {
    _SERIAL.println(F("Disconnected\n"));
  });

  if (!client.open("192.168.46.9", 3000)) {
    _SERIAL.println(F("Connection failed!"));
    while (true) ;
  }
}

void loop() {
  client.listen();
}
