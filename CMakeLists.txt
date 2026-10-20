cmake_minimum_required(VERSION 3.20)
project(poc-azure-cmake VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(POC_BUILD_TESTS "Build the unit tests" ON)

# The Azure SDK builds its own (large) test suites whenever BUILD_TESTING is on. Our tests are
# registered separately below, so keep the SDK's out of the build.
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)

include(cmake/libcurl.cmake)
include(cmake/azure-sdk.cmake)
include(cmake/nlohmann-json.cmake)

add_library(poc-azure-cmake-lib STATIC
    src/cli.cpp
    src/jwt.cpp
    src/workload_identity.cpp
)
target_include_directories(poc-azure-cmake-lib PUBLIC src)
target_link_libraries(poc-azure-cmake-lib
    PUBLIC
        Azure::azure-identity
        nlohmann_json::nlohmann_json
)

add_executable(poc-azure-cmake src/main.cpp)
target_link_libraries(poc-azure-cmake PRIVATE poc-azure-cmake-lib)

if(POC_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

install(TARGETS poc-azure-cmake RUNTIME DESTINATION bin)
