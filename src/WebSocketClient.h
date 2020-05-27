#pragma once

#include "WebSocket.h"

namespace net {

class WebSocketClient : public WebSocket {
public:
  using onOpenCallback = void (*)(WebSocket &ws);
  using onErrorCallback = void (*)(const WebSocketError &code);

public:
  WebSocketClient();
  ~WebSocketClient();

  bool open(const char *host, uint16_t port = 3000, const char *path = "/");
  void listen();

  void onOpen(const onOpenCallback &onOpen);
  void onError(const onErrorCallback &onError);

private:
  void _sendRequest(const char *host, uint16_t port, const char *path);
  bool _readResponse();

private:
  onOpenCallback _onOpen{ nullptr };
  onErrorCallback _onError{ nullptr };
};

} // namespace net