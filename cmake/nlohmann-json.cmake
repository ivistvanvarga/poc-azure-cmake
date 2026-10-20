# nlohmann/json is used to read the claims out of the JWTs that Azure hands out.
# It is header only, so nothing is compiled into this project.

include(FetchContent)

set(NLOHMANN_JSON_VERSION "v3.11.3" CACHE STRING "nlohmann/json tag to use")

# nlohmann/json ships its own unit test suite and the option that turns it on is called
# JSON_BuildTests, not BUILD_TESTING. It does not look at BUILD_TESTING at all, so the switch
# that keeps the Azure SDK from building its tests does not stop it. The suite is large (a few
# hundred binaries) and it fails to compile outside of the upstream project layout, because it
# includes <test_data.hpp> from a directory that is not on the include path and that is only
# filled by the download_test_data target. We only want the header, so the tests stay off.
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG "${NLOHMANN_JSON_VERSION}"
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

# A build tree that was configured with the tests on keeps the test registrations of that run.
# CMake does not remove them when the subdirectory is dropped, and ctest would keep trying to
# run the binaries that are not built any more.
file(REMOVE_RECURSE "${nlohmann_json_BINARY_DIR}/tests")
file(REMOVE "${nlohmann_json_BINARY_DIR}/CTestTestfile.cmake")
