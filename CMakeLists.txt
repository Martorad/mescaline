cmake_minimum_required(VERSION 3.10)
project(mescaline C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -O3")

add_executable(mescaline src/shader.c)

target_link_libraries(mescaline m)