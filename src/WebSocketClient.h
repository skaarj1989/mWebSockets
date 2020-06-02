#pragma once

#include "WebSocket.h"

namespace net {

class WebSocketClient : public WebSocket {
public:
  using onOpenCallback = void (*)(WebSocket &ws);
  using onErrorCallback = void (*)(const WebSocketError &code);

public:
  /**
    * Attempts to connect to server.
    * @remarks Do not use "ws://"
    */
  bool open(const char *host, uint16_t port = 3000, const char *path = "/");
  void terminate();

  void listen();

  /** Sets callback that will be called on successfull connection. */
  void onOpen(const onOpenCallback &onOpen);
  void onError(const onErrorCallback &onError);

private:
  void _sendRequest(const char *host, uint16_t port, const char *path);
  bool _readResponse();

  bool _waitForResponse(uint16_t maxAttempts, uint8_t time = 1);

private:
  char *m_secKey{ nullptr };

  onOpenCallback _onOpen{ nullptr };
  onErrorCallback _onError{ nullptr };
};

} // namespace net