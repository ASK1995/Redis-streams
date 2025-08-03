cmake_minimum_required(VERSION 3.10)
project(RedisStreamsService)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(Threads REQUIRED)

# Include directories
include_directories(include)

# Source files
set(SOURCES
    src/main.cpp
    src/redis_server.cpp
    src/redis_protocol.cpp
    src/stream.cpp
    src/stream_entry.cpp
    src/consumer_group.cpp
    src/consumer.cpp
)

# Create executable
add_executable(redis_streams_service ${SOURCES})

# Link libraries
target_link_libraries(redis_streams_service Threads::Threads)

# Compiler flags
target_compile_options(redis_streams_service PRIVATE -Wall -Wextra -std=c++17)
