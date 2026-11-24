cmake_minimum_required(VERSION 3.20)
project(Slipstream LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(-Wall -Wextra -Wpedantic)

add_library(slipstream_codec STATIC src/codec.cpp)
target_include_directories(slipstream_codec PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

add_executable(codec_tests tests/codec_tests.cpp)
target_link_libraries(codec_tests PRIVATE slipstream_codec)

enable_testing()
add_test(NAME codec_tests COMMAND codec_tests)
