#pragma once

#include <azure/core/credentials/credentials.hpp>
#include <azure/core/datetime.hpp>
#include <azure/identity/workload_identity_credential.hpp>


#include <string>
#include <vector>

namespace poc {

// The environment a workload identity runs in. On AKS the mutating webhook injects the first
// three values and mounts the projected service account token; everywhere else you set them
// yourself.
struct WorkloadIdentityEnvironment
{
  std::string tenantId; // AZURE_TENANT_ID
  std::string clientId; // AZURE_CLIENT_ID
  std::string tokenFilePath; // AZURE_FEDERATED_TOKEN_FILE
  std::string authorityHost; // AZURE_AUTHORITY_HOST

  // True when tenant id, client id and token file are all set. The SDK needs all three.
  bool IsComplete() const;

  // The names of the required settings that are missing, in a stable order.
  std::vector<std::string> MissingSettings() const;

  // True when the federated token file exists and can be opened for reading.
  bool TokenFileExists() const;
};

// Reads the environment variables above.
WorkloadIdentityEnvironment ReadWorkloadIdentityEnvironment();

struct CredentialSettings
{
  WorkloadIdentityEnvironment environment;

  // Turns off TLS verification. Only used to talk to the local mock in tools/mock_entra.py,
  // never against real Azure.
  bool disableTlsValidation{false};
};

// Maps the settings onto the options of Azure::Identity::WorkloadIdentityCredential.
Azure::Identity::WorkloadIdentityCredentialOptions MakeCredentialOptions(
    CredentialSettings const& settings);

struct AcquiredToken
{
  std::string token; // never printed, it is a bearer credential
  Azure::DateTime expiresOn;
  std::string scope;
  std::string tenantId;
  std::string clientId;
};

// Exchanges the federated token for an Entra access token.
AcquiredToken AcquireToken(CredentialSettings const& settings, std::string const& scope);

} // namespace poc
