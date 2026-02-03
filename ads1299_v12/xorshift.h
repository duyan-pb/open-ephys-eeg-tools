#ifndef XORSHIFT_H
#define XORSHIFT_H

#include <stdint.h>

// The state must be initialized to non-zero
// Algorithm "xor" from p. 4 of Marsaglia, "Xorshift RNGs"
void xorshift32(uint32_t *state) {
  // Algorithm "xor" from p. 4 of Marsaglia, "Xorshift RNGs"
  uint32_t x = *state;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  *state = x;
}

#endif
