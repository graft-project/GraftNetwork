#pragma once
#include <string>

namespace crypto
{
struct ed25519_public_key;
};

namespace base32z
{
bool decode(std::string const &src, crypto::ed25519_public_key &dest);

/// adapted from i2pd
template <typename stack_t>
const char *encode(std::string const &src, stack_t &stack)
{
  // from  https://en.wikipedia.org/wiki/Base32#z-base-32
  static const char zbase32_alpha[] = {'y', 'b', 'n', 'd', 'r', 'f', 'g', '8', 'e', 'j', 'k', 'm', 'c', 'p', 'q', 'x',
                                       'o', 't', '1', 'u', 'w', 'i', 's', 'z', 'a', '3', '4', '5', 'h', '7', '6', '9'};

  size_t ret = 0, pos = 1;
  int bits     = 8;
  uint32_t tmp = src[0];
  size_t len   = sizeof(src);
  while (ret < sizeof(stack) && (bits > 0 || pos < len))
  {
    if (bits < 5)
    {
      if (pos < len)
      {
        tmp <<= 8;
        tmp |= src[pos] & 0xFF;
        pos++;
        bits += 8;
      }
      else // last byte
      {
        tmp <<= (5 - bits);
        bits = 5;
      }
    }

    bits -= 5;
    int ind = (tmp >> bits) & 0x1F;
    if (ret < sizeof(stack))
    {
      stack[ret] = zbase32_alpha[ind];
      ret++;
    }
    else
      return nullptr;
  }
  return &stack[0];
}
};
