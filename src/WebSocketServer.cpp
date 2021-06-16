#include "WebSocketServer.h"

// https://developer.mozilla.org/en-US/docs/Web/API/WebSockets_API/Writing_WebSocket_servers

namespace net {

WebSocketServer::WebSocketServer(uint16_t port) : m_server{port} {}
WebSocketServer::~WebSocketServer() { shutdown(); }

void WebSocketServer::begin(const verifyClientCallback &verifyClient,
  const protocolHandlerCallback &protocolHandler) {
  _verifyClient = verifyClient;
  _protocolHandler = protocolHandler;
  m_server.begin();
}
void WebSocketServer::shutdown() {
  for (auto ws : m_sockets) {
    if (ws) {
      ws->close(WebSocket::CloseCode::GOING_AWAY, true);
      SAFE_DELETE(ws);
    }
  }

  // Here I shoud call somethig like m_server.close() but unfortunately
  // EthernetServer does not implement anything like that
  // #TODO server state enum?
}

void WebSocketServer::broadcast(
  const WebSocket::DataType dataType, const char *message, uint16_t length) {
  for (auto ws : m_sockets)
    if (ws && ws->getReadyState() == WebSocket::ReadyState::OPEN)
      ws->send(dataType, message, length);
}

void WebSocketServer::listen() {
  _cleanDeadConnections();

#if NETWORK_CONTROLLER == NETWORK_CONTROLLER_WIFI
  if (m_server.hasClient()) {
    WiFiClient client;
    WebSocket *ws{nullptr};

    for (auto &it : m_sockets) {
      if (!it) {
        client = m_server.available();
        char selectedProtocol[32]{};
        if (_handleRequest(client, selectedProtocol)) {
          ws = it = new WebSocket{
            client, *selectedProtocol ? selectedProtocol : nullptr};
          if (_onConnection) _onConnection(*ws);
        }
        break;
      }
    }

    // Server is full ...
    if (!ws) _rejectRequest(client, WebSocketError::SERVICE_UNAVAILABLE);
  }

  for (auto it : m_sockets) {
    if (it && it->m_client.connected() && it->m_client.available()) {
      it->_readFrame();
    }
  }
#else
  EthernetClient client{m_server.available()};
  if (client && client.available()) {
    WebSocket *ws{_getWebSocket(client)};

    if (!ws) {
      for (auto &it : m_sockets) {
        if (!it) {
          char selectedProtocol[32]{};
          if (_handleRequest(client, selectedProtocol)) {
            ws = it = new WebSocket{
              client, *selectedProtocol ? selectedProtocol : nullptr};
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
  uint8_t count{0};
  for (auto ws : m_sockets)
    if (ws && ws->isAlive()) ++count;

  return count;
}

void WebSocketServer::onConnection(const onConnectionCallback &callback) {
  _onConnection = callback;
}

WebSocket *WebSocketServer::_getWebSocket(NetClient &client) const {
  for (auto ws : m_sockets)
    if (ws && ws->m_client == client) return ws;

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
bool WebSocketServer::_handleRequest(
  NetClient &client, char selectedProtocol[]) {
#if NETWORK_CONTROLLER == NETWORK_CONTROLLER_WIFI
  while (!client.available()) {
    delay(10);
    __debugOutput(F("."));
  }
#endif

  // Large enought to hold the longest header field
  //  Chrome: 'User-Agent' = ~126 characters
  //  Edge: 'User-Agent' = ~141 characters
  //  Firefox: 'User-Agent' = ~90 characters
  //  Opera: 'User-Agent' = ~145 characters
  char buffer[160]{};

  char secKey[32]{}; // Holds client Sec-WebSocket-Key
  uint8_t flags{0};
  char protocols[32]{};

  int32_t bite{-1};
  byte currentLine{0};
  byte counter{0};

  while ((bite = client.read()) != -1) {
    buffer[counter++] = bite;

    if (bite == '\n') {
      const auto lineBreakPos = static_cast<uint8_t>(strcspn(buffer, "\r\n"));
      buffer[lineBreakPos] = '\0';
#ifdef _DUMP_HANDSHAKE
      printf(F("[Line #%u] %s\n"), currentLine, buffer);
#endif

      char *rest{buffer};

      //
      // [1] GET method:
      //

      if (currentLine == 0) {
        if (!_isValidGET(rest)) {
          _rejectRequest(client, WebSocketError::BAD_REQUEST);
          return false;
        }
      } else {
        if (lineBreakPos > 0) {
          auto header = strtok_r(rest, ":", &rest);
          char *value{nullptr};

          //
          // [2] Host header:
          //

          if (strcasecmp_P(header, (PGM_P)F("Host")) == 0) {
            // #TODO ... or not
          }

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
            if (value) strcpy(secKey, value);
          }

          //
          // [6] Sec-WebSocket-Version header:
          //

          else if (strcasecmp_P(header, (PGM_P)F("Sec-WebSocket-Version")) ==
                   0) {
            value = strtok_r(rest, " ", &rest);
            if (!value || !_isValidVersion(atoi(value))) {
              _rejectRequest(client, WebSocketError::BAD_REQUEST);
              return false;
            }
            flags |= kValidVersion;
          }

          //
          // Sec-WebSocket-Protocol (optional):
          //

          else if (strcasecmp_P(header, (PGM_P)F("Sec-WebSocket-Protocol")) ==
                   0) {
            for (byte i = 0; rest != nullptr; ++i) {
              if (*protocols) strcat(protocols, ",");
              const auto pch = strtok_r(rest, ",", &rest);
              strcat(protocols, pch + 1); // Skip leading whitespace
            }
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
          const auto errorCode = _validateHandshake(flags, secKey);
          if (errorCode != WebSocketError::NO_ERROR) {
            _rejectRequest(client, errorCode);
            return false;
          }

          selectedProtocol[0] = '\0';
          if (*protocols) {
            strcpy(selectedProtocol, _protocolHandler
                                       ? _protocolHandler(protocols)
                                       : strtok_r(protocols, ",", &rest));
          }
          _acceptRequest(client, secKey, selectedProtocol);
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
  char *rest{line};
  for (byte i = 0; rest != nullptr; ++i) {
    const auto pch = strtok_r(rest, " ", &rest);
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
  char *rest{value};
  char *item{nullptr};

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
  NetClient &client, const WebSocketError code) {
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
void WebSocketServer::_acceptRequest(
  NetClient &client, const char *secKey, const char *protocol) {
  client.println(F("HTTP/1.1 101 Switching Protocols"));
  // client.println(F("Server: Arduino"));
  client.println(F("X-Powered-By: mWebSockets"));
  client.println(F("Upgrade: websocket"));
  client.println(F("Connection: Upgrade"));

  // 22 characters for header + 28 for accept key + 1 for NULL
  char buffer[51]{};
  strcpy_P(buffer, (PGM_P)F("Sec-WebSocket-Accept: ")); // 22
  encodeSecKey(secKey, buffer + 22);
  client.println(buffer);

  if (protocol) {
    // NOTE: Up to 26 characters for protocol value
    snprintf_P(
      buffer, sizeof(buffer), (PGM_P)F("Sec-WebSocket-Protocol: %s"), protocol);
    client.println(buffer);
  }

  client.println();
}

void WebSocketServer::_cleanDeadConnections() {
  for (auto &it : m_sockets) {
    if (it && !it->isAlive()) {
      delete it;
      it = nullptr;
    }
  }
}

} // namespace net
