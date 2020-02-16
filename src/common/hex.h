#pragma once

namespace hex
{
  constexpr bool char_is_hex(char c)
  {
    bool result = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    return result;
  }

  constexpr char from_hex_digit(char x) {
      return
          (x >= '0' && x <= '9') ? x - '0' :
          (x >= 'a' && x <= 'f') ? x - ('a' - 10):
          (x >= 'A' && x <= 'F') ? x - ('A' - 10):
          0;
  }
  constexpr char from_hex_pair(char a, char b) { return (from_hex_digit(a) << 4) | from_hex_digit(b); }

  // Creates a string from a character sequence of hex digits.  Undefined behaviour if any characters
  // are not in [0-9a-fA-F] or if the input sequence length is not even.
  template <typename It>
  std::string from_hex(It begin, It end) {
      using std::distance;
      using std::next;
      assert(distance(begin, end) % 2 == 0);
      std::string raw;
      raw.reserve(distance(begin, end) / 2);
      while (begin != end) {
          char a = *begin++;
          char b = *begin++;
          raw += from_hex_pair(a, b);
      }
      return raw;
  }
}
