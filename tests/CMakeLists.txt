include(FetchContent)

set(GOOGLETEST_VERSION "v1.17.0" CACHE STRING "GoogleTest tag to use")

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG "${GOOGLETEST_VERSION}"
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(googletest)

add_executable(poc-azure-cmake-tests
    test_jwt.cpp
    test_workload_identity.cpp
)
target_link_libraries(poc-azure-cmake-tests PRIVATE poc-azure-cmake-lib GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(poc-azure-cmake-tests)

# The offline demo runs the whole workload identity flow against a local stand-in for the
# Microsoft Entra token endpoint. It needs bash, python3 and openssl.
find_program(POC_BASH bash)
find_program(POC_PYTHON python3)
find_program(POC_OPENSSL openssl)

if(POC_BASH AND POC_PYTHON AND POC_OPENSSL)
    add_test(
        NAME OfflineWorkloadIdentityFlow
        COMMAND "${POC_BASH}" "${CMAKE_SOURCE_DIR}/scripts/demo-offline.sh"
    )
    set_tests_properties(OfflineWorkloadIdentityFlow PROPERTIES
        LABELS "e2e"
        ENVIRONMENT "BUILD_DIR=${CMAKE_BINARY_DIR}"
        TIMEOUT 180
    )
else()
    message(STATUS
        "bash, python3 or openssl is missing, the offline demo is not registered as a test")
endif()
