#ifndef __WEBSOCKETS_PLATFORM_DOT_H_INCLUDED_
#define __WEBSOCKETS_PLATFORM_DOT_H_INCLUDED_

//
// Board architectures:
//

#define PLATFORM_ARCHITECTURE_AVR 1
#define PLATFORM_ARCHITECTURE_ESP8266 2
#define PLATFORM_ARCHITECTURE_SAMD21 3

#if defined(__AVR__)
# define PLATFORM_ARCH PLATFORM_ARCHITECTURE_AVR
#elif defined(ESP8266)
# define PLATFORM_ARCH PLATFORM_ARCHITECTURE_ESP8266
#elif defined(ARDUINO_SAMD_ZERO)
# define PLATFORM_ARCH PLATFORM_ARCHITECTURE_SAMD21
#endif

//
// Ethernet devices:
//

#define ETHERNET_CONTROLLER_W5100 1
#define ETHERNET_CONTROLLER_W5500 2
#define ETHERNET_CONTROLLER_ENC28J60 3
#define NETWORK_CONTROLLER_WIFI 4

#include "config.h"

#if NETWORK_CONTROLLER == ETHERNET_CONTROLLER_W5100
# include <Ethernet.h>
# define MAX_CONNECTIONS 4
#elif NETWORK_CONTROLLER == ETHERNET_CONTROLLER_W5500
# include <Ethernet2.h>
# define MAX_CONNECTIONS 8
#elif NETWORK_CONTROLLER == ETHERNET_CONTROLLER_ENC28J60
# include <UIPEthernet.h>
# define MAX_CONNECTIONS 4
#elif NETWORK_CONTROLLER == NETWORK_CONTROLLER_WIFI
# if PLATFORM_ARCH == PLATFORM_ARCHITECTURE_ESP8266
#  include <ESP8266WiFi.h>
# endif
# include <WiFiClient.h>
# include <WiFiServer.h>
# define MAX_CONNECTIONS 8
#else
# error "Network controller is required!"
#endif

#endif