#include <SPI.h>
#include <WebSocketClient.h>

#define BUFFER_SIZE 64
char message[BUFFER_SIZE] = { '\0' };
bool newData = false;

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
WebSocketClient client;

void onOpen(WebSocketClient &ws) {
  Serial.println(F("Type message in following format: <text>"));
  Serial.println(F("----------------------------------------"));
}

void onMessage(WebSocketClient &ws, const eWebSocketDataType dataType, const char *message, uint16_t length) {
  Serial.println(message);
}

void onClose(WebSocketClient &ws) {
  Serial.println(F("Disconnected"));
}

void setup() {
  Serial.begin(57600);
  while (!Serial) ;

  Serial.println("Initializing ...");

  if (Ethernet.begin(mac) == 0) {
    Serial.println(F("Can't open ethernet device!"));
    while (true) ;
  }

  Serial.print(F("Client IP: "));
  Serial.println(Ethernet.localIP());

  // ---

  client.setOnOpenCallback(onOpen);
  client.setOnMessageCallback(onMessage);
  client.setOnCloseCallback(onClose);

  if (!client.open("192.168.46.10", 3000)) {
    Serial.println(F("Connection failed!"));
    while (true) ;
  }
}

unsigned long previousTime = 0;

void loop() {
  static bool recvInProgress = false;
  static byte idx = 0;
  
  char c;

  while (Serial.available() > 0 && newData == false) {
    c = Serial.read();

    if (recvInProgress == true) {
      if (c != '>') {
        message[idx++] = c;

        if (idx >= BUFFER_SIZE)
          idx = BUFFER_SIZE - 1;
      }
      else {
        message[idx] = '\0';
        recvInProgress = false;
        idx = 0;
        newData = true;
      }
    }
    else if (c == '<')
      recvInProgress = true;
  }

  if (newData == true) {
    client.send(TEXT, message, strlen(message));
    newData = false;
  }

  client.listen();

  unsigned long currentTime = millis();
  if (currentTime - previousTime > 1000) {
    previousTime = currentTime;
    
    client.ping();
  }
}
