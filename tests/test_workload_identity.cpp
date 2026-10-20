#include "workload_identity.hpp"

#include <azure/core/context.hpp>
#include <azure/core/credentials/credentials.hpp>
#include <azure/core/credentials/token_credential_options.hpp>

#include <gtest/gtest.h>

#include <cstdlib>
#include <fstream>
#include <string>

namespace {

// Sets an environment variable for the lifetime of the object and restores the old value.
class ScopedEnv
{
public:
  ScopedEnv(char const* name, char const* value) : m_name(name)
  {
    if (char const* const old = std::getenv(name); old != nullptr)
    {
      m_hadValue = true;
      m_oldValue = old;
    }

    if (value == nullptr)
    {
      ::unsetenv(m_name.c_str());
    }
    else
    {
      ::setenv(m_name.c_str(), value, 1);
    }
  }

  ~ScopedEnv()
  {
    if (m_hadValue)
    {
      ::setenv(m_name.c_str(), m_oldValue.c_str(), 1);
    }
    else
    {
      ::unsetenv(m_name.c_str());
    }
  }

private:
  std::string m_name;
  std::string m_oldValue;
  bool m_hadValue{false};
};

class TempFile
{
public:
  explicit TempFile(std::string const& contents)
  {
    m_path = "poc-azure-cmake-test-token.tmp";
    std::ofstream file(m_path, std::ios::binary);
    file << contents;
  }

  ~TempFile() { std::remove(m_path.c_str()); }

  std::string const& Path() const { return m_path; }

private:
  std::string m_path;
};

} // namespace

TEST(WorkloadIdentity, ReadsTheEnvironment)
{
  ScopedEnv tenant("AZURE_TENANT_ID", "11111111-2222-3333-4444-555555555555");
  ScopedEnv client("AZURE_CLIENT_ID", "66666666-7777-8888-9999-000000000000");
  ScopedEnv tokenFile("AZURE_FEDERATED_TOKEN_FILE", "/var/run/secrets/azure/token");
  ScopedEnv authority("AZURE_AUTHORITY_HOST", "https://login.microsoftonline.com/");

  auto const environment = poc::ReadWorkloadIdentityEnvironment();

  EXPECT_EQ(environment.tenantId, "11111111-2222-3333-4444-555555555555");
  EXPECT_EQ(environment.clientId, "66666666-7777-8888-9999-000000000000");
  EXPECT_EQ(environment.tokenFilePath, "/var/run/secrets/azure/token");
  EXPECT_EQ(environment.authorityHost, "https://login.microsoftonline.com/");
  EXPECT_TRUE(environment.IsComplete());
  EXPECT_TRUE(environment.MissingSettings().empty());
}

TEST(WorkloadIdentity, ReportsWhatIsMissing)
{
  ScopedEnv tenant("AZURE_TENANT_ID", nullptr);
  ScopedEnv client("AZURE_CLIENT_ID", "66666666-7777-8888-9999-000000000000");
  ScopedEnv tokenFile("AZURE_FEDERATED_TOKEN_FILE", nullptr);

  auto const environment = poc::ReadWorkloadIdentityEnvironment();

  EXPECT_FALSE(environment.IsComplete());

  auto const missing = environment.MissingSettings();
  ASSERT_EQ(missing.size(), 2U);
  EXPECT_EQ(missing[0], "AZURE_TENANT_ID");
  EXPECT_EQ(missing[1], "AZURE_FEDERATED_TOKEN_FILE");
}

TEST(WorkloadIdentity, TheTokenFileHasToExist)
{
  poc::WorkloadIdentityEnvironment environment;
  environment.tokenFilePath = "/this/does/not/exist";
  EXPECT_FALSE(environment.TokenFileExists());

  TempFile const file("a.jwt.token");
  environment.tokenFilePath = file.Path();
  EXPECT_TRUE(environment.TokenFileExists());
}

TEST(WorkloadIdentity, MapsTheSettingsOntoTheCredentialOptions)
{
  poc::CredentialSettings settings;
  settings.environment.tenantId = "tenant";
  settings.environment.clientId = "client";
  settings.environment.tokenFilePath = "/tmp/token";
  settings.environment.authorityHost = "https://login.microsoftonline.us/";

  auto const options = poc::MakeCredentialOptions(settings);

  EXPECT_EQ(options.TenantId, "tenant");
  EXPECT_EQ(options.ClientId, "client");
  EXPECT_EQ(options.TokenFilePath, "/tmp/token");
  EXPECT_EQ(options.AuthorityHost, "https://login.microsoftonline.us/");
  EXPECT_FALSE(options.Transport.DisableTlsCertificateValidation);
}

TEST(WorkloadIdentity, KeepsTheDefaultAuthorityHostWhenNoneIsGiven)
{
  poc::CredentialSettings settings;

  auto const options = poc::MakeCredentialOptions(settings);

  EXPECT_EQ(options.AuthorityHost, "https://login.microsoftonline.com/");
}

TEST(WorkloadIdentity, TheTlsSwitchOnlyReachesTheTransport)
{
  poc::CredentialSettings settings;
  settings.disableTlsValidation = true;

  auto const options = poc::MakeCredentialOptions(settings);

  EXPECT_TRUE(options.Transport.DisableTlsCertificateValidation);
}

// The SDK does not complain when the credential is built from an incomplete environment, it
// complains when the token is requested. That is why the error shows up in a container that
// was never wired up for workload identity and not at startup.
TEST(WorkloadIdentity, AnIncompleteEnvironmentFailsWhenTheTokenIsRequested)
{
  Azure::Identity::WorkloadIdentityCredentialOptions options;
  Azure::Identity::WorkloadIdentityCredential credential(options);

  Azure::Core::Credentials::TokenRequestContext requestContext;
  requestContext.Scopes.emplace_back("https://management.azure.com/.default");

  EXPECT_THROW(
      credential.GetToken(requestContext, Azure::Core::Context()),
      Azure::Core::Credentials::AuthenticationException);
}
