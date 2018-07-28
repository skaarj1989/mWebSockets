#ifndef __WEBSOCKETCLIENT_CONFIG_DOT_H_INCLUDED_
#define __WEBSOCKETCLIENT_CONFIG_DOT_H_INCLUDED_

//
//
//

#define _DEBUG
#define _DUMP_HANDSHAKE
#define _DUMP_HEADER
#define _DUMP_FRAME_DATA

//
//
//

#define TIMEOUT_INTERVAL  5000
#define BUFFER_MAX_SIZE		512

//
//
//

#define _USE_ETHERNET
//#define _USE_WIFI

#ifdef _USE_ETHERNET
# define W5100			1
# define W5500			2
# define ENC28J60		3

# define ETHERNET_CONTROLLER		W5100
#endif

#endif