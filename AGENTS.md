# AGENTS.md

## What this is

A CMake project that uses the **Azure SDK for C++** to do a workload identity token exchange, the
way a pod on AKS would. It is a proof of concept, the point is the plumbing around the SDK, not a
library.

- `src/main.cpp` — the two commands, `check` (no network) and `token` (the real exchange)
- `src/workload_identity.*` — reads the environment, builds the credential, acquires the token
- `src/jwt.*` — base64url and JWT decoding, hand written, no dependency
- `src/cli.*` — argument parsing
- `tests/` — unit tests, they never touch the network
- `tools/mock_entra.py` — a local stand-in for the Microsoft Entra token endpoint
- `scripts/demo-offline.sh` — the whole flow without an Azure subscription
- `cmake/` — how the Azure SDK and nlohmann/json are fetched, GoogleTest is fetched in
  `tests/CMakeLists.txt`

## Build and test

```bash
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Prerequisites that are not always present: `gcc-c++`, `libcurl-devel`, `openssl-devel`
(`dnf install gcc-c++ libcurl-devel openssl-devel`). The first configure downloads the Azure SDK,
GoogleTest and nlohmann/json, so it needs network; it takes about 20 seconds, the first build
about a minute with 8 cores.

Run the end to end demo by hand with `./scripts/demo-offline.sh`, it is also the
`OfflineWorkloadIdentityFlow` test.

## The Azure SDK for C++ dependency

`cmake/azure-sdk.cmake` fetches the SDK from source. Every step in it exists because of a way the
SDK bites:

- The package directories were renamed. `sdk/core/azure-core-cpp` is now `sdk/core/azure-core`
  and `sdk/identity/azure-identity-cpp` is now `sdk/identity/azure-identity`. The old paths do
  not exist any more, `SOURCE_SUBDIR` with them fails the configure step.
- The tags are per package now (`azure-identity_1.13.3`, `azure-core_1.16.4`, ...), there is no
  monorepo tag any more. The pinned tag `azure-identity_1.13.3` carries azure-core 1.16.2.
- **Without `AZURE_SDK_DISABLE_AUTO_VCPKG` in the environment the SDK clones vcpkg and re-runs
  the configure step** (`az_vcpkg_integrate()` in `cmake-modules/AzureVcpkg.cmake`). That is slow
  and needs a compiler it manages itself. The variable is set in `cmake/azure-sdk.cmake`.
- **Without `AZ_ALL_LIBRARIES=ON` azure-identity calls `find_package(azure-core-cpp)`** and the
  configure step fails, because there is no installed copy of it.
- azure-core has to be added before azure-identity, the identity package links
  `Azure::azure-core`.
- `BUILD_TESTING` is forced to `OFF` before the SDK is added, otherwise the SDK builds its own
  test suites. The tests of this project are registered separately, so `ctest` still works.
- On Linux and macOS the libcurl transport is not optional, `AzureTransportAdapters.cmake` turns
  it on unconditionally. The **headers** are needed, the runtime library is not enough.
  `cmake/libcurl.cmake` fails the configure step with a hint when they are missing.
- The SDK sets `CMAKE_CXX_STANDARD 14` for itself. This project uses C++17, both work.

## Azure SDK API details that cost time

- `DateTime` lives in namespace `Azure`, not `Azure::Core`, and `DateFormat` is nested in it:
  `Azure::DateTime::DateFormat::Rfc3339`. `Azure::DateTime` is not a `system_clock::time_point`,
  build one from `std::chrono::system_clock::now()` before subtracting.
- `RequestFailedException` is `Azure::Core::RequestFailedException`, not in the `Http`
  namespace.
- `WorkloadIdentityCredential` **does not fail when it is constructed** from an incomplete
  environment, it only logs a warning. The failure comes from `GetToken`, which throws
  `AuthenticationException`. A container that was never wired up therefore fails on the first
  token request and not at startup.
- The authority host has to be `https`, `ClientCredentialCore::GetRequestUrl` rejects anything
  else. A plain HTTP mock is not possible, the mock in `tools/mock_entra.py` serves TLS.
- The client assertion is posted as it is, the content of the token file. There is no local
  signing, the key never touches the process.
- `DefaultAzureCredential` chains `EnvironmentCredential`, `WorkloadIdentityCredential`,
  `ManagedIdentityCredential` and `AzureCliCredential`. This project uses the workload identity
  credential directly, the chain is only mentioned for context.

## Workload identity in practice

- The three required settings are `AZURE_TENANT_ID`, `AZURE_CLIENT_ID` and
  `AZURE_FEDERATED_TOKEN_FILE`, `AZURE_AUTHORITY_HOST` is optional and defaults to the global
  cloud.
- The projected token is only valid for the audience `api://AzureADTokenExchange`. A wrong
  audience is the usual reason a federation does not work, so `check` prints the claims.
- The tenant id has to be alphanumeric with periods or hyphens, the SDK validates it and warns
  when it is not.
- The access token is a bearer credential. The program prints its claims and its length, never
  the token.

## Testing quirks

- The unit tests are offline by design, they cover the environment handling, the option mapping
  and the lazy failure of the credential.
- `gtest_discover_tests` discovers the tests after the test binary is built (POST_BUILD, the
  default). On a fresh checkout `ctest` therefore lists no unit tests until `cmake --build` ran.
- The offline demo test needs `bash`, `python3` and `openssl`, and a free TCP port. It is only
  registered when all three are found.
- `tools/mock_entra.py` logs every request to stderr, that is how the demo shows what the SDK
  actually sent.

## Conventions

- The project follows the style of the sibling repositories: a flat `src/`, a `tests/` directory
  with one file per area, `cmake/` for the dependency wiring.
- Comments explain why something is the way it is, not what the line does.
- The token is never printed, not even partially.
