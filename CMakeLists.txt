cmake_minimum_required(VERSION 3.26)
project(custom_file_system C)

set(CMAKE_C_STANDARD 11)

add_executable(custom_file_system
        test.c
        userfs.c)
