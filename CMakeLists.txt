cmake_minimum_required(VERSION 3.12)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
project(babel)

add_executable(babel src/shell.cpp src/lexer.h)
