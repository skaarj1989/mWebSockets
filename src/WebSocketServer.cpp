#include "WebSocketServer.hpp"

namespace net {

bool isValidGET(char *line) {
  char *rest{line};
  for (uint8_t i{0}; rest != nullptr; ++i) {
    const auto pch = strtok_r(rest, " ", &rest);
    switch (i) {
    case 0:
      if (strcmp_P(pch, (PGM_P)F("GET")) != 0) {
        return false;
      }
      break;
    case 1:
      break; // path doesn't matter ...
    case 2:
      if (strcmp_P(pch, (PGM_P)F("HTTP/1.1")) != 0) {
        return false;
      }
      break;

    default:
      return false;
    }
  }
  return true;
}
bool isValidUpgrade(const char *value) {
  return strcasecmp_P(value, (PGM_P)F("websocket")) == 0;
}
bool isValidConnection(char *value) {
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

WebSocketError validateHandshake(uint8_t flags, const char *secKey) {
  if ((flags & (kValidConnectionHeader | kValidUpgradeHeader)) !=
      (kValidConnectionHeader | kValidUpgradeHeader)) {
    return WebSocketError::UPGRADE_REQUIRED;
  }

  if (!(flags & kValidVersion) || strlen(secKey) == 0)
    return WebSocketError::BAD_REQUEST;

  return WebSocketError::NO_ERROR;
}

} // namespace net
