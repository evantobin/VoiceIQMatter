cmake_minimum_required(VERSION 3.16)
project(voiceiqmatter_tests LANGUAGES CXX)

enable_testing()

add_executable(touch2o_protocol_tests
    test_touch2o_protocol.cpp
    ../main/faucet/touch2o_protocol.cpp
)
target_compile_features(touch2o_protocol_tests PRIVATE cxx_std_17)
target_include_directories(touch2o_protocol_tests PRIVATE ../main)

add_test(NAME touch2o_protocol COMMAND touch2o_protocol_tests)
