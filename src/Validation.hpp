#pragma once

#include <stdint.h>

/**
 * @see
 * https://github.com/websockets/utf-8-validate/blob/master/src/validation.c
 */
bool isValidUTF8(const uint8_t *s, uint16_t length);
