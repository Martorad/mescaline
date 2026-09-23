cmake_minimum_required(VERSION 3.21)
project(mescaline C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

add_executable(mescaline
  src/algorithms.c
  src/cli.c
  src/encode.c
  src/output.c
  src/progress.c
  src/render.c
  src/shader.c
)
target_compile_definitions(mescaline PRIVATE _POSIX_C_SOURCE=200809L)
target_compile_options(mescaline PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(mescaline PRIVATE m)

option(MESCALINE_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
include(CTest)
if(BUILD_TESTING)
  find_package(Python3 3.8 REQUIRED COMPONENTS Interpreter)
  add_test(NAME cli COMMAND "${Python3_EXECUTABLE}"
                            "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_cli.py"
                            "$<TARGET_FILE:mescaline>")

  add_executable(mescaline_render_test tests/test_render.c src/algorithms.c src/render.c)
  target_include_directories(mescaline_render_test PRIVATE src)
  target_compile_definitions(mescaline_render_test PRIVATE _POSIX_C_SOURCE=200809L)
  target_compile_options(mescaline_render_test PRIVATE -Wall -Wextra -Wpedantic)
  target_link_libraries(mescaline_render_test PRIVATE m)
  add_test(NAME render COMMAND mescaline_render_test)
endif()

if(MESCALINE_ENABLE_SANITIZERS)
  if(NOT CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    message(FATAL_ERROR "Sanitizers require GCC or Clang")
  endif()

  set(sanitized_targets mescaline)
  if(BUILD_TESTING)
    list(APPEND sanitized_targets mescaline_render_test)
  endif()
  foreach(target IN LISTS sanitized_targets)
    target_compile_options(${target} PRIVATE -O1 -fno-omit-frame-pointer
                                             -fno-sanitize-recover=all
                                             -fsanitize=address,undefined)
    target_link_options(${target} PRIVATE -fsanitize=address,undefined)
  endforeach()
endif()
