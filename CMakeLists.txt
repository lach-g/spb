cmake_minimum_required(VERSION 3.15)
project(spb C)

add_library(spb_lib src/spb.c)

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

find_package(eclipse-paho-mqtt-c REQUIRED)

add_executable(simple_publisher src/simple_publisher.c
        src/spb.h
        src/spb.c)

add_executable(simple_subscriber src/simple_subscriber.c
        src/spb.h
        src/spb.c)

# target_link_libraries(${PROJECT_NAME} eclipse-paho-mqtt-c::paho-mqtt3as-static)
target_link_libraries(simple_publisher eclipse-paho-mqtt-c::paho-mqtt3as-static)
target_link_libraries(simple_subscriber eclipse-paho-mqtt-c::paho-mqtt3as-static)
