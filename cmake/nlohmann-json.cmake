# nlohmann/json is used to read the claims out of the JWTs that Azure hands out.
# It is header only, so nothing is compiled into this project.

include(FetchContent)

set(NLOHMANN_JSON_VERSION "v3.11.3" CACHE STRING "nlohmann/json tag to use")

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG "${NLOHMANN_JSON_VERSION}"
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(nlohmann_json)
