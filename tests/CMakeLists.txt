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

function(add_config_validation_test name transport http_debug hostname passcode)
    add_test(
        NAME ${name}
        COMMAND ${CMAKE_COMMAND}
            -DVOICEIQ_MATTER_TRANSPORT=${transport}
            -DVOICEIQ_ENABLE_HTTP_DEBUG=${http_debug}
            -DVOICEIQ_HOSTNAME=${hostname}
            -DVOICEIQ_MATTER_PASSCODE=${passcode}
            -P ${CMAKE_CURRENT_LIST_DIR}/../cmake/validate_project_config.cmake)
endfunction()

add_config_validation_test(project_config_valid wifi ON voiceiqmatter 20202021)
add_config_validation_test(project_config_valid_leading_zero thread OFF faucet-1 02020202)

add_config_validation_test(project_config_rejects_transport zigbee ON voiceiqmatter 20202021)
add_config_validation_test(project_config_rejects_debug wifi MAYBE voiceiqmatter 20202021)
add_config_validation_test(project_config_rejects_hostname wifi ON voiceiqmatter.local 20202021)
add_config_validation_test(project_config_rejects_passcode wifi ON voiceiqmatter 12345678)
add_config_validation_test(project_config_rejects_passcode_length wifi ON voiceiqmatter 202021)

set_tests_properties(
    project_config_rejects_transport
    project_config_rejects_debug
    project_config_rejects_hostname
    project_config_rejects_passcode
    project_config_rejects_passcode_length
    PROPERTIES WILL_FAIL TRUE)
