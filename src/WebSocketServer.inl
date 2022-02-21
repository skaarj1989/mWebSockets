// https://developer.mozilla.org/en-US/docs/Web/API/WebSockets_API/Writing_WebSocket_servers

namespace net {

constexpr bool isValidVersion(uint8_t version) {
  return version == 8 || version == 13;
}

//
// WebSocketServer class (public):
//

template <class NetServer, class NetClient, uint8_t _>
WebSocketServer<NetServer, NetClient, _>::WebSocketServer(uint16_t port)
  : m_server{port} {}
template <class NetServer, class NetClient, uint8_t _>
WebSocketServer<NetServer, NetClient, _>::~WebSocketServer() {
  shutdown();
}

template <class NetServer, class NetClient, uint8_t _>
void WebSocketServer<NetServer, NetClient, _>::begin(
    const verifyClientCallback &verifyClient,
    const protocolHandlerCallback &protocolHandler) {
  _verifyClient = verifyClient;
  _protocolHandler = protocolHandler;
  m_server.begin();
}
template <class NetServer, class NetClient, uint8_t _>
void WebSocketServer<NetServer, NetClient, _>::shutdown() {
  for (auto *ws : m_sockets) {
    if (ws) {
      ws->close(WebSocketCloseCode::GOING_AWAY, true);
      delete ws;
      ws = nullptr;
    }
  }

  // Here I shoud call somethig like m_server.close() but unfortunately
  // EthernetServer does not implement anything like that
  // #TODO server state enum?
}

template <class NetServer, class NetClient, uint8_t _>
void WebSocketServer<NetServer, NetClient, _>::broadcast(
    WebSocketDataType dataType, const char *message, uint16_t length) {
  for (auto *ws : m_sockets)
    if (ws && ws->getReadyState() == WebSocketReadyState::OPEN)
      ws->send(dataType, message, length);
}

template <class NetServer, class NetClient, uint8_t _>
void WebSocketServer<NetServer, NetClient, _>::listen() {
  _cleanDeadConnections();

  NetClient client{m_server.available()};
  if (client) {
    auto *ws = _getWebSocket(client);
    if (!ws) {
      for (auto &it : m_sockets) {
        if (!it) {
          char selectedProtocol[32]{};
          if (_handleRequest(client, selectedProtocol)) {
            ws = it = new WebSocket<NetClient>{
                client, *selectedProtocol ? selectedProtocol : nullptr};
            if (_onConnection) _onConnection(*ws);
          }
          break;
        }
      }
      // Server is full ...
      if (!ws) _rejectRequest(client, WebSocketError::SERVICE_UNAVAILABLE);
    }
  }

  for (auto it : m_sockets)
    if (it && it->m_client.connected() && it->m_client.available())
      it->_readFrame();
}

template <class NetServer, class NetClient, uint8_t _>
uint8_t WebSocketServer<NetServer, NetClient, _>::countClients() const {
  uint8_t count{0};
  for (auto *ws : m_sockets)
    if (ws && ws->isAlive()) ++count;

  return count;
}

template <class NetServer, class NetClient, uint8_t _>
void WebSocketServer<NetServer, NetClient, _>::onConnection(
    const onConnectionCallback &callback) {
  _onConnection = callback;
}

template <class NetServer, class NetClient, uint8_t _>
WebSocket<NetClient> *WebSocketServer<NetServer, NetClient, _>::_getWebSocket(
    NetClient &client) const {
  for (auto *ws : m_sockets)
    if (ws && ws->m_client == client) return ws;

  return nullptr;
}

template <class NetServer, class NetClient, uint8_t _>
bool WebSocketServer<NetServer, NetClient, _>::_handleRequest(
    NetClient &client, char selectedProtocol[]) {
  while (!client.available())
    delay(1);

  char secKey[32]{}; // Holds client Sec-WebSocket-Key
  uint8_t flags{0};
  char protocols[32]{};

  int32_t bite{-1};
  uint8_t currentLine{0};
  uint8_t counter{0};

  // Large enought to hold the longest header field
  //  Chrome: 'User-Agent' = ~126 characters
  //  Edge: 'User-Agent' = ~141 characters
  //  Firefox: 'User-Agent' = ~90 characters
  //  Opera: 'User-Agent' = ~145 characters
  char buffer[160]{};

  // [1] GET /chat HTTP/1.1
  // [2] Host: example.com:8000
  // [3] Upgrade: websocket
  // [4] Connection: Upgrade
  // [5] Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==
  // [6] Sec-WebSocket-Version: 13
  // [7]

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
        if (!isValidGET(rest)) {
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
            if (!value || !isValidUpgrade(value)) {
              _rejectRequest(client, WebSocketError::BAD_REQUEST);
              return false;
            }
            flags |= kValidUpgradeHeader;
          }

          //
          // [4] Connection header:
          //

          else if (strcasecmp_P(header, (PGM_P)F("Connection")) == 0) {
            if (rest && isValidConnection(rest))
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

          else if (
              strcasecmp_P(header, (PGM_P)F("Sec-WebSocket-Version")) == 0) {
            value = strtok_r(rest, " ", &rest);
            if (!value || !isValidVersion(atoi(value))) {
              _rejectRequest(client, WebSocketError::BAD_REQUEST);
              return false;
            }
            flags |= kValidVersion;
          }

          //
          // Sec-WebSocket-Protocol (optional):
          //

          else if (
              strcasecmp_P(header, (PGM_P)F("Sec-WebSocket-Protocol")) == 0) {
            for (uint8_t i{0}; rest != nullptr; ++i) {
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
          const auto errorCode = validateHandshake(flags, secKey);
          if (errorCode != WebSocketError::NO_ERROR) {
            _rejectRequest(client, errorCode);
            return false;
          }

          selectedProtocol[0] = '\0';
          if (*protocols) {
            strcpy(
                selectedProtocol, _protocolHandler
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

template <class NetServer, class NetClient, uint8_t _>
void WebSocketServer<NetServer, NetClient, _>::_rejectRequest(
    NetClient &client, const WebSocketError code) {
  switch (code) {
  case WebSocketError::CONNECTION_REFUSED:
    client.println(F("HTTP/1.1 111 Connection refused"));
    break;
  case WebSocketError::BAD_REQUEST:
    client.println(F("HTTP/1.1 400 Bad Request"));
    break;
  case WebSocketError::UPGRADE_REQUIRED:
    client.println(F("HTTP/1.1 426 Upgrade Required"));
    break;
  case WebSocketError::SERVICE_UNAVAILABLE:
    client.println(F("HTTP/1.1 503 Service Unavailable"));
    break;
  default:
    client.println(F("HTTP/1.1 501 Not Implemented"));
    break;
  }

  client.stop();
}
template <class NetServer, class NetClient, uint8_t _>
void WebSocketServer<NetServer, NetClient, _>::_acceptRequest(
    NetClient &client, const char *secKey, const char *protocol) {

  // [1] HTTP/1.1 101 Switching Protocols
  // [2] Upgrade: websocket
  // [3] Connection: Upgrade
  // [4] Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=
  // [5]

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

  if (protocol[0] != '\0') {
    // NOTE: Up to 26 characters for protocol value
    snprintf(
        buffer, sizeof(buffer), (PGM_P)F("Sec-WebSocket-Protocol: %s"),
        protocol);
    client.println(buffer);
  }

  client.println();
}
template <class NetServer, class NetClient, uint8_t _>
void WebSocketServer<NetServer, NetClient, _>::_cleanDeadConnections() {
  for (auto &it : m_sockets) {
    if (it && !it->isAlive()) {
      delete it;
      it = nullptr;
    }
  }
}

} // namespace net
