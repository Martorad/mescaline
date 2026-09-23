cmake_minimum_required(VERSION 3.21)
project(mescaline C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(mescaline src/shader.c)
target_compile_options(mescaline PRIVATE -Wall -O3)
target_link_libraries(mescaline PRIVATE m)

option(MESCALINE_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
if(MESCALINE_ENABLE_SANITIZERS)
  if(NOT CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    message(FATAL_ERROR "Sanitizers require GCC or Clang")
  endif()

  target_compile_options(mescaline PRIVATE -O1 -fno-omit-frame-pointer -fno-sanitize-recover=all
                                            -fsanitize=address,undefined)
  target_link_options(mescaline PRIVATE -fsanitize=address,undefined)
endif()

include(CTest)
if(BUILD_TESTING)
  find_package(Python3 3.8 REQUIRED COMPONENTS Interpreter)
  add_test(NAME cli COMMAND "${Python3_EXECUTABLE}"
                            "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_cli.py"
                            "$<TARGET_FILE:mescaline>")
endif()
