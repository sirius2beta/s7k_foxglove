cmake_minimum_required(VERSION 3.20)

# TODO: replace "foxglove_test" throughout with the name of your project
project(s7k_foxglove LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

# Fetch Foxglove SDK
include(FetchContent)
FetchContent_Declare(
    foxglove
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    # TODO: Copy the URL and SHA for the appropriate .zip file from here:
    # https://github.com/foxglove/foxglove-sdk/releases?q=sdk%2F&expanded=true
    URL https://github.com/foxglove/foxglove-sdk/releases/download/sdk%2Fv0.22.0/foxglove-v0.22.0-cpp-x86_64-pc-windows-msvc.zip
    URL_HASH SHA256=c12d2666b03cfd1835d84d0ee2b0bb5cd03e5e6d8e84da4a0b0588840c85cea0
)
FetchContent_MakeAvailable(foxglove)

# Assuming the project consists of a single source file, main.cpp
add_executable(s7k_foxglove
    main.cpp
    Example7k.cpp
    Example7k.h
    NetworkSocket.cpp
    NetworkSocket.h
    stdafx.cpp
    stdafx.h
    TcpSocket7k.cpp
    TcpSocket7k.h
    7k.h
)

add_library(FoxgloveBridge
    FoxgloveWrapper.h
    FoxgloveWrapper.cpp
)


# Find all Foxglove SDK source files
file(GLOB FOXGLOVE_SOURCES CONFIGURE_DEPENDS
    "${foxglove_SOURCE_DIR}/src/*.cpp"
    "${foxglove_SOURCE_DIR}/src/server/*.cpp"
)


# Link against libfoxglove.a
target_link_libraries(s7k_foxglove PRIVATE
    PRIVATE ws2_32
    FoxgloveBridge
)
target_include_directories(FoxgloveBridge PRIVATE ${foxglove_SOURCE_DIR}/include)
target_include_directories(FoxgloveBridge PRIVATE ${foxglove_SOURCE_DIR}/include/foxglove)
target_sources(FoxgloveBridge PRIVATE ${FOXGLOVE_SOURCES})
target_link_libraries(FoxgloveBridge PRIVATE ${foxglove_SOURCE_DIR}/lib/foxglove.dll.lib
)
