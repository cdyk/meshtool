#pragma once
#include <cinttypes>


uint16_t buildHalf(unsigned sign, unsigned exp, unsigned mantissa)
{
  return (sign << 15) | (exp << 10) | mantissa;
}

uint16_t halfFromFloat(float f)
{
  union {
    float f;
    unsigned u;
  } v;
  v.f = f;
  auto sign = v.u >> 31;
  auto exponent = (v.u >> 23) & 0xFF;
  auto mantissa = v.u & ((1 << 23) - 1);

  if (exponent < 127 - 24) {
    return buildHalf(sign, 0, 0); // flush to zero
  }
  else if (exponent < 127 - 14) {
    auto shift = (23 - 10) + 127 - 15 + 1 - exponent;
    mantissa |= 1 << 23;  // add implicit one.
    return buildHalf(sign, 0, mantissa >> shift);
  }
  else if (exponent < 127 + 16) {
    auto exp = exponent - (127 - 15);
    return buildHalf(sign, exp, mantissa >> (23 - 10));
  }
  else if (exponent < 255 || mantissa == 0) {
    return buildHalf(sign, 0x1F, 0);  // infinity
  }
  else {
    return buildHalf(sign, 0x1F, 1);  // nan
  }
}
