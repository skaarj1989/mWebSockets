#include <SPI.h>
#include <WebSocketClient.h>

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
WebSocketClient client;

void setup() {
  Serial.begin(57600);
  if (Ethernet.begin(mac) == 0) {
    Serial.println(F("Can't open ethernet device"));
    for ( ; ; ) ;
  }

  Serial.print(F("Client IP: "));
  Serial.println(Ethernet.localIP());

  //client.open("echo.websocket.org");
  client.open("192.168.46.10", 3000);
}

void loop() {
  client.listen();
}
