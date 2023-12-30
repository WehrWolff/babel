# Release -- cmake .. -DCMAKE_TOOLCHAIN_FILE=./conan/build/Release/generators/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=release
# Debug -- cmake .. -DCMAKE_TOOLCHAIN_FILE=./conan/build/Debug/generators/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=release

cmake_minimum_required(VERSION 3.12)
project(babel)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SOURCE_FILES
    src/shell.cpp
)

#include_directories(include)

find_package(Boost 1.83.0 REQUIRED COMPONENTS algorithm)

add_executable(babel ${SOURCE_FILES})
target_link_libraries(babel PRIVATE ${Boost_LIBRARIES})
