#include <SPI.h>
#include <WebSocketServer.h>

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
WebSocketServer server(3000);

void onOpen(WebSocket &ws) {
  Serial.println(F("Connected"));
  
  char message[] = "Hello from Arduino server!";
  ws.send(TEXT, message, strlen(message), false);
}

void onClose(WebSocket &ws, const eWebSocketCloseEvent code, const char *reason, uint16_t length) {
  Serial.println(F("Disconnected"));
}

void onMessage(WebSocket &ws, const eWebSocketDataType dataType, const char *message, byte length) {
  switch (dataType) {
    case TEXT:
      Serial.print(F("Received: ")); Serial.println(message);
      break;
    case BINARY:
      Serial.println(F("Received binary data"));
      break;
  }
}

void onError(const eWebSocketError code) {
  Serial.print("Error: "); Serial.println(code);
}

void setup() {
  Serial.begin(57600);
  while (!Serial) ;
  
  Serial.println(F("Initializing ... "));
  
  if (Ethernet.begin(mac) == 0) {
    Serial.println(F("Can't open ethernet device"));
    while (true) ;
  }

  Serial.print(F("Client IP: "));
  Serial.println(Ethernet.localIP());

  // ---

  server.setOnOpenCallback(onOpen);
  server.setOnCloseCallback(onClose);
  server.setOnMessageCallback(onMessage);

  server.begin();
}

void loop() {
  server.listen();
}
