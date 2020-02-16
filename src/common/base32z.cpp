#include "base32z.h"
#include "crypto/crypto.h"

#include <unordered_map>

namespace base32z
{
static const std::unordered_map<char, uint8_t> zbase32_reverse_alpha = {
    {'y', 0},  {'b', 1},  {'n', 2},  {'d', 3},  {'r', 4},  {'f', 5},  {'g', 6},  {'8', 7},
    {'e', 8},  {'j', 9},  {'k', 10}, {'m', 11}, {'c', 12}, {'p', 13}, {'q', 14}, {'x', 15},
    {'o', 16}, {'t', 17}, {'1', 18}, {'u', 19}, {'w', 20}, {'i', 21}, {'s', 22}, {'z', 23},
    {'a', 24}, {'3', 25}, {'4', 26}, {'5', 27}, {'h', 28}, {'7', 29}, {'6', 30}, {'9', 31}};

static size_t decode_size(size_t sz)
{
  auto d = div(sz, 5);
  if (d.rem) d.quot++;
  return 8 * d.quot;
}

bool decode(std::string const &src, crypto::ed25519_public_key &dest)
{
  int tmp = 0, bits = 0;
  size_t idx           = 0;
  const size_t len     = decode_size(sizeof(dest));
  const size_t out_len = sizeof(dest);
  for (size_t i = 0; i < len; i++)
  {
    char ch = src[i];
    if (ch)
    {
      auto itr = zbase32_reverse_alpha.find(ch);
      if (itr == zbase32_reverse_alpha.end()) return false;
      ch = itr->second;
    }
    else
    {
      return idx == out_len;
    }
    tmp |= ch;
    bits += 5;
    if (bits >= 8)
    {
      if (idx >= out_len) return false;
      dest.data[idx] = tmp >> (bits - 8);
      bits -= 8;
      idx++;
    }
    tmp <<= 5;
  }
  return idx == out_len;
}
}
