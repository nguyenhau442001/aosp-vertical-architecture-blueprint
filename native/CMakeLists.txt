# Host build for fast iteration on macOS/Linux, and an NDK cross build for
# pushing to a device/emulator (scripts/build_android.sh). In an AOSP tree the
# same sources build through the Android.bp files next to them.
cmake_minimum_required(VERSION 3.20)
project(blueprint_native CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_compile_options(-Wall -Wextra -Werror)

include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(googletest)
enable_testing()
include(GoogleTest)

# Cross-compiled test binaries cannot run on the build host, so test discovery
# only happens for host builds. On Android run them with `make android-test`.
function(blueprint_add_test target)
    if(NOT ANDROID)
        gtest_discover_tests(${target})
    endif()
endfunction()

# --- cansignal -------------------------------------------------------------
add_library(cansignal STATIC cansignal/src/Signal.cpp)
target_include_directories(cansignal PUBLIC cansignal/include)

add_executable(cansignal_test cansignal/test/SignalTest.cpp)
target_link_libraries(cansignal_test cansignal GTest::gtest_main)
blueprint_add_test(cansignal_test)

# --- cantransport ----------------------------------------------------------
add_library(cantransport STATIC
    cantransport/src/CanFrame.cpp
    cantransport/src/SocketCanTransport.cpp
    cantransport/src/TcpCanTransport.cpp
    cantransport/src/Transport.cpp
)
target_include_directories(cantransport PUBLIC cantransport/include)

add_executable(bp-candump cantransport/tools/candump_main.cpp)
target_link_libraries(bp-candump cantransport)

add_executable(cantransport_test cantransport/test/TransportTest.cpp)
target_link_libraries(cantransport_test cantransport GTest::gtest_main)
blueprint_add_test(cantransport_test)

# --- canbridge -------------------------------------------------------------
add_library(canbridge STATIC
    canbridge/src/CanBridge.cpp
    canbridge/src/SignalRouter.cpp
)
target_include_directories(canbridge PUBLIC canbridge/include canbridge)
target_link_libraries(canbridge PUBLIC cansignal cantransport)
find_package(Threads REQUIRED)
target_link_libraries(canbridge PUBLIC Threads::Threads)

add_executable(bp-canbridge canbridge/tools/canbridge_main.cpp)
target_link_libraries(bp-canbridge canbridge)

add_executable(canbridge_test canbridge/test/CanBridgeTest.cpp)
target_link_libraries(canbridge_test canbridge GTest::gtest_main)
blueprint_add_test(canbridge_test)
