#include "jwt.hpp"

#include <gtest/gtest.h>

namespace {

// "hello" in base64url, without padding.
constexpr char kHello[] = "aGVsbG8";

// {"alg":"RS256"} / {"aud":"api://AzureADTokenExchange"} / "signature"
constexpr char kToken[] = "eyJhbGciOiJSUzI1NiJ9."
                         "eyJhdWQiOiJhcGk6Ly9BenVyZUFEVG9rZW5FeGNoYW5nZSJ9."
                         "c2lnbmF0dXJl";

} // namespace

TEST(Jwt, Base64UrlDecodesWithoutPadding)
{
  auto const decoded = poc::Base64UrlDecode(kHello);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(*decoded, "hello");
}

TEST(Jwt, Base64UrlToleratesPadding)
{
  auto const decoded = poc::Base64UrlDecode("aGVsbG8=");
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(*decoded, "hello");
}

TEST(Jwt, Base64UrlRejectsTheStandardAlphabet)
{
  // '+' and '/' belong to standard base64, not to base64url.
  EXPECT_FALSE(poc::Base64UrlDecode("a+b/c").has_value());
}

TEST(Jwt, Base64UrlRejectsAnImpossibleLength)
{
  // Four base64 characters carry three bytes, so a single leftover character is never valid.
  EXPECT_FALSE(poc::Base64UrlDecode("a").has_value());
}

TEST(Jwt, DecodesAThreePartToken)
{
  auto const decoded = poc::DecodeJwt(kToken);
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded->headerJson, R"({"alg":"RS256"})");
  EXPECT_EQ(decoded->payloadJson, R"({"aud":"api://AzureADTokenExchange"})");
  EXPECT_TRUE(decoded->hasSignature);
}

TEST(Jwt, RejectsATokenWithoutASignaturePart)
{
  auto const decoded = poc::DecodeJwt("eyJhbGciOiJSUzI1NiJ9.eyJhdWQiOiJhcGk6Ly9BenVyZUFEVG9rZW5FeGNoYW5nZSJ9.");
  ASSERT_TRUE(decoded.has_value());
  EXPECT_FALSE(decoded->hasSignature);
}

TEST(Jwt, RejectsATokenWithTooFewParts)
{
  EXPECT_FALSE(poc::DecodeJwt("a.b").has_value());
  EXPECT_FALSE(poc::DecodeJwt("onlyonepart").has_value());
}
