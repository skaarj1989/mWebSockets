#include "WebSocketServer.h"

namespace net {

WebSocketServer::WebSocketServer(uint16_t port) : m_server(port) {}
WebSocketServer::~WebSocketServer() { shutdown(); }

void WebSocketServer::begin(const verifyClientCallback &callback) {
  _verifyClient = callback;
  m_server.begin();
}

void WebSocketServer::shutdown() {
  const auto end = &m_sockets[kMaxConnections];
  for (auto it = &m_sockets[0]; it != end; ++it) {
    if (*it) {
      (*it)->close(WebSocket::CloseCode::GOING_AWAY, true);
      SAFE_DELETE(*it);
    }
  }

  // Here I shoud call somethig like m_server.close() but unfortunately
  // EthernetServer does not implement anything like that
  // #TODO server state enum?
}

void WebSocketServer::broadcast(
  const WebSocket::DataType &dataType, const char *message, uint16_t length) {
  const auto end = &m_sockets[kMaxConnections];
  for (auto it = &m_sockets[0]; it != end; ++it) {
    if (!(*it) || (*it)->getReadyState() != WebSocket::ReadyState::OPEN)
      continue;

    (*it)->send(dataType, message, length);
  }
}

void WebSocketServer::listen() {
  _cleanDeadConnections();

#if NETWORK_CONTROLLER == NETWORK_CONTROLLER_WIFI
  const auto end = &m_sockets[kMaxConnections];

  if (m_server.hasClient()) {
    WiFiClient client;
    WebSocket *ws{ nullptr };

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
      if ((*it)->m_client.available()) (*it)->_readFrame();
    }
  }
#else
  EthernetClient client = m_server.available();
  if (client && client.available()) {
    WebSocket *ws{ _getWebSocket(client) };

    if (!ws) {
      const auto end = &m_sockets[kMaxConnections];
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
    } else {
      ws->_readFrame();
    }
  }
#endif
}

uint8_t WebSocketServer::countClients() const {
  uint8_t count{ 0 };
  const auto end = &m_sockets[kMaxConnections];
  for (auto it = &m_sockets[0]; it != end; ++it)
    if (*it && (*it)->isAlive()) ++count;

  return count;
}

void WebSocketServer::onConnection(const onConnectionCallback &callback) {
  _onConnection = callback;
}

WebSocket *WebSocketServer::_getWebSocket(NetClient &client) const {
  const auto end = &m_sockets[kMaxConnections];
  for (auto it = &m_sockets[0]; it != end; ++it)
    if (*it && (*it)->m_client == client) return *it;

  return nullptr;
}

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
bool WebSocketServer::_handleRequest(NetClient &client) {
#if NETWORK_CONTROLLER == NETWORK_CONTROLLER_WIFI
  while (!client.available()) {
    delay(10);
    __debugOutput(F("."));
  }
#endif

  uint8_t flags{ 0 };

  int32_t bite{ -1 };
  byte currentLine{ 0 };
  byte counter{ 0 };

  // Large enought to hold longest header field
  //  Chrome: 'User-Agent' = ~126 characters
  //  Edge: 'User-Agent' = ~141 characters
  //  Firefox: 'User-Agent' = ~90 characters
  //  Opera: 'User-Agent' = ~145 characters
  char buffer[160]{};
  char secKey[32]{}; // Holds client Sec-WebSocket-Key

  while ((bite = client.read()) != -1) {
    buffer[counter++] = bite;

    if (bite == '\n') {
      uint8_t lineBreakPos = strcspn(buffer, "\r\n");
      buffer[lineBreakPos] = '\0';

      char *rest{ buffer };

#ifdef _DUMP_HANDSHAKE
      printf(F("[Line #%u] %s\n"), currentLine, buffer);
#endif

      //
      // [1] GET method
      //

      if (currentLine == 0) {
        if (!_isValidGET(rest)) {
          _rejectRequest(client, WebSocketError::BAD_REQUEST);
          return false;
        }
      } else {
        if (lineBreakPos > 0) {
          char *header{ strtok_r(rest, ":", &rest) };
          char *value{ nullptr };

          //
          // [2] Host header:
          //

          // #TODO ... or not
          if (strcasecmp_P(header, (PGM_P)F("Host")) == 0) {}

          //
          // [3] Upgrade header:
          //

          else if (strcasecmp_P(header, (PGM_P)F("Upgrade")) == 0) {
            value = strtok_r(rest, " ", &rest);
            if (!value || !_isValidUpgrade(value)) {
              _rejectRequest(client, WebSocketError::BAD_REQUEST);
              return false;
            }

            flags |= kValidUpgradeHeader;              
          }

          //
          // [4] Connection header:
          //

          else if (strcasecmp_P(header, (PGM_P)F("Connection")) == 0) {
            if (rest && _isValidConnection(rest))
              flags |= kValidConnectionHeader;
          }

          //
          // [5] Sec-WebSocket-Key header:
          //

          else if (strcasecmp_P(header, (PGM_P)F("Sec-WebSocket-Key")) == 0) {
            value = strtok_r(rest, " ", &rest);
            if (value != nullptr) strcpy(secKey, value);
          }

          //
          // [6] Sec-WebSocket-Version header:
          //

          else if (strcasecmp_P(header, (PGM_P)F("Sec-WebSocket-Version")) == 0) {
            value = strtok_r(rest, " ", &rest);
            if (!value || !_isValidVersion(atoi(value))) {
              _rejectRequest(client, WebSocketError::BAD_REQUEST);
              return false;
            }

            flags |= kValidVersion;
          }

          //
          // [ ] Other headers
          //

          else {
            if (_verifyClient) {
              value = strtok_r(rest, " ", &rest);
              if (!_verifyClient(fetchRemoteIp(client), header, value)) {
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
          WebSocketError errorCode{ _validateHandshake(flags, secKey) };
          if (errorCode != WebSocketError::NO_ERROR) {
            _rejectRequest(client, errorCode);
            return false;
          }

          _acceptRequest(client, secKey);
          return true;
        }
      }

      memset(buffer, '\0', sizeof(buffer));
      counter = 0;
      ++currentLine;
    }
  }

  _rejectRequest(client, WebSocketError::BAD_REQUEST);
  return false;
}

bool WebSocketServer::_isValidGET(char *line) {
  char *rest{ line };
  for (byte i = 0; rest != nullptr; ++i) {
    char *pch{ strtok_r(rest, " ", &rest) };
    switch (i) {
    case 0: {
      if (strcmp_P(pch, (PGM_P)F("GET")) != 0) {
        return false;
      }
      break;
    }
    case 1: {
      break; // path doesn't matter ...
    }
    case 2: {
      if (strcmp_P(pch, (PGM_P)F("HTTP/1.1")) != 0) {
        return false;
      }
      break;
    }
    default:
      return false;
    }
  }

  return true;
}

bool WebSocketServer::_isValidUpgrade(const char *value) {
  return strcasecmp_P(value, (PGM_P)F("websocket")) == 0;
}

bool WebSocketServer::_isValidConnection(char *value) {
  char *rest{ value };
  char *item{ nullptr };

  // Firefox sends: "Connection: keep-alive, Upgrade"
  // simple "includes" check:
  while ((item = strtok_r(rest, ",", &rest))) {
    if (strstr_P(item, (PGM_P)F("Upgrade")) != nullptr) {
      return true;
    }
  }

  return false;
}

bool WebSocketServer::_isValidVersion(uint8_t version) {
  switch (version) {
  case 8:
  case 13:
    return true;

  default:
    return false;
  }
}

WebSocketError WebSocketServer::_validateHandshake(
  uint8_t flags, const char *secKey) {
  if ((flags & (kValidConnectionHeader | kValidUpgradeHeader)) !=
      (kValidConnectionHeader | kValidUpgradeHeader)) {
    return WebSocketError::UPGRADE_REQUIRED;
  }

  if (!(flags & kValidVersion) || strlen(secKey) == 0)
    return WebSocketError::BAD_REQUEST;

  return WebSocketError::NO_ERROR;
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

//
// Send response:
//
// [1] HTTP/1.1 101 Switching Protocols
// [2] Upgrade: websocket
// [3] Connection: Upgrade
// [4] Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=
// [5]
//
void WebSocketServer::_acceptRequest(NetClient &client, const char *secKey) {
  char acceptKey[29]{};
  encodeSecKey(acceptKey, secKey);

  // 22 characters for header + 28 for accept key + 1 for NULL
  char secWebSocketAccept[51]{};
  strcpy_P(secWebSocketAccept, (PGM_P)F("Sec-WebSocket-Accept: "));
  strcat(secWebSocketAccept, acceptKey);

  client.println(F("HTTP/1.1 101 Switching Protocols"));
  //client.println(F("Server: Arduino"));
  //client.println(F("X-Powered-By: mWebSockets"));
  client.println(F("Upgrade: websocket"));
  client.println(F("Connection: Upgrade"));
  client.println(secWebSocketAccept);
  client.println();
}

void WebSocketServer::_cleanDeadConnections() {
  const auto end = &m_sockets[kMaxConnections];
  for (auto it = &m_sockets[0]; it != end; ++it) {
    if (*it && !(*it)->isAlive()) {
      delete *it;
      *it = nullptr;
    }
  }
}

} // namespace net