cmake_minimum_required(VERSION 3.16)

project(ex1 C)

add_compile_options(-Wall -std=c11)

add_executable(pt.o pt.c os.c)

target_link_libraries(pt.o m)