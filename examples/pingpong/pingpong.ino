#include <SPI.h>
#include <WebSocketClient.h>

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
WebSocketClient client;

void onOpen(WebSocketClient &ws) {
  char message[] = "hello from arduino";
  ws.send(message, strlen(message));
}

void onClose(WebSocketClient &ws) {
  Serial.println("onclose");
}

void onMessage(WebSocketClient &ws, const char *message, byte length) {
  Serial.print(F("Received: ")); Serial.println(message);
}

void setup() {
  Serial.begin(57600);
  Serial.print(F("Initializing ... "));
	
  if (Ethernet.begin(mac) == 0) {
    Serial.println(F("can't open ethernet device"));
    for ( ; ; ) ;
  }
	
  Serial.println(F("ok"));

  Serial.print(F("Client IP: "));
  Serial.println(Ethernet.localIP());

  client.setOnOpenCallback(onOpen);
  client.setOnCloseCallback(onClose);
  client.setOnMessageCallback(onMessage);
	
  if (!client.open("echo.websocket.org")) {
    Serial.println(F("Connection failed!"));
    for ( ; ; ) ;
  }

  Serial.println(F("Connected"));
}

unsigned long previousTime = 0;

void loop() {
  client.listen();

  unsigned long currentTime = millis();
  if (currentTime - previousTime > 10000) {
    previousTime = currentTime;

    client.ping();
  }
}
