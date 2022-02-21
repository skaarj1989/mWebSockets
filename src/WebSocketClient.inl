// https://developer.mozilla.org/en-US/docs/Web/API/WebSockets_API/Writing_WebSocket_client_applications

namespace net {

//
// WebSocketClient class (public):
//

template <class NetClient>
bool WebSocketClient<NetClient>::open(
    const char *host, uint16_t port, const char *path,
    const char *supportedProtocols) {
  WebSocket<NetClient>::close(GOING_AWAY, true); // Close if it's already open

  const auto status = WebSocket<NetClient>::m_client.connect(host, port);
  if (status != 1) {
    __debugOutput(
        F("Error in connection establishment: net::ERR_CONNECTION_REFUSED\n"));
    _triggerError(WebSocketError::CONNECTION_REFUSED);
    return false;
  }

  char secKey[25]{};
  generateSecKey(secKey);
  _sendRequest(host, port, path, secKey, supportedProtocols);

  WebSocket<NetClient>::m_readyState = WebSocketReadyState::CONNECTING;
  if (!_waitForResponse(kTimeoutInterval)) {
    __debugOutput(F(
        "Error in connection establishment: net::ERR_CONNECTION_TIMED_OUT\n"));
    _triggerError(WebSocketError::REQUEST_TIMEOUT);
    return false;
  }

  if (!_readResponse(secKey)) return false;

  WebSocket<NetClient>::m_readyState = WebSocketReadyState::OPEN;
  if (_onOpen) _onOpen(*this);
  return true;
}

template <class NetClient> void WebSocketClient<NetClient>::listen() {
  if (!WebSocket<NetClient>::m_client.connected()) {
    if (WebSocket<NetClient>::m_readyState == WebSocketReadyState::OPEN) {
      WebSocket<NetClient>::terminate();
      if (WebSocket<NetClient>::_onClose)
        WebSocket<NetClient>::_onClose(*this, ABNORMAL_CLOSURE, nullptr, 0);
    }
    return;
  }

  if (WebSocket<NetClient>::m_client.available())
    WebSocket<NetClient>::_readFrame();
}

template <class NetClient>
void WebSocketClient<NetClient>::onOpen(const onOpenCallback &callback) {
  _onOpen = callback;
}
template <class NetClient>
void WebSocketClient<NetClient>::onError(const onErrorCallback &callback) {
  _onError = callback;
}

//
// (private):
//

template <class NetClient>
void WebSocketClient<NetClient>::_sendRequest(
    const char *host, uint16_t port, const char *path, const char *secKey,
    const char *supportedProtocols) {
  char buffer[128]{};

  // [1] GET /chat HTTP/1.1
  // [2] Host: example.com:8000
  // [3] Upgrade: websocket
  // [4] Connection: Upgrade
  // [5] Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==
  // [6] Sec-WebSocket-Version: 13
  // [7]

  snprintf_P(buffer, sizeof(buffer), (PGM_P)F("GET %s HTTP/1.1"), path);
  WebSocket<NetClient>::m_client.println(buffer);

  snprintf_P(buffer, sizeof(buffer), (PGM_P)F("Host: %s:%u"), host, port);
  WebSocket<NetClient>::m_client.println(buffer);

  WebSocket<NetClient>::m_client.println(F("Upgrade: websocket"));
  WebSocket<NetClient>::m_client.println(F("Connection: Upgrade"));

  snprintf_P(buffer, sizeof(buffer), (PGM_P)F("Sec-WebSocket-Key: %s"), secKey);
  WebSocket<NetClient>::m_client.println(buffer);

  if (supportedProtocols) {
    snprintf_P(
        buffer, sizeof(buffer), (PGM_P)F("Sec-WebSocket-Protocol: %s"),
        supportedProtocols);
    WebSocket<NetClient>::m_client.println(buffer);
  }
  WebSocket<NetClient>::m_client.println(F("Sec-WebSocket-Version: 13\r\n"));

  WebSocket<NetClient>::m_client.flush();
}
template <class NetClient>
bool WebSocketClient<NetClient>::_waitForResponse(
    uint16_t maxAttempts, uint8_t time) {
  uint16_t attempts{0};
  while (!WebSocket<NetClient>::m_client.available() &&
         attempts < maxAttempts) {
    ++attempts;
    delay(time);
  }

  return attempts < maxAttempts;
}
template <class NetClient>
bool WebSocketClient<NetClient>::_readResponse(const char *secKey) {
  uint8_t flags{0};

  int32_t bite{-1};
  uint8_t currentLine{0};
  uint8_t counter{0};

  char buffer[128]{};

  // [1] HTTP/1.1 101 Switching Protocols
  // [2] Upgrade: websocket
  // [3] Connection: Upgrade
  // [4] Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=
  // [5]

  while ((bite = WebSocket<NetClient>::_read()) != -1) {
    buffer[counter++] = bite;

    if (bite == '\n') {
      uint8_t lineBreakPos = strcspn(buffer, "\r\n");
      buffer[lineBreakPos] = '\0';

#ifdef _DUMP_HANDSHAKE
      printf(F("[Line #%u] %s\n"), currentLine, buffer);
#endif

      if (currentLine == 0) {
        if (strncmp_P(buffer, (PGM_P)F("HTTP/1.1 101"), 12) != 0) {
          __debugOutput(F("Error during WebSocket handshake: "
                          "net::ERR_INVALID_HTTP_RESPONSE\n"));
          _triggerError(WebSocketError::BAD_REQUEST);
          return false;
        }
      } else {
        if (lineBreakPos > 0) {
          char *rest{buffer};
          char *value{nullptr};

          char *header{strtok_r(rest, ":", &rest)};

          //
          // [2] Upgrade header:
          //

          if (strcasecmp_P(header, (PGM_P)F("Upgrade")) == 0) {
            value = strtok_r(rest, " ", &rest);
            if (!value || (strcasecmp_P(value, (PGM_P)F("websocket")) != 0)) {
              __debugOutput(
                  F("Error during WebSocket handshake: 'Upgrade' "
                    "header value is not 'websocket': %s\n"),
                  value);
              _triggerError(WebSocketError::UPGRADE_REQUIRED);
              return false;
            }

            flags |= kValidUpgradeHeader;
          }

          //
          // [3] Connection header:
          //

          else if (strcasecmp_P(header, (PGM_P)F("Connection")) == 0) {
            value = strtok_r(rest, " ", &rest);
            if (!value || (strcasecmp_P(value, (PGM_P)F("Upgrade")) != 0)) {
              __debugOutput(
                  F("Error during WebSocket handshake: 'Connection' header "
                    "value is not 'Upgrade': %s\n"),
                  value);
              _triggerError(WebSocketError::UPGRADE_REQUIRED);
              return false;
            }

            flags |= kValidConnectionHeader;
          }

          //
          // [4] Sec-WebSocket-Accept header:
          //

          else if (
              strcasecmp_P(header, (PGM_P)F("Sec-WebSocket-Accept")) == 0) {
            value = strtok_r(rest, " ", &rest);

            char encodedKey[29]{};
            encodeSecKey(secKey, encodedKey);
            if (!value || (strcmp(value, encodedKey) != 0)) {
              __debugOutput(F("Error during WebSocket handshake: Incorrect "
                              "'Sec-WebSocket-Accept' header value\n"));
              _triggerError(WebSocketError::BAD_REQUEST);
              return false;
            }

            flags |= kValidSecKey;
          }

          //
          // Sec-WebSocket-Protocol (optional):
          //

          else if (
              strcasecmp_P(header, (PGM_P)F("Sec-WebSocket-Protocol")) == 0) {
            value = strtok_r(rest, " ", &rest);
            WebSocket<NetClient>::m_protocol = new char[strlen(value) + 1]{};
            strcpy(WebSocket<NetClient>::m_protocol, value);
          }

          else {
            // don't care about other headers ...
          }
        }

        //
        // [5] Empty line (end of response)
        //

        else
          break;
      }

      memset(buffer, '\0', sizeof(buffer));
      counter = 0;
      ++currentLine;
    }
  }

  return _validateHandshake(flags);
}
template <class NetClient>
bool WebSocketClient<NetClient>::_validateHandshake(uint8_t flags) {
  if (!(flags & kValidUpgradeHeader)) {
    __debugOutput(
        F("Error during WebSocket handshake: 'Upgrade' header is missing\n"));
    _triggerError(WebSocketError::UPGRADE_REQUIRED);
    return false;
  }
  if (!(flags & kValidConnectionHeader)) {
    __debugOutput(F(
        "Error during WebSocket handshake: 'Connection' header is missing\n"));
    _triggerError(WebSocketError::UPGRADE_REQUIRED);
    return false;
  }
  if (!(flags & kValidSecKey)) {
    __debugOutput(F("Error during WebSocket handshake: "
                    "'Sec-WebSocket-Accept' header missing or invalid\n"));
    _triggerError(WebSocketError::BAD_REQUEST);
    return false;
  }
  return true;
}

template <class NetClient>
void WebSocketClient<NetClient>::_triggerError(WebSocketError code) {
  WebSocket<NetClient>::terminate();
  if (_onError) _onError(code);
}

} // namespace net
