#pragma once

/** @file */

#include "WebSocket.hpp"

namespace net {

/** @class WebSocketClient */
template <class NetClient> class WebSocketClient : public WebSocket<NetClient> {
public:
  using onOpenCallback = void (*)(IWebSocket &);
  using onErrorCallback = void (*)(WebSocketError);

public:
  WebSocketClient() = default;
  ~WebSocketClient() = default;

  /**
   * @brief Attempts to connect to a server.
   * @remark Do not use "ws://"
   */
  bool open(
      const char *host, uint16_t port = 3000, const char *path = "/",
      const char *supportedProtocols = nullptr);

  /** @note Call this in the main loop. */
  void listen();

  /**
   * @brief Sets callback that will be called on a successfull connection.
   * @code{.cpp}
   * client.onOpen([](WebSocket &ws) {
   *   // connected ... send hello message or whatever
   * });
   * @endcode
   */
  void onOpen(const onOpenCallback &);
  /**
   * @brief Sets error handler.
   * @code{.cpp}
   * client.onError([](WebSocketError code) {
   *   switch (code) {
   *   case BAD_REQUEST:
   *     break;
   *   
   *   // ...
   *   }
   * });
   * @endcode
   */
  void onError(const onErrorCallback &);

private:
  /** @cond */
  void _sendRequest(
      const char *host, uint16_t port, const char *path, const char *secKey,
      const char *supportedProtocols);
  bool _waitForResponse(uint16_t maxAttempts, uint8_t time = 1);
  bool _readResponse(const char *secKey);
  bool _validateHandshake(uint8_t flags);

  void _triggerError(WebSocketError);
  /** @endcond */
private:
  onOpenCallback _onOpen{nullptr};
  onErrorCallback _onError{nullptr};
};

/**
 * @example ./simple-client/simple-client.ino
 * Example usage of WebSocketClient class
 */

} // namespace net

#include "WebSocketClient.inl"
