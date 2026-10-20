#include "cli.hpp"

#include <string>

namespace poc {
namespace {

// Reads the value of "--name value" or "--name=value". Returns false when the value is missing.
bool TakeValue(
    int argc,
    char const* const* argv,
    int& index,
    std::string const& name,
    std::string const& inlineValue,
    bool hasInlineValue,
    std::string& out,
    std::string& error)
{
  if (hasInlineValue)
  {
    out = inlineValue;
    return true;
  }

  if (index + 1 >= argc)
  {
    error = "missing value for " + name;
    return false;
  }

  ++index;
  out = argv[index];
  return true;
}

} // namespace

CommandLine ParseCommandLine(int argc, char const* const* argv)
{
  CommandLine parsed;

  for (int i = 1; i < argc; ++i)
  {
    std::string const argument = argv[i] == nullptr ? std::string() : std::string(argv[i]);

    std::string name = argument;
    std::string inlineValue;
    bool hasInlineValue = false;

    if (argument.rfind("--", 0) == 0)
    {
      auto const equals = argument.find('=');
      if (equals != std::string::npos)
      {
        name = argument.substr(0, equals);
        inlineValue = argument.substr(equals + 1);
        hasInlineValue = true;
      }
    }

    if (name == "--help" || name == "-h")
    {
      parsed.command = CommandLine::Command::Help;
      return parsed;
    }
    else if (name == "--insecure-tls")
    {
      parsed.insecureTls = true;
    }
    else if (name == "--scope")
    {
      if (!TakeValue(argc, argv, i, name, inlineValue, hasInlineValue, parsed.scope, parsed.error))
      {
        parsed.ok = false;
        return parsed;
      }
    }
    else if (name == "--tenant-id")
    {
      if (!TakeValue(
              argc,
              argv,
              i,
              name,
              inlineValue,
              hasInlineValue,
              parsed.tenantId,
              parsed.error))
      {
        parsed.ok = false;
        return parsed;
      }
    }
    else if (name == "--client-id")
    {
      if (!TakeValue(
              argc,
              argv,
              i,
              name,
              inlineValue,
              hasInlineValue,
              parsed.clientId,
              parsed.error))
      {
        parsed.ok = false;
        return parsed;
      }
    }
    else if (name == "--token-file")
    {
      if (!TakeValue(
              argc,
              argv,
              i,
              name,
              inlineValue,
              hasInlineValue,
              parsed.tokenFilePath,
              parsed.error))
      {
        parsed.ok = false;
        return parsed;
      }
    }
    else if (name == "--authority-host")
    {
      if (!TakeValue(
              argc,
              argv,
              i,
              name,
              inlineValue,
              hasInlineValue,
              parsed.authorityHost,
              parsed.error))
      {
        parsed.ok = false;
        return parsed;
      }
    }
    else if (!argument.empty() && argument[0] == '-')
    {
      parsed.ok = false;
      parsed.error = "unknown option " + argument;
      return parsed;
    }
    else if (parsed.command == CommandLine::Command::Help)
    {
      if (argument == "check")
      {
        parsed.command = CommandLine::Command::Check;
      }
      else if (argument == "token")
      {
        parsed.command = CommandLine::Command::Token;
      }
      else
      {
        parsed.ok = false;
        parsed.error = "unknown command " + argument;
        return parsed;
      }
    }
    else
    {
      parsed.ok = false;
      parsed.error = "unexpected argument " + argument;
      return parsed;
    }
  }

  return parsed;
}

std::string UsageText()
{
  return "poc-azure-cmake - Azure workload identity example\n"
         "\n"
         "Usage:\n"
         "  poc-azure-cmake check [options]   report whether the environment can do workload identity\n"
         "  poc-azure-cmake token [options]   exchange the federated token for an access token\n"
         "\n"
         "Options:\n"
         "  --scope <scope>            scope to request (default https://management.azure.com/.default)\n"
         "  --tenant-id <id>           overrides AZURE_TENANT_ID\n"
         "  --client-id <id>           overrides AZURE_CLIENT_ID\n"
         "  --token-file <path>        overrides AZURE_FEDERATED_TOKEN_FILE\n"
         "  --authority-host <url>     overrides AZURE_AUTHORITY_HOST (must be https)\n"
         "  --insecure-tls             skip TLS verification, only for the local mock\n"
         "  -h, --help                 print this text\n";
}

} // namespace poc
