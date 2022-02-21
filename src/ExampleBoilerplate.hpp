#pragma once

// This file should be located inside "examples" directory.
// Unfortunately Arduino does not allow to include a file from parent path

#include "Platform.hpp"

#if PLATFORM_ARCH == PLATFORM_ARCHITECTURE_SAMD21
#  define _SERIAL SerialUSB
#else
#  define _SERIAL Serial
#endif

#if NETWORK_CONTROLLER == ETHERNET_CONTROLLER_W5X00
#  include <Ethernet.h>
#elif NETWORK_CONTROLLER == ETHERNET_CONTROLLER_ENC28J60
#  include <EthernetENC.h>
#elif NETWORK_CONTROLLER == NETWORK_CONTROLLER_WIFI
#  if PLATFORM_ARCH == PLATFORM_ARCHITECTURE_ESP8266
#    include <ESP8266WiFi.h>
#  else
#    include <WiFi.h>
#  endif
#  include <WiFiClient.h>
#  include <WiFiServer.h>
#else
#  error Network controller required!
#endif

#if NETWORK_CONTROLLER == NETWORK_CONTROLLER_WIFI
using NetClient = WiFiClient;
using NetServer = WiFiServer;
#else
using NetClient = EthernetClient;
using NetServer = EthernetServer;
#endif

template <typename NetClient> IPAddress fetchRemoteIp(const NetClient &client) {
#if (PLATFORM_ARCH == PLATFORM_ARCHITECTURE_ESP8266) &&                        \
    (NETWORK_CONTROLLER == ETHERNET_CONTROLLER_W5X00)
  // EthernetClient class in ESP8266 doesn't implement remoteIP()
  return {};
#else
  // EthernetClient class is not "const correct"
  return const_cast<NetClient &>(client).remoteIP();
#endif
}

inline void setupNetwork() {
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

  Ethernet.init();
  // Ethernet.init(10);
  // Ethernet.init(53); // Mega2560
  // Ethernet.init(5); // ESPDUINO-32
  // Ethernet.init(PA4); // STM32

  Ethernet.begin(mac); //, ip);

  _SERIAL.print(F("Device IP: "));
  _SERIAL.println(Ethernet.localIP());
#endif
}
