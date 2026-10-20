#pragma once

#include <string>

namespace poc {

struct CommandLine
{
  enum class Command
  {
    Check,
    Token,
    Help
  };

  Command command{Command::Help};
  std::string scope{"https://management.azure.com/.default"};
  std::string tenantId;
  std::string clientId;
  std::string tokenFilePath;
  std::string authorityHost;
  bool insecureTls{false};
  bool ok{true};
  std::string error;
};

// Parses the command line. argv[0] is the program name and is ignored.
CommandLine ParseCommandLine(int argc, char const* const* argv);

// The usage text, also printed on a parse error.
std::string UsageText();

} // namespace poc
