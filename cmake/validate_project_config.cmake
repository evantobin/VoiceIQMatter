foreach(setting
        VOICEIQ_MATTER_TRANSPORT
        VOICEIQ_ENABLE_HTTP_DEBUG
        VOICEIQ_HOSTNAME
        VOICEIQ_MATTER_PASSCODE)
    if(NOT DEFINED ${setting})
        message(FATAL_ERROR "${setting} is missing from project_config.cmake")
    endif()
endforeach()

string(TOLOWER "${VOICEIQ_MATTER_TRANSPORT}" VOICEIQ_MATTER_TRANSPORT)
if(NOT VOICEIQ_MATTER_TRANSPORT STREQUAL "wifi" AND
   NOT VOICEIQ_MATTER_TRANSPORT STREQUAL "thread")
    message(FATAL_ERROR "VOICEIQ_MATTER_TRANSPORT must be \"wifi\" or \"thread\"")
endif()

string(TOUPPER "${VOICEIQ_ENABLE_HTTP_DEBUG}" VOICEIQ_ENABLE_HTTP_DEBUG)
if(NOT VOICEIQ_ENABLE_HTTP_DEBUG STREQUAL "ON" AND
   NOT VOICEIQ_ENABLE_HTTP_DEBUG STREQUAL "OFF")
    message(FATAL_ERROR "VOICEIQ_ENABLE_HTTP_DEBUG must be ON or OFF")
endif()

string(LENGTH "${VOICEIQ_HOSTNAME}" voiceiq_hostname_length)
if(voiceiq_hostname_length LESS 1 OR voiceiq_hostname_length GREATER 63 OR
   NOT "${VOICEIQ_HOSTNAME}" MATCHES "^[A-Za-z0-9]([A-Za-z0-9-]*[A-Za-z0-9])?$")
    message(FATAL_ERROR
        "VOICEIQ_HOSTNAME must be 1-63 letters, numbers, or hyphens; "
        "it cannot start or end with a hyphen and should not include .local")
endif()

if(NOT "${VOICEIQ_MATTER_PASSCODE}" MATCHES
   "^[0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9]$")
    message(FATAL_ERROR "VOICEIQ_MATTER_PASSCODE must contain exactly eight digits")
endif()

set(voiceiq_disallowed_passcodes
    00000000
    11111111
    22222222
    33333333
    44444444
    55555555
    66666666
    77777777
    88888888
    99999999
    12345678
    87654321)
list(FIND voiceiq_disallowed_passcodes "${VOICEIQ_MATTER_PASSCODE}" voiceiq_passcode_index)
if(NOT voiceiq_passcode_index EQUAL -1)
    message(FATAL_ERROR "VOICEIQ_MATTER_PASSCODE is not allowed by Matter")
endif()

# Remove leading zeroes before inserting the numeric passcode into C++ so it
# cannot be interpreted as an octal literal.
string(REGEX REPLACE "^0+" "" VOICEIQ_MATTER_PASSCODE_CPP "${VOICEIQ_MATTER_PASSCODE}")
if(VOICEIQ_MATTER_PASSCODE_CPP STREQUAL "")
    set(VOICEIQ_MATTER_PASSCODE_CPP 0)
endif()
