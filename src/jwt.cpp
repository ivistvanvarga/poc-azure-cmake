#include "jwt.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace poc {
namespace {

constexpr char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

int DecodeChar(char c)
{
  if (c >= 'A' && c <= 'Z')
  {
    return c - 'A';
  }
  if (c >= 'a' && c <= 'z')
  {
    return c - 'a' + 26;
  }
  if (c >= '0' && c <= '9')
  {
    return c - '0' + 52;
  }
  if (c == '-')
  {
    return 62;
  }
  if (c == '_')
  {
    return 63;
  }
  return -1;
}

} // namespace

std::optional<std::string> Base64UrlDecode(std::string const& input)
{
  // A group of four base64 characters carries three bytes, so a remainder of one character can
  // never be produced by a valid encoder.
  if (input.size() % 4 == 1)
  {
    return std::nullopt;
  }

  std::string out;
  out.reserve((input.size() * 3) / 4);

  std::uint32_t buffer = 0;
  int bits = 0;

  for (char const c : input)
  {
    if (c == '=')
    {
      break;
    }

    int const value = DecodeChar(c);
    if (value < 0)
    {
      return std::nullopt;
    }

    buffer = (buffer << 6) | static_cast<std::uint32_t>(value);
    bits += 6;

    if (bits >= 8)
    {
      bits -= 8;
      out.push_back(static_cast<char>((buffer >> bits) & 0xFF));
      buffer &= (1U << bits) - 1U;
    }
  }

  return out;
}

std::optional<DecodedJwt> DecodeJwt(std::string const& compact)
{
  std::vector<std::string> parts;
  std::string current;

  for (char const c : compact)
  {
    if (c == '.')
    {
      parts.push_back(current);
      current.clear();
      continue;
    }
    current.push_back(c);
  }
  parts.push_back(current);

  if (parts.size() != 3)
  {
    return std::nullopt;
  }

  auto const header = Base64UrlDecode(parts[0]);
  auto const payload = Base64UrlDecode(parts[1]);
  if (!header || !payload)
  {
    return std::nullopt;
  }

  DecodedJwt decoded;
  decoded.headerJson = *header;
  decoded.payloadJson = *payload;
  decoded.hasSignature = !parts[2].empty();
  return decoded;
}

} // namespace poc
