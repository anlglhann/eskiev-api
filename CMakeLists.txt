cmake_minimum_required(VERSION 3.16)
project(eskiev_api LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(eskiev_api src/main.cpp)

# Homebrew include path (Asio için)
target_include_directories(eskiev_api PRIVATE /opt/homebrew/include)