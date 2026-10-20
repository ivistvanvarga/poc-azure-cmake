#include "workload_identity.hpp"

#include <azure/core/context.hpp>
#include <azure/core/credentials/token_credential_options.hpp>

#include <cstdlib>
#include <fstream>
#include <string>

namespace poc {
namespace {

std::string GetEnv(char const* name)
{
  char const* const value = std::getenv(name);
  return value == nullptr ? std::string() : std::string(value);
}

} // namespace

bool WorkloadIdentityEnvironment::IsComplete() const
{
  return !tenantId.empty() && !clientId.empty() && !tokenFilePath.empty();
}

std::vector<std::string> WorkloadIdentityEnvironment::MissingSettings() const
{
  std::vector<std::string> missing;
  if (tenantId.empty())
  {
    missing.emplace_back("AZURE_TENANT_ID");
  }
  if (clientId.empty())
  {
    missing.emplace_back("AZURE_CLIENT_ID");
  }
  if (tokenFilePath.empty())
  {
    missing.emplace_back("AZURE_FEDERATED_TOKEN_FILE");
  }
  return missing;
}

bool WorkloadIdentityEnvironment::TokenFileExists() const
{
  if (tokenFilePath.empty())
  {
    return false;
  }

  std::ifstream file(tokenFilePath, std::ios::binary);
  return file.good();
}

WorkloadIdentityEnvironment ReadWorkloadIdentityEnvironment()
{
  WorkloadIdentityEnvironment environment;
  environment.tenantId = GetEnv("AZURE_TENANT_ID");
  environment.clientId = GetEnv("AZURE_CLIENT_ID");
  environment.tokenFilePath = GetEnv("AZURE_FEDERATED_TOKEN_FILE");
  environment.authorityHost = GetEnv("AZURE_AUTHORITY_HOST");
  return environment;
}

Azure::Identity::WorkloadIdentityCredentialOptions MakeCredentialOptions(
    CredentialSettings const& settings)
{
  Azure::Identity::WorkloadIdentityCredentialOptions options;
  options.TenantId = settings.environment.tenantId;
  options.ClientId = settings.environment.clientId;
  options.TokenFilePath = settings.environment.tokenFilePath;

  // Leaving AuthorityHost alone keeps the SDK default, which is the global Entra cloud.
  if (!settings.environment.authorityHost.empty())
  {
    options.AuthorityHost = settings.environment.authorityHost;
  }

  if (settings.disableTlsValidation)
  {
    options.Transport.DisableTlsCertificateValidation = true;
  }

  return options;
}

AcquiredToken AcquireToken(CredentialSettings const& settings, std::string const& scope)
{
  auto const options = MakeCredentialOptions(settings);
  Azure::Identity::WorkloadIdentityCredential credential(options);

  Azure::Core::Credentials::TokenRequestContext requestContext;
  requestContext.Scopes.emplace_back(scope);

  auto const accessToken = credential.GetToken(requestContext, Azure::Core::Context());

  AcquiredToken acquired;
  acquired.token = accessToken.Token;
  acquired.expiresOn = accessToken.ExpiresOn;
  acquired.scope = scope;
  acquired.tenantId = settings.environment.tenantId;
  acquired.clientId = settings.environment.clientId;
  return acquired;
}

} // namespace poc
