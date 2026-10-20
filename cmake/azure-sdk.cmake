# The Azure SDK for C++ is consumed from source.
#
# Things that are easy to get wrong with this dependency:
#
#  * It is a monorepo and the package directories were renamed: sdk/core/azure-core-cpp became
#    sdk/core/azure-core and sdk/identity/azure-identity-cpp became sdk/identity/azure-identity.
#    The old paths no longer exist.
#  * Tags are per package (azure-core_1.16.4, azure-identity_1.13.3, ...), there is no more
#    v1.x monorepo tag.
#  * Every package CMakeLists calls az_vcpkg_integrate(). Without AZURE_SDK_DISABLE_AUTO_VCPKG
#    in the environment that macro clones vcpkg and re-runs the configure step, which is slow
#    and needs a compiler it manages itself.
#  * azure-identity calls find_package(azure-core-cpp) unless AZ_ALL_LIBRARIES is on, in which
#    case it uses the in-tree Azure::azure-core target. azure-core therefore has to be added
#    first.
#  * The SDK needs OpenSSL (find_package(OpenSSL REQUIRED) on non-Windows).

include(FetchContent)

set(AZURE_SDK_CPP_REPOSITORY "https://github.com/Azure/azure-sdk-for-cpp.git" CACHE STRING
    "Azure SDK for C++ git repository")
set(AZURE_SDK_CPP_TAG "azure-identity_1.13.3" CACHE STRING
    "Azure SDK for C++ tag (per package tags, e.g. azure-identity_1.13.3)")

# Keep the SDK from cloning vcpkg behind our back.
set(ENV{AZURE_SDK_DISABLE_AUTO_VCPKG} "1")

# Let azure-identity use the azure-core target from this build instead of looking for an
# installed azure-core-cpp package.
set(AZ_ALL_LIBRARIES ON CACHE BOOL "" FORCE)

FetchContent_Declare(
    azure_core
    GIT_REPOSITORY "${AZURE_SDK_CPP_REPOSITORY}"
    GIT_TAG "${AZURE_SDK_CPP_TAG}"
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
    SOURCE_SUBDIR sdk/core/azure-core
)
FetchContent_MakeAvailable(azure_core)

# Reuse the checkout that was just cloned and add the identity package from it.
FetchContent_Declare(
    azure_identity
    SOURCE_DIR "${azure_core_SOURCE_DIR}"
    SOURCE_SUBDIR sdk/identity/azure-identity
)
FetchContent_MakeAvailable(azure_identity)

if(NOT TARGET Azure::azure-core OR NOT TARGET Azure::azure-identity)
    message(FATAL_ERROR "The Azure SDK for C++ did not provide the expected targets")
endif()
