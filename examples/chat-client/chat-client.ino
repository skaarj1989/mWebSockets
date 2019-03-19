#include <WebSocketClient.h>
using namespace net;

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

#define BUFFER_SIZE 64
char message[BUFFER_SIZE] = { '\0' };
bool newData = false;

WebSocketClient client;

void onOpen(WebSocket &ws) {
  CONSOLE.println(F("Type message in following format: <text>"));
  CONSOLE.println(F("----------------------------------------"));
}

void onMessage(WebSocket &ws, const WebSocketDataType dataType, const char *message, uint16_t length) {
  CONSOLE.println(message);
}

void onClose(WebSocket &ws, const WebSocketCloseCode code, const char *reason, uint16_t length) {
  CONSOLE.println(F("Disconnected"));
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

  CONSOLE.print(F("Device IP: "));
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

  CONSOLE.print(F("Device IP: "));
  CONSOLE.println(Ethernet.localIP());
#endif

  // ---

  client.setOnOpenCallback(onOpen);
  client.setOnMessageCallback(onMessage);
  client.setOnCloseCallback(onClose);

  if (!client.open("192.168.46.9", 3000)) {
    CONSOLE.println(F("Connection failed!"));
    while (true) ;
  }
}

unsigned long previousTime = 0;

void loop() {
  static bool recvInProgress = false;
  static byte idx = 0;
  
  char c;

  while (CONSOLE.available() > 0 && newData == false) {
    c = CONSOLE.read();

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
