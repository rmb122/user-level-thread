cmake_minimum_required(VERSION 3.14)
project(ult C)

set(CMAKE_C_STANDARD 11)

add_library(libul_thread ul_thread.c)
add_executable(ult main.c)