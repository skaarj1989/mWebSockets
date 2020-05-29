#include "WebSocketServer.h"

namespace net {

WebSocketServer::WebSocketServer(uint16_t port) : m_server(port) {}
WebSocketServer::~WebSocketServer() { shutdown(); }

void WebSocketServer::begin(const verifyClientCallback &callback) {
  _verifyClient = callback;
  m_server.begin();
}

void WebSocketServer::shutdown() {
  const auto end = &m_sockets[MAX_CONNECTIONS];
  for (auto it = &m_sockets[0]; it != end; ++it) {
    if (*it) {
      (*it)->close(WebSocket::CloseCode::GOING_AWAY, true);
      SAFE_DELETE(*it);
    }
  }
}

void WebSocketServer::broadcast(
  const WebSocket::DataType &dataType, const char *message, uint16_t length) {
  const auto end = &m_sockets[MAX_CONNECTIONS];
  for (auto it = &m_sockets[0]; it != end; ++it) {
    if (!(*it) || (*it)->getReadyState() != WebSocket::ReadyState::OPEN)
      continue;

    (*it)->send(dataType, message, length);
  }
}

void WebSocketServer::listen() {
  _cleanDeadConnections();

#if NETWORK_CONTROLLER == NETWORK_CONTROLLER_WIFI
  const auto end = &m_sockets[MAX_CONNECTIONS];

  if (m_server.hasClient()) {
    WiFiClient client;
    WebSocket *ws = nullptr;

    for (auto it = &m_sockets[0]; it != end; ++it) {
      if (!(*it)) {
        client = m_server.available();
        if (_handleRequest(client)) {
          *it = new WebSocket(client);
          ws = *it;
          if (_onConnection) _onConnection(*ws);
        }
        break;
      }
    }

     // Server is full ...
    if (!ws) _rejectRequest(client, WebSocketError::SERVICE_UNAVAILABLE);
  }

  for (auto it = &m_sockets[0]; it != end; ++it) {
    if (*it && (*it)->m_client.connected()) {
      if ((*it)->m_client.available()) (*it)->_handleFrame();
    }
  }
#else
  EthernetClient client = m_server.available();
  if (client && client.available()) {
    WebSocket *ws = _getWebSocket(client);

    if (!ws) {
      const auto end = &m_sockets[MAX_CONNECTIONS];
      for (auto it = &m_sockets[0]; it != end; ++it) {
        if (!(*it)) {
          if (_handleRequest(client)) {
            *it = new WebSocket(client);
            ws = *it;
            if (_onConnection) _onConnection(*ws);
          }
          break;
        }
      }

      // Server is full ...
      if (!ws) _rejectRequest(client, WebSocketError::SERVICE_UNAVAILABLE);
    }

    if (ws) ws->_handleFrame();
  }
#endif
}

uint8_t WebSocketServer::countClients() {
  uint8_t count = 0;
  const auto end = &m_sockets[MAX_CONNECTIONS];
  for (auto it = &m_sockets[0]; it != end; ++it)
    if (*it && (*it)->isAlive()) count++;

  return count;
}

void WebSocketServer::onConnection(const onConnectionCallback &callback) {
  _onConnection = callback;
}

WebSocket *WebSocketServer::_getWebSocket(NetClient &client) const {
  const auto end = &m_sockets[MAX_CONNECTIONS];
  for (auto it = &m_sockets[0]; it != end; ++it)
    if (*it && (*it)->m_client == client) return *it;

  return nullptr;
}

bool WebSocketServer::_handleRequest(NetClient &client) {
  uint8_t flags = 0x0;

  int16_t bite = -1;
  byte currentLine = 0;
  byte counter = 0;

  //
  // Read client request:
  //
  // [1] GET /chat HTTP/1.1
  // [2] Host: example.com:8000
  // [3] Upgrade: websocket
  // [4] Connection: Upgrade
  // [5] Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==
  // [6] Sec-WebSocket-Version: 13
  // [7]
  //

  // buffer large enought to hold longest header field
  // Chrome: 'User-Agent' = ~126 characters
  // Firefox: 'User-Agent' = ~90 characters
  // Edge: 'User-Agent' = ~141 characters
  char buffer[160]{};
  char secKey[32]{};

#if NETWORK_CONTROLLER == NETWORK_CONTROLLER_WIFI
  while (!client.available()) {
    delay(10);
    __debugOutput(F("."));
  }
#endif

  while ((bite = client.read()) != -1) {
    buffer[counter++] = bite;

    if (static_cast<char>(bite) == '\n') {
      uint8_t lineBreakPos = strcspn(buffer, "\r\n");
      buffer[lineBreakPos] = '\0';

#ifdef _DUMP_HANDSHAKE
      printf(F("[Line #%d] %s\n"), currentLine, buffer);
#endif

      //
      // [1] GET method
      //

      if (currentLine == 0) {
        char *pch = strtok(buffer, " ");
        for (byte i = 0; pch != nullptr; i++) {
          switch (i) {
          case 0: {
            if (strcmp_P(pch, (PGM_P)F("GET")) != 0) {
              _rejectRequest(client, WebSocketError::BAD_REQUEST);
              return false;
            }

            break;
          }
          case 1: {
            break; // path doesn't matter ...
          }
          case 2: {
            if (strcmp_P(pch, (PGM_P)F("HTTP/1.1")) != 0) {
              _rejectRequest(client, WebSocketError::BAD_REQUEST);
              return false;
            }

            break;
          }
          default: {
            _rejectRequest(client, WebSocketError::BAD_REQUEST);
            return false;
          }
          }

          pch = strtok(nullptr, " ");
        }
      } else {
        if (lineBreakPos > 0) {
          char *header = strtok(buffer, ":");
          char *value = nullptr;

          //
          // [2] Host header:
          //

          if (strcmp_P(header, (PGM_P)F("Host")) == 0) {
            value = strtok(nullptr, " ");

            // #TODO ... or not
          }

          //
          // [3] Upgrade header:
          //

          else if (strcmp_P(header, (PGM_P)F("Upgrade")) == 0) {
            value = strtok(nullptr, " ");

            if (strcasecmp_P(value, (PGM_P)F("websocket")) != 0) {
              _rejectRequest(client, WebSocketError::BAD_REQUEST);
              return false;
            } else
              flags |= kValidUpgradeHeader;
          }

          //
          // [4] Connection header:
          //

          else if (strcmp_P(header, (PGM_P)F("Connection")) == 0) {
            // Firefox sends: "Connection: keep-alive, Upgrade"
            // simple "includes" check:
            while (value = strtok(nullptr, " ,")) {
              if (strstr_P(value, (PGM_P)F("Upgrade")) != nullptr) {
                flags != kValidConnectionHeader;
                break;
              }
            }
          }

          //
          // [5] Sec-WebSocket-Key header:
          //

          else if (strcmp_P(header, (PGM_P)F("Sec-WebSocket-Key")) == 0) {
            value = strtok(nullptr, " ");
            strcpy(secKey, value);
            //flags |= kValidSecKey;
          }

          //
          // [6] Sec-WebSocket-Version header:
          //

          else if (strcmp_P(header, (PGM_P)F("Sec-WebSocket-Version")) == 0) {
            value = strtok(nullptr, " ");
            uint8_t version = atoi(value);

            switch (version) {
            case 13: {
              flags |= kValidVersion;
              break;
            }
            default: {
              _rejectRequest(client, WebSocketError::BAD_REQUEST);
              return false;
            }
            }
          }

          //
          // [ ] Other headers
          //

          else {
            if (_verifyClient) {
              if (!_verifyClient(_REMOTE_IP(client), header, value)) {
                _rejectRequest(client, WebSocketError::CONNECTION_REFUSED);
                return false;
              }
            }
          }
        }

        //
        // [7] Empty line (end of request)
        //

        else {
          if (!(flags & (kValidUpgradeHeader | kValidConnectionHeader))) {
            _rejectRequest(client, WebSocketError::UPGRADE_REQUIRED);
            return false;
          }

          if (!(flags & kValidVersion) || strlen(secKey) == 0) {
            _rejectRequest(client, WebSocketError::BAD_REQUEST);
            return false;
          }

          //
          // Send response:
          //
          // [1] HTTP/1.1 101 Switching Protocols
          // [2] Upgrade: websocket
          // [3] Connection: Upgrade
          // [4] Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=
          // [5]
          //

          char acceptKey[32]{};
          encodeSecKey(acceptKey, secKey);

          char secWebSocketAccept[64]{};
          strcpy_P(secWebSocketAccept, (PGM_P)F("Sec-WebSocket-Accept: "));
          strcat(secWebSocketAccept, acceptKey);

          client.println(F("HTTP/1.1 101 Switching Protocols"));
          client.println(F("Upgrade: websocket"));
          client.println(F("Connection: Upgrade"));
          client.println(secWebSocketAccept);
          client.println();

          return true;
        }
      }

      memset(buffer, '\0', sizeof(buffer));
      counter = 0;
      currentLine++;
    }
  }

  _rejectRequest(client, WebSocketError::BAD_REQUEST);
  return false;
}

void WebSocketServer::_rejectRequest(
  NetClient &client, const WebSocketError &code) {
  switch (code) {
  case WebSocketError::CONNECTION_REFUSED: {
    client.println(F("HTTP/1.1 111 Connection refused"));
    break;
  }
  case WebSocketError::BAD_REQUEST: {
    client.println(F("HTTP/1.1 400 Bad Request"));
    break;
  }
  case WebSocketError::UPGRADE_REQUIRED: {
    client.println(F("HTTP/1.1 426 Upgrade Required"));
    break;
  }
  case WebSocketError::SERVICE_UNAVAILABLE: {
    client.println(F("HTTP/1.1 503 Service Unavailable"));
    break;
  }
  default: {
    client.println(F("HTTP/1.1 501 Not Implemented"));
    break;
  }
  }

  client.stop();
}

void WebSocketServer::_cleanDeadConnections() {
  const auto end = &m_sockets[MAX_CONNECTIONS];
  for (auto it = &m_sockets[0]; it != end; ++it) {
    if (*it && !(*it)->isAlive()) {
      delete *it;
      *it = nullptr;
    }
  }
}

} // namespace net