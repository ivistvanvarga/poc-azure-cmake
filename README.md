# poc-azure-cmake

A proof of concept that shows the [Azure SDK for C++](https://github.com/Azure/azure-sdk-for-cpp)
doing a **workload identity** token exchange, built with CMake.

The interesting part is not the SDK, it is the plumbing around it: which environment variables
the platform injects, what the federated token actually contains, and how the SDK behaves when
the environment is not wired up.

## What it does

```
$ ./build/poc-azure-cmake check
workload identity environment
  tenant id             : 11111111-2222-3333-4444-555555555555
  client id             : 66666666-7777-8888-9999-000000000000
  authority host        : https://login.microsoftonline.com/ (default)
  token file            : /var/run/secrets/azure/tokens/azure-identity-token
  token file readable   : yes
  token file size       : 1493 bytes
  federated token       :
      alg = RS256
      aud = api://AzureADTokenExchange
      iss = https://kubernetes.default.svc.cluster.local
      sub = system:serviceaccount:default:azure-workload-identity
  verdict               : READY

$ ./build/poc-azure-cmake token
requesting an access token
  ...
access token
  expires on : 2026-10-20T19:30:00.0000000Z
  expires in : 3599 s
  length : 1287 bytes
  claims :
      aud = https://management.azure.com/
      iss = https://login.microsoftonline.com/<tenant>/v2.0
```

`check` never talks to the network. `token` performs the real exchange and prints the claims of
the access token, never the token itself.

## Requirements

- CMake >= 3.20
- a C++17 compiler
- OpenSSL and libcurl development headers, the Azure SDK needs both:
  `dnf install gcc-c++ libcurl-devel openssl-devel` (or `apt install g++ libcurl4-openssl-dev libssl-dev`)

The Azure SDK for C++ is fetched and built from source by CMake, nothing has to be installed
for it.

## Build and test

```bash
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## Run it without an Azure subscription

`scripts/demo-offline.sh` runs the whole flow against a local stand-in for the Microsoft Entra
token endpoint (`tools/mock_entra.py`), so the client assertion flow can be seen without an
Azure tenant:

```bash
./scripts/demo-offline.sh
```

It is also registered as the `OfflineWorkloadIdentityFlow` test, so `ctest` runs it.

## How workload identity works

1. An app registration gets a **federated identity credential**. On AKS the subject is usually a
   user assigned managed identity.
2. A Kubernetes service account is annotated with `azure.workload.identity/client-id` and the
   pod is labelled `azure.workload.identity/use: "true"`.
3. The Azure workload identity webhook injects the environment and mounts a projected service
   account token:

   | Variable | Meaning |
   | --- | --- |
   | `AZURE_TENANT_ID` | tenant of the service principal |
   | `AZURE_CLIENT_ID` | client id of the service principal |
   | `AZURE_FEDERATED_TOKEN_FILE` | path of the projected token |
   | `AZURE_AUTHORITY_HOST` | optional, the cloud to talk to |

4. `WorkloadIdentityCredential` reads the token file and posts it as the `client_assertion` of
   a client credentials request, which Entra exchanges for an access token.

The projected token is only valid for the audience `api://AzureADTokenExchange`, and it is
rotated by Kubernetes. That is why `check` prints the claims: a wrong audience is the usual
reason a federation does not work.

## Layout

| Path | What it is |
| --- | --- |
| `src/main.cpp` | the two commands and the output |
| `src/workload_identity.*` | the environment and the credential |
| `src/jwt.*` | base64url and JWT decoding, no dependency |
| `src/cli.*` | argument parsing |
| `tests/` | unit tests, they need no network |
| `tools/mock_entra.py` | the local stand-in for the token endpoint |
| `scripts/demo-offline.sh` | the end to end demo |
| `cmake/` | how the Azure SDK and the other dependencies are fetched |

## Notes

- The Azure SDK for C++ is pinned to the `azure-identity_1.13.3` tag. The SDK uses per package
  tags, there is no monorepo tag any more.
- The SDK compiles itself as C++14, this project uses C++17.
- The access token is a bearer credential, the program never prints it.
