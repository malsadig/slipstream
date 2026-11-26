cmake_minimum_required(VERSION 3.20)
project(Slipstream LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(-Wall -Wextra -Wpedantic)

add_library(slipstream_codec STATIC
    src/codec/encode.cpp
    src/codec/decode.cpp
    src/codec/stream_decoder.cpp
)

target_include_directories(slipstream_codec PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

add_executable(slipstream
    src/server/main.cpp
    src/server/config.cpp
)
target_link_libraries(slipstream PRIVATE slipstream_codec)

add_executable(codec_tests
    tests/codec/test_main.cpp
    tests/codec/heartbeat_tests.cpp
    tests/codec/quote_tests.cpp
    tests/codec/trade_tests.cpp
    tests/codec/session_control_tests.cpp
    tests/codec/validation_tests.cpp
    tests/codec/stream_tests.cpp
    tests/codec/new_order_tests.cpp
)
target_link_libraries(codec_tests PRIVATE slipstream_codec)

add_library(slipstream_replay STATIC
    src/replay/csv.cpp
    src/replay/replay.cpp
)

target_include_directories(slipstream_replay PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

add_executable(csv_tests
    tests/replay/csv_tests.cpp
)

target_link_libraries(csv_tests PRIVATE slipstream_replay)

enable_testing()
add_test(NAME codec_tests COMMAND codec_tests)
add_test(NAME csv_tests COMMAND csv_tests)

target_link_libraries(slipstream_replay PUBLIC slipstream_codec)

add_executable(md_client src/clients/md_client.cpp)
target_link_libraries(md_client PRIVATE slipstream_replay)

add_executable(oe_client src/clients/oe_client.cpp)
target_link_libraries(oe_client PRIVATE slipstream_replay)
