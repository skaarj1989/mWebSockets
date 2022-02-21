#include "WebSocket.hpp"
#include "CryptoLegacy/SHA1.h"
#include "base64/Base64.h"

namespace net {

void generateSecKey(char output[]) {
  constexpr auto kLength = 16;
  char temp[kLength + 1]{};

  randomSeed(analogRead(0));
  for (uint8_t i{0}; i < kLength; ++i)
    temp[i] = static_cast<char>(random(0xFF));

  base64_encode(output, temp, kLength);
}

void encodeSecKey(const char *key, char output[]) {
  constexpr auto kSecKeyLength = 24;
  constexpr char kMagicString[]{"258EAFA5-E914-47DA-95CA-C5AB0DC85B11"};
  constexpr auto kMagicStringLength = sizeof(kMagicString) - 1;

  constexpr auto kBufferLength = kSecKeyLength + kMagicStringLength;
  char buffer[kBufferLength + 1]{};
  memcpy(&buffer[0], key, kSecKeyLength);
  memcpy(&buffer[kSecKeyLength], kMagicString, kMagicStringLength);

  SHA1 sha1;
  sha1.update(buffer, kBufferLength);
  sha1.finalize(buffer, 20);
  base64_encode(output, buffer, 20);
}

void generateMask(char output[]) {
  randomSeed(analogRead(0));
  for (uint8_t i{0}; i < 4; ++i)
    output[i] = static_cast<char>(random(0xFF));
}

} // namespace net
