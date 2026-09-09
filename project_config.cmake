# VoiceIQMatter user settings
#
# This is the one file intended for normal customization. After changing the
# transport, run `idf.py fullclean` before rebuilding so ESP-IDF regenerates its
# low-level network configuration.

# Matter always remains enabled. Choose the network Matter runs over: "wifi"
# or "thread". HTTP debugging is automatically omitted from Thread builds.
if(NOT DEFINED VOICEIQ_MATTER_TRANSPORT)
    set(VOICEIQ_MATTER_TRANSPORT "wifi")
endif()

# The .local suffix is added automatically.
set(VOICEIQ_HOSTNAME "voiceiqmatter")

# Completely removes the HTTP log server when OFF. This setting only applies to
# Wi-Fi builds; Thread builds always remove it.
if(NOT DEFINED VOICEIQ_ENABLE_HTTP_DEBUG)
    set(VOICEIQ_ENABLE_HTTP_DEBUG ON)
endif()

# Matter commissioning values. The passcode must be eight digits and must not
# be one of Matter's disallowed trivial values.
set(VOICEIQ_MATTER_PASSCODE 20202021)
set(VOICEIQ_MATTER_DISCRIMINATOR 3840)
set(VOICEIQ_MATTER_SPAKE2P_ITERATIONS 1000)
set(VOICEIQ_MATTER_SPAKE2P_SALT "SPAKE2P Key Salt")

# Names displayed by Matter controllers.
set(VOICEIQ_MANUFACTURER "DIY")
set(VOICEIQ_PRODUCT_NAME "Delta Touch2O Faucet")
set(VOICEIQ_MODEL "VoiceIQ replacement")

# XIAO ESP32-C6 / Touch2O wiring and serial protocol.
set(VOICEIQ_UART_PORT 1)
set(VOICEIQ_UART_RX_GPIO 16)       # XIAO D6 -> RJ45 pin 3
set(VOICEIQ_UART_TX_GPIO 17)       # XIAO D7 -> RJ45 pin 6
set(VOICEIQ_HANDSHAKE_GPIO 21)     # XIAO D3 -> RJ45 pin 7
set(VOICEIQ_UART_BAUD 9600)
set(VOICEIQ_HEARTBEAT_PERIOD_US 5000000)
set(VOICEIQ_HANDSHAKE_LOW_US 500)
set(VOICEIQ_POST_HANDSHAKE_DELAY_US 200)

# RAM retained for the browser log when HTTP debugging is enabled.
set(VOICEIQ_HTTP_LOG_BUFFER_SIZE 12288)
