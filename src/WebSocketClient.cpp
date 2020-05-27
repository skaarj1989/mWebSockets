#include "WebSocketClient.h"

#define _TRIGGER_ERROR(code)                                                   \
  {                                                                            \
    if (_onError) _onError(code);                                              \
  }

namespace net {

WebSocketClient::WebSocketClient() {}
WebSocketClient::~WebSocketClient() {}

bool WebSocketClient::open(const char *host, uint16_t port, const char *path) {
  close(GOING_AWAY, true); // close if already open

  m_readyState = ReadyState::CONNECTING;
  if (!m_client.connect(host, port)) {
    __debugOutput(
      F("Error in connection establishment: net::ERR_CONNECTION_REFUSED\n"));
    terminate();
    return false;
  }

  _sendRequest(host, port, path);

  if (!_waitForResponse(kTimeoutInterval)) {
    __debugOutput(
      F("Error in connection establishment: net::ERR_CONNECTION_TIMED_OUT\n"));
    _TRIGGER_ERROR(WebSocketError::REQUEST_TIMEOUT);
    terminate();
    return false;
  }

  if (!_readResponse()) {
    terminate();
    return false;
  }

  return true;
}

void WebSocketClient::onOpen(const onOpenCallback &callback) {
  _onOpen = callback;
}
void WebSocketClient::onError(const onErrorCallback &callback) {
  _onError = callback;
}

void WebSocketClient::listen() {
  if (!m_client.connected()) {
    if (m_readyState == ReadyState::OPEN) {
      terminate();
      if (_onClose) _onClose(*this, ABNORMAL_CLOSURE, nullptr, 0);
    }

    return;
  }

  _handleFrame();
}

void WebSocketClient::_sendRequest(
  const char *host, uint16_t port, const char *path) {
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

  char buffer[128]{};

  snprintf_P(buffer, sizeof(buffer), (PGM_P)F("GET %s HTTP/1.1"), path);
  m_client.println(buffer);

  snprintf_P(buffer, sizeof(buffer), (PGM_P)F("Host: %s:%d"), host, port);
  m_client.println(buffer);

  m_client.println(F("Upgrade: websocket"));
  m_client.println(F("Connection: Upgrade"));

  m_secKey = new char[26]{};
  memset(m_secKey, '\0', sizeof(char) * 26);

  generateSecKey(m_secKey);
  snprintf_P(
    buffer, sizeof(buffer), (PGM_P)F("Sec-WebSocket-Key: %s"), m_secKey);
  m_client.println(buffer);

  m_client.println(F("Sec-WebSocket-Version: 13\r\n"));
  m_client.flush();
}

bool WebSocketClient::_readResponse() {
  uint8_t flags = 0x0;

  int16_t bite = -1;
  byte currentLine = 0;
  byte counter = 0;

  char buffer[128]{};

  //
  // Read response (server-side handshake):
  //
  // [1] HTTP/1.1 101 Switching Protocols
  // [2] Upgrade: websocket
  // [3] Connection: Upgrade
  // [4] Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=
  // [5]
  //

  while ((bite = _read()) != -1) {
    buffer[counter++] = bite;

    if (static_cast<char>(bite) == '\n') {
      uint8_t lineBreakPos = strcspn(buffer, "\r\n");
      buffer[lineBreakPos] = '\0';

#ifdef _DUMP_HANDSHAKE
      printf(F("[Line #%d] %s\n"), currentLine, buffer);
#endif

      if (currentLine == 0) {
        if (strncmp_P(buffer, (PGM_P)F("HTTP/1.1 101"), 12) != 0) {
          __debugOutput(F("Error during WebSocket handshake: "
                          "net::ERR_INVALID_HTTP_RESPONSE\n"));
          _TRIGGER_ERROR(WebSocketError::BAD_REQUEST);
          terminate();
          return false;
        }
      } else {
        if (lineBreakPos > 0) {
          char *header = strtok(buffer, ":");
          char *value = nullptr;

          //
          // [2] Upgrade header:
          //

          if (strcmp_P(header, (PGM_P)F("Upgrade")) == 0) {
            value = strtok(nullptr, " ");

            if (strcasecmp_P(value, (PGM_P)F("websocket")) != 0) {
              __debugOutput(F("Error during WebSocket handshake: 'Upgrade' "
                              "header value is not 'websocket': %s\n"),
                value);
              _TRIGGER_ERROR(WebSocketError::UPGRADE_REQUIRED);
              terminate();
              return false;
            }

            flags |= kValidUpgradeHeader;
          }

          //
          // [3] Connection header:
          //

          else if (strcmp_P(header, (PGM_P)F("Connection")) == 0) {
            value = strtok(nullptr, " ");

            if (strcmp_P(value, (PGM_P)F("Upgrade")) != 0) {
              __debugOutput(
                F("Error during WebSocket handshake: 'Connection' header "
                  "value is not 'Upgrade': %s\n"),
                value);
              _TRIGGER_ERROR(WebSocketError::UPGRADE_REQUIRED);
              terminate();
              return false;
            }

            flags |= kValidConnectionHeader;
          }

          //
          // [4] Sec-WebSocket-Accept header:
          //

          else if (strcmp_P(header, (PGM_P)F("Sec-WebSocket-Accept")) == 0) {
            value = strtok(nullptr, " ");

            char encodedKey[32]{};
            encodeSecKey(encodedKey, m_secKey);
            SAFE_DELETE_ARRAY(m_secKey);

            if (strcmp(value, encodedKey) != 0) {
              __debugOutput(F("Error during WebSocket handshake: Incorrect "
                              "'Sec-WebSocket-Accept' header value\n"));
              _TRIGGER_ERROR(WebSocketError::BAD_REQUEST);
              terminate();
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

  if (!(flags & kValidUpgradeHeader)) {
    __debugOutput(
      F("Error during WebSocket handshake: 'Upgrade' header is missing\n"));
    _TRIGGER_ERROR(WebSocketError::UPGRADE_REQUIRED);
    terminate();
    return false;
  }

  if (!(flags & kValidConnectionHeader)) {
    __debugOutput(
      F("Error during WebSocket handshake: 'Connection' header is missing\n"));
    _TRIGGER_ERROR(WebSocketError::UPGRADE_REQUIRED);
    terminate();
    return false;
  }

  if (!(flags & kValidSecKey)) {
    __debugOutput(F("Error during WebSocket handshake: "
                    "'Sec-WebSocket-Accept' header missing or invalid\n"));
    _TRIGGER_ERROR(WebSocketError::BAD_REQUEST);
    terminate();
    return false;
  }

  m_readyState = ReadyState::OPEN;
  return true;
}

} // namespace net