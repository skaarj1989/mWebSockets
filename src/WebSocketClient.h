#pragma once

/** @file */

#include "WebSocket.h"

namespace net {

/**
 * @class WebSocketClient
 * @see https://developer.mozilla.org/en-US/docs/Web/API/WebSockets_API/Writing_WebSocket_client_applications
 */
class WebSocketClient : public WebSocket {
public:
  /** @param ws */
  using onOpenCallback = void (*)(WebSocket &ws);
  /** @param code */
  using onErrorCallback = void (*)(const WebSocketError &code);

public:
  /**
   * @brief Attempts to connect to a server.
   * @remark Do not use "ws://"
   * @code{.cpp}
   * client.onOpen([](WebSocket &ws) { 
   *   // connected ... send hello message or whatever
   * });
   * @endcode
   */
  bool open(const char *host, uint16_t port = 3000, const char *path = "/");
  void terminate();

  /** @note Call this in main loop. */
  void listen();

  /** @brief Sets callback that will be called on successfull connection. */
  void onOpen(const onOpenCallback &onOpen);
  /**
   * @brief Sets error handler.
   * @code{.cpp}
   * client.onError([](const WebSocketError &code) {
   *   switch (code) {
   *   case BAD_REQUEST:
   *   break;
   *   // ...
   *   }
   * });
   * @endcode
   */
  void onError(const onErrorCallback &onError);

private:
  /** @cond */
  void _sendRequest(const char *host, uint16_t port, const char *path);
  bool _waitForResponse(uint16_t maxAttempts, uint8_t time = 1);
  bool _readResponse();
  bool _validateHandshake(uint8_t flags);
  /** @endcond */
private:
  char *m_secKey{ nullptr };

  onOpenCallback _onOpen{ nullptr };
  onErrorCallback _onError{ nullptr };
};

/** @example ./simple-client/simple-client.ino
 * Example usage of WebSocketClient class
 */

} // namespace net