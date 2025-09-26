cmake_minimum_required(VERSION 3.15)
project(spb C)

# === Library ===
add_library(spb_lib src/spb.c)
target_include_directories(spb_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Enable warnings depending on compiler
if (MSVC)
    target_compile_options(spb_lib PRIVATE /W4 /permissive- /WX)
else()
    message("testing.. 1 2 3")
    target_compile_options(spb_lib PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

set_property(TARGET spb_lib PROPERTY C_STANDARD 99)
set_property(TARGET spb_lib PROPERTY C_STANDARD_REQUIRED ON)
set_property(TARGET spb_lib PROPERTY C_EXTENSIONS OFF)

# Dependencies
find_package(eclipse-paho-mqtt-c REQUIRED)

# === Examples ===
add_executable(simple_publisher examples/simple_publisher.c)
target_link_libraries(simple_publisher spb_lib)

add_executable(simple_subscriber examples/simple_subscriber.c)
target_link_libraries(simple_subscriber spb_lib)

target_link_libraries(spb_lib eclipse-paho-mqtt-c::paho-mqtt3as-static)