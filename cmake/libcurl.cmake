# libcurl is a hard requirement of the Azure SDK for C++ on Linux and macOS.
#
# AzureTransportAdapters.cmake turns BUILD_TRANSPORT_CURL on unconditionally on UNIX, so the SDK
# always runs find_package(CURL) and the configure step fails when the development headers are
# missing. The runtime library alone is not enough, curl/curl.h has to be installed as well.
#
# Fedora/RHEL: dnf install libcurl-devel
# Debian/Ubuntu: apt install libcurl4-openssl-dev

find_package(CURL QUIET)

if(NOT CURL_FOUND)
    message(FATAL_ERROR
        "libcurl development headers were not found.\n"
        "The Azure SDK for C++ needs libcurl on Linux and macOS. Install the development "
        "package (Fedora/RHEL: 'dnf install libcurl-devel', Debian/Ubuntu: "
        "'apt install libcurl4-openssl-dev') and configure again."
    )
endif()
