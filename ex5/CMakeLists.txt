cmake_minimum_required(VERSION 3.16)

project(ex5 C)

add_compile_options(-D_POSIX_C_SOURCE=200809 -Wall -std=c11)

add_executable(pcc_client pcc_client.c)
add_executable(pcc_server pcc_server.c)
add_executable(random_generator random_generator.c)