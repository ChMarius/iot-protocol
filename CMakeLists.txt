cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(iot_protocol)

target_sources(app PRIVATE
    src/main.c
    src/protocol/communication.c
    src/protocol/IP_connect.c
    src/protocol/message_format.c
    src/protocol/socket.c
    src/protocol/ip_header.c
)