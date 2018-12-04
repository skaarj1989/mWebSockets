/*
   1. Install docker (and set shared drives)
   2. Run docker pull crossbario/autobahn-testsuite
   3. Download: https://github.com/crossbario/autobahn-testsuite
   4. Modify ./docker/config/ .json files
      set server ip (this device), agent (name) and cases
   5. Run with admin permissions:
      docker run -it --rm -v %cd%\config:/config \
      -v %cd%\reports:/reports crossbario/autobahn-testsuite \
      -p 9001:9001 wstest -m fuzzingserver \
      -s /config/fuzzingserver.json
   6. Write to serial (for each case):
      <192.168.46.9,9001,/runCase?case=1&agent=Mega2560>
   7. Write to serial (to generate report):
      <192.168.46.9,9001,/updateReports?agent=Mega2560>
   8. See client-auto.js for automation
*/

#define _USE_SERIAL_OUTPUT

#ifdef _USE_SERIAL_OUTPUT
# include <SPI.h>
#endif
#include <WebSocketClient.h>

#if PLATFORM_ARCH == PLATFORM_ARCHITECTURE_SAMD21
# define CONSOLE SerialUSB
#else
# define CONSOLE Serial
#endif

#define BUFFER_SIZE 64
char buffer[BUFFER_SIZE] = { '\0' };
char tempBuffer[BUFFER_SIZE] = { '\0' };

bool newData = false;

char host[32] = { '\0' };
char path[48] = { '\0' };
int port = 3000;

#if NETWORK_CONTROLLER == NETWORK_CONTROLLER_WIFI
const char *SSID = "SKYNET";
const char *password = "***";
#else
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 46, 177);
#endif

WebSocketClient client;

//
// Callbacks:
//

void onOpen(WebSocket &ws) {
  digitalWrite(LED_BUILTIN, HIGH);
}

void onMessage(WebSocket &ws, const eWebSocketDataType dataType, const char *message, uint16_t length) {
  ws.send(dataType, message, length);
}

void onClose(WebSocket &ws, const eWebSocketCloseEvent code, const char *reason, uint16_t length) {
#ifdef _USE_SERIAL_OUTPUT
  CONSOLE.println(F("Disconnected\n"));
#endif

  digitalWrite(LED_BUILTIN, LOW);
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

#ifdef _USE_SERIAL_OUTPUT
  CONSOLE.begin(115200);
  while (!CONSOLE) ;
#endif

#if NETWORK_CONTROLLER == NETWORK_CONTROLLER_WIFI
# ifdef _USE_SERIAL_OUTPUT
  //CONSOLE.setDebugOutput(true);
  CONSOLE.printf("\nConnecting to %s ", SSID);
# endif

  WiFi.begin(SSID, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
# ifdef _USE_SERIAL_OUTPUT
    CONSOLE.print(".");
# endif
  }

# ifdef _USE_SERIAL_OUTPUT
  CONSOLE.println(" connected");

  CONSOLE.print(F("Device IP: "));
  CONSOLE.println(WiFi.localIP());
  WiFi.printDiag(Serial);
# endif
#else // ethernet
# ifdef _USE_SERIAL_OUTPUT
  CONSOLE.println(F("Initializing ... "));
# endif

 Ethernet.begin(mac, ip);

# ifdef _USE_SERIAL_OUTPUT
  CONSOLE.print(F("Device IP: "));
  CONSOLE.println(Ethernet.localIP());
# endif
#endif

  // ---

  client.setOnOpenCallback(onOpen);
  client.setOnCloseCallback(onClose);
  client.setOnMessageCallback(onMessage);

  digitalWrite(LED_BUILTIN, HIGH);
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
        buffer[idx++] = c;

        if (idx >= BUFFER_SIZE)
          idx = BUFFER_SIZE - 1;
      }
      else {
        buffer[idx] = '\0';
        recvInProgress = false;
        idx = 0;
        newData = true;
      }
    }
    else if (c == '<')
      recvInProgress = true;
  }

  if (newData == true) {
    strcpy(tempBuffer, buffer);

    char *ptr = strtok(tempBuffer, ",");
    strcpy(host, ptr);

    ptr = strtok(NULL, ",");
    port = atoi(ptr);

    ptr = strtok(NULL, ",");
    strcpy(path, ptr);

    CONSOLE.print("Performing test: ");
    CONSOLE.println(path);

    if (!client.open(host, port, path)) {
#ifdef _USE_SERIAL_OUTPUT
      //CONSOLE.println("Connection failed!");
#endif
    }

    newData = false;
  }

  client.listen();
}
