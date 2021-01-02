cmake_minimum_required(VERSION 3.16)

project(ex4 C)

add_compile_options(-D_POSIX_C_SOURCE=200809 -Wall -std=c11)

add_executable(pfind pfind.c)

target_link_libraries(pfind pthread)
