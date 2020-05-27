#pragma once

#include "WebSocket.h"
#include "utility.h"

namespace net {

class WebSocketServer {
public:
  using verifyClientCallback = bool (*)(
    const IPAddress &ip, const char *header, const char *value);
  using onConnectionCallback = void (*)(WebSocket &ws);

public:
  WebSocketServer(uint16_t port);
  ~WebSocketServer();

  void begin(const verifyClientCallback &callback = nullptr);
  void shutdown();

  void broadcast(
    const WebSocket::DataType &dataType, const char *message, uint16_t length);

  void listen();

  uint8_t countClients();

  void onConnection(const onConnectionCallback &callback);

private:
  WebSocket *_getWebSocket(NetClient &client) const;

  bool _handleRequest(NetClient &client);
  void _rejectRequest(NetClient &client, const WebSocketError &code);

  void _cleanDeadConnections();

private:
  NetServer m_server;
  WebSocket *m_sockets[MAX_CONNECTIONS]{};

  verifyClientCallback _verifyClient{ nullptr };
  onConnectionCallback _onConnection{ nullptr };
};

} // namespace net