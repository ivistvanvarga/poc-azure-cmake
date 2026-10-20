#pragma once

#include <optional>
#include <string>

namespace poc {

// A decoded compact JWS ("header.payload.signature").
struct DecodedJwt
{
  std::string headerJson;
  std::string payloadJson;
  bool hasSignature{false};
};

// Decodes base64url (RFC 4648 section 5) without padding. Padding characters are tolerated but
// not required. Returns std::nullopt when the input contains characters outside the alphabet.
std::optional<std::string> Base64UrlDecode(std::string const& input);

// Splits a compact JWS into its three parts and decodes the header and the payload.
// Returns std::nullopt when the input is not a three part token or when a part is not base64url.
std::optional<DecodedJwt> DecodeJwt(std::string const& compact);

} // namespace poc
