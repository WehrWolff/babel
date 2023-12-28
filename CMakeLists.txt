cmake_minimum_required(VERSION 3.12)
project(babel)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories(include)

set(SOURCE_FILES
    src/shell.cpp
)

add_executable(babel ${SOURCE_FILES})
