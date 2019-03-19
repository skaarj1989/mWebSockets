#include <WebSocketServer.h>

#if PLATFORM_ARCH == PLATFORM_ARCHITECTURE_SAMD21
# define CONSOLE SerialUSB
#else
# define CONSOLE Serial
#endif

#if NETWORK_CONTROLLER == NETWORK_CONTROLLER_WIFI
const char *SSID = "SKYNET";
const char *password = "***";
#else
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
#endif

WebSocketServer server(3000);

void onOpen(WebSocket &ws) {
  CONSOLE.println(F("Connected"));
  
  char message[] = "Hello from Arduino server!";
  ws.send(TEXT, message, strlen(message));
}

void onClose(WebSocket &ws, const WebSocketCloseCode code, const char *reason, uint16_t length) {
  CONSOLE.println(F("Disconnected"));
}

void onMessage(WebSocket &ws, const WebSocketDataType dataType, const char *message, uint16_t length) {
  switch (dataType) {
    case TEXT:
      CONSOLE.print(F("Received: ")); CONSOLE.println(message);
      break;
    case BINARY:
      CONSOLE.println(F("Received binary data"));
      break;
  }
}

void onError(const WebSocketError code) {
  CONSOLE.print("Error: "); CONSOLE.println(code);
}

void setup() {
  CONSOLE.begin(115200);
  while (!CONSOLE) ;
  
#if NETWORK_CONTROLLER == NETWORK_CONTROLLER_WIFI
  CONSOLE.printf("\nConnecting to %s ", SSID);

  WiFi.begin(SSID, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    CONSOLE.print(".");
  }

  CONSOLE.println(" connected");

  CONSOLE.print(F("Server IP: "));
  CONSOLE.println(WiFi.localIP());
  //WiFi.printDiag(CONSOLE);
#else
  CONSOLE.println(F("Initializing ... "));

  // For official Arduino Ethernet library specify CS pin
  //Ethernet.init(53);

  if (Ethernet.begin(mac) == 0) {
    CONSOLE.println(F("Can't open ethernet device"));
    while (true) ;
  }

  CONSOLE.print(F("Server IP: "));
  CONSOLE.println(Ethernet.localIP());
#endif

  // ---

	//server.setOnErrorCallback(onError);
  server.setOnOpenCallback(onOpen);
  server.setOnCloseCallback(onClose);
  server.setOnMessageCallback(onMessage);

  server.begin();
}

void loop() {
  server.listen();
}
