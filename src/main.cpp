#include "cli.hpp"
#include "jwt.hpp"
#include "workload_identity.hpp"

#include <azure/core/credentials/credentials.hpp>
#include <azure/core/exception.hpp>

#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

using json = nlohmann::json;

void Field(std::string const& label, std::string const& value)
{
  std::cout << "  " << std::left << std::setw(21) << label << " : " << value << "\n";
}

void Field(std::string const& label, bool value)
{
  Field(label, std::string(value ? "yes" : "no"));
}

std::string ReadWholeFile(std::string const& path)
{
  std::ifstream file(path, std::ios::binary);
  if (!file.good())
  {
    return std::string();
  }

  std::ostringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

// Prints the claims that matter for a token. A claim that is not there is simply skipped, the
// set of claims differs between the projected service account token and an access token.
void PrintClaims(json const& claims, std::vector<std::string> const& keys, std::string const& indent)
{
  for (auto const& key : keys)
  {
    auto const found = claims.find(key);
    if (found == claims.end())
    {
      continue;
    }

    std::string value = found->is_string() ? found->get<std::string>() : found->dump();
    std::cout << indent << key << " = " << value << "\n";
  }
}

// Decodes a compact JWS and prints its interesting claims. Never prints the token itself.
void PrintJwt(std::string const& label, std::string const& compact)
{
  auto const decoded = poc::DecodeJwt(compact);
  if (!decoded)
  {
    std::cout << "  " << label << " : not a JWT\n";
    return;
  }

  auto const header = json::parse(decoded->headerJson, nullptr, false);
  auto const payload = json::parse(decoded->payloadJson, nullptr, false);

  std::cout << "  " << std::left << std::setw(21) << label << " :\n";
  if (!header.is_discarded())
  {
    PrintClaims(header, {"alg", "typ", "kid"}, "      ");
  }
  if (!payload.is_discarded())
  {
    PrintClaims(payload, {"aud", "iss", "sub", "tid", "appid", "azp", "exp"}, "      ");
  }
}

poc::WorkloadIdentityEnvironment ResolveEnvironment(poc::CommandLine const& cli)
{
  auto environment = poc::ReadWorkloadIdentityEnvironment();

  // An explicit option wins over the environment, which is what the offline demo needs.
  if (!cli.tenantId.empty())
  {
    environment.tenantId = cli.tenantId;
  }
  if (!cli.clientId.empty())
  {
    environment.clientId = cli.clientId;
  }
  if (!cli.tokenFilePath.empty())
  {
    environment.tokenFilePath = cli.tokenFilePath;
  }
  if (!cli.authorityHost.empty())
  {
    environment.authorityHost = cli.authorityHost;
  }

  return environment;
}

int RunCheck(poc::CommandLine const& cli)
{
  auto const environment = ResolveEnvironment(cli);

  std::cout << "workload identity environment\n";
  Field("tenant id", environment.tenantId.empty() ? "<unset>" : environment.tenantId);
  Field("client id", environment.clientId.empty() ? "<unset>" : environment.clientId);
  Field(
      "authority host",
      environment.authorityHost.empty() ? "https://login.microsoftonline.com/ (default)"
                                         : environment.authorityHost);
  Field("token file", environment.tokenFilePath.empty() ? "<unset>" : environment.tokenFilePath);
  Field("token file readable", environment.TokenFileExists());

  if (environment.TokenFileExists())
  {
    auto const contents = ReadWholeFile(environment.tokenFilePath);
    Field("token file size", std::to_string(contents.size()) + " bytes");
    PrintJwt("federated token", contents);
  }

  auto const missing = environment.MissingSettings();
  if (missing.empty())
  {
    Field("verdict", std::string("READY"));
    return 0;
  }

  std::string list;
  for (std::size_t i = 0; i < missing.size(); ++i)
  {
    list += (i == 0 ? "" : ", ") + missing[i];
  }
  Field("verdict", "NOT READY, missing " + list);
  return 1;
}

int RunToken(poc::CommandLine const& cli)
{
  auto const environment = ResolveEnvironment(cli);

  poc::CredentialSettings settings;
  settings.environment = environment;
  settings.disableTlsValidation = cli.insecureTls;

  std::cout << "requesting an access token\n";
  Field("scope", cli.scope);
  Field("tenant id", environment.tenantId.empty() ? "<unset>" : environment.tenantId);
  Field("client id", environment.clientId.empty() ? "<unset>" : environment.clientId);
  Field(
      "authority host",
      environment.authorityHost.empty() ? "https://login.microsoftonline.com/ (default)"
                                         : environment.authorityHost);

  try
  {
    auto const acquired = poc::AcquireToken(settings, cli.scope);

    auto const now = Azure::DateTime(std::chrono::system_clock::now());
    auto const remaining = std::chrono::duration_cast<std::chrono::seconds>(
        acquired.expiresOn - now);

    std::cout << "\naccess token\n";
    Field("expires on", acquired.expiresOn.ToString(Azure::DateTime::DateFormat::Rfc3339));
    Field("expires in", std::to_string(remaining.count()) + " s");
    Field("length", std::to_string(acquired.token.size()) + " bytes");

    auto const decoded = poc::DecodeJwt(acquired.token);
    if (decoded)
    {
      auto const payload = json::parse(decoded->payloadJson, nullptr, false);
      if (!payload.is_discarded())
      {
        std::cout << "  " << std::left << std::setw(21) << "claims" << " :\n";
        PrintClaims(payload, {"aud", "iss", "tid", "appid", "azp", "exp"}, "      ");
      }
    }
    else
    {
      std::cout << "  claims           : the access token is not a JWT\n";
    }

    std::cout << "\nthe token itself is not printed, it is a bearer credential\n";
    return 0;
  }
  catch (Azure::Core::Credentials::AuthenticationException const& e)
  {
    std::cerr << "\nauthentication failed: " << e.what() << "\n";
    auto const missing = environment.MissingSettings();
    if (!missing.empty())
    {
      std::cerr << "missing: ";
      for (std::size_t i = 0; i < missing.size(); ++i)
      {
        std::cerr << (i == 0 ? "" : ", ") << missing[i];
      }
      std::cerr << "\n";
    }
    return 1;
  }
  catch (Azure::Core::RequestFailedException const& e)
  {
    std::cerr << "\nthe token request failed: " << e.what() << "\n";
    return 1;
  }
  catch (std::exception const& e)
  {
    std::cerr << "\nunexpected failure: " << e.what() << "\n";
    return 1;
  }
}

} // namespace

int main(int argc, char** argv)
{
  auto const cli = poc::ParseCommandLine(argc, argv);

  if (!cli.ok)
  {
    std::cerr << cli.error << "\n\n" << poc::UsageText();
    return 2;
  }

  switch (cli.command)
  {
    case poc::CommandLine::Command::Check:
      return RunCheck(cli);
    case poc::CommandLine::Command::Token:
      return RunToken(cli);
    case poc::CommandLine::Command::Help:
      break;
  }

  std::cout << poc::UsageText();
  return 0;
}
