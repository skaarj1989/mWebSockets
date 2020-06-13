#include "WebSocketClient.h"

#define _TRIGGER_ERROR(code)                                                   \
  {                                                                            \
    terminate();                                                               \
    if (_onError) _onError(code);                                              \
  }

namespace net {

bool WebSocketClient::open(const char *host, uint16_t port, const char *path) {
  close(GOING_AWAY, true); // close if already open

  m_readyState = ReadyState::CONNECTING;
  if (!m_client.connect(host, port)) {
    __debugOutput(
      F("Error in connection establishment: net::ERR_CONNECTION_REFUSED\n"));
    _TRIGGER_ERROR(WebSocketError::CONNECTION_REFUSED);
    return false;
  }

  _sendRequest(host, port, path);

  if (!_waitForResponse(kTimeoutInterval)) {
    __debugOutput(
      F("Error in connection establishment: net::ERR_CONNECTION_TIMED_OUT\n"));
    _TRIGGER_ERROR(WebSocketError::REQUEST_TIMEOUT);
    return false;
  }

  if (!_readResponse()) return false;

  m_readyState = ReadyState::OPEN;
  if (_onOpen) _onOpen(*this);
  return true;
}

void WebSocketClient::terminate() {
  WebSocket::terminate();
  SAFE_DELETE_ARRAY(m_secKey);
}

void WebSocketClient::listen() {
  if (!m_client.connected()) {
    if (m_readyState == ReadyState::OPEN) {
      terminate();
      if (_onClose) _onClose(*this, ABNORMAL_CLOSURE, nullptr, 0);
    }
    return;
  }

  _readFrame();
}

void WebSocketClient::onOpen(const onOpenCallback &callback) {
  _onOpen = callback;
}
void WebSocketClient::onError(const onErrorCallback &callback) {
  _onError = callback;
}

//
// Send request (client handshake):
//
// [1] GET /chat HTTP/1.1
// [2] Host: example.com:8000
// [3] Upgrade: websocket
// [4] Connection: Upgrade
// [5] Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==
// [6] Sec-WebSocket-Version: 13
// [7]
//
void WebSocketClient::_sendRequest(
  const char *host, uint16_t port, const char *path) {
  char buffer[128]{};

  snprintf_P(buffer, sizeof(buffer), (PGM_P)F("GET %s HTTP/1.1"), path);
  m_client.println(buffer);

  snprintf_P(buffer, sizeof(buffer), (PGM_P)F("Host: %s:%u"), host, port);
  m_client.println(buffer);

  m_client.println(F("Upgrade: websocket"));
  m_client.println(F("Connection: Upgrade"));

  m_secKey = new char[26]{};
  generateSecKey(m_secKey);
  snprintf_P(
    buffer, sizeof(buffer), (PGM_P)F("Sec-WebSocket-Key: %s"), m_secKey);
  m_client.println(buffer);

  m_client.println(F("Sec-WebSocket-Version: 13\r\n"));
  m_client.flush();
}

bool WebSocketClient::_waitForResponse(uint16_t maxAttempts, uint8_t time) {
  uint16_t attempts = 0;
  while (!m_client.available() && attempts < maxAttempts) {
    attempts++;
    delay(time);
  }

  return attempts < maxAttempts;
}

//
// Read response (server-side handshake):
//
// [1] HTTP/1.1 101 Switching Protocols
// [2] Upgrade: websocket
// [3] Connection: Upgrade
// [4] Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=
// [5]
//
bool WebSocketClient::_readResponse() {
  uint8_t flags = 0x0;

  int32_t bite = -1;
  byte currentLine = 0;
  byte counter = 0;

  char buffer[128]{};

  while ((bite = _read()) != -1) {
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
          _TRIGGER_ERROR(WebSocketError::BAD_REQUEST);
          return false;
        }
      } else {
        if (lineBreakPos > 0) {
          char *rest = buffer;
          char *value = nullptr;
          
          char *header = strtok_r(rest, ":", &rest);

          //
          // [2] Upgrade header:
          //

          if (strcmp_P(header, (PGM_P)F("Upgrade")) == 0) {
            value = strtok_r(rest, " ", &rest);
            if (!value || (strcasecmp_P(value, (PGM_P)F("websocket")) != 0)) {
              __debugOutput(F("Error during WebSocket handshake: 'Upgrade' "
                              "header value is not 'websocket': %s\n"),
                value);
              _TRIGGER_ERROR(WebSocketError::UPGRADE_REQUIRED);
              return false;
            }

            flags |= kValidUpgradeHeader;
          }

          //
          // [3] Connection header:
          //

          else if (strcmp_P(header, (PGM_P)F("Connection")) == 0) {
            value = strtok_r(rest, " ", &rest);
            if (!value || (strcmp_P(value, (PGM_P)F("Upgrade")) != 0)) {
              __debugOutput(
                F("Error during WebSocket handshake: 'Connection' header "
                  "value is not 'Upgrade': %s\n"),
                value);
              _TRIGGER_ERROR(WebSocketError::UPGRADE_REQUIRED);
              return false;
            }

            flags |= kValidConnectionHeader;
          }

          //
          // [4] Sec-WebSocket-Accept header:
          //

          else if (strcmp_P(header, (PGM_P)F("Sec-WebSocket-Accept")) == 0) {
            value = strtok_r(rest, " ", &rest);

            char encodedKey[28]{};
            encodeSecKey(encodedKey, m_secKey);
            SAFE_DELETE_ARRAY(m_secKey);

            if (!value || (strcmp(value, encodedKey) != 0)) {
              __debugOutput(F("Error during WebSocket handshake: Incorrect "
                              "'Sec-WebSocket-Accept' header value\n"));
              _TRIGGER_ERROR(WebSocketError::BAD_REQUEST);
              return false;
            }

            flags |= kValidSecKey;
          } else {
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
      currentLine++;
    }
  }

  return _validateHandshake(flags);
}

bool WebSocketClient::_validateHandshake(uint8_t flags) {
  if (!(flags & kValidUpgradeHeader)) {
    __debugOutput(
      F("Error during WebSocket handshake: 'Upgrade' header is missing\n"));
    _TRIGGER_ERROR(WebSocketError::UPGRADE_REQUIRED);
    return false;
  }

  if (!(flags & kValidConnectionHeader)) {
    __debugOutput(
      F("Error during WebSocket handshake: 'Connection' header is missing\n"));
    _TRIGGER_ERROR(WebSocketError::UPGRADE_REQUIRED);
    return false;
  }

  if (!(flags & kValidSecKey)) {
    __debugOutput(F("Error during WebSocket handshake: "
                    "'Sec-WebSocket-Accept' header missing or invalid\n"));
    _TRIGGER_ERROR(WebSocketError::BAD_REQUEST);
    return false;
  }

  return true;
}

} // namespace net