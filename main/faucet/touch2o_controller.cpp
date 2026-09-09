#include "faucet/touch2o_controller.h"

#include <driver/gpio.h>
#include <driver/uart.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>

#include <cstdio>

#include "faucet/touch2o_protocol.h"

namespace faucet {
namespace {

constexpr const char *kLogTag = "touch2o";
constexpr uart_port_t kUart = UART_NUM_1;
// These are the verified protocol GPIO numbers from Vitaliy Kholyavenko's
// Touch2O sketch, mapped to exposed XIAO ESP32-C6 pins. UART1 is routed through
// the GPIO matrix so USB serial remains available for flashing and logs.
constexpr gpio_num_t kRxPin = GPIO_NUM_16;
constexpr gpio_num_t kTxPin = GPIO_NUM_17;
constexpr gpio_num_t kHandshakePin = GPIO_NUM_21;
constexpr uint64_t kHeartbeatPeriodUs = 5'000'000;
constexpr uint64_t kHandshakeLowUs = 500;
constexpr uint64_t kPostHandshakeDelayUs = 200;

enum class PendingFrame : uint8_t { None, Heartbeat, Open, Close };

Touch2OController *sOwner = nullptr;
esp_timer_handle_t sHeartbeatTimer = nullptr;
esp_timer_handle_t sHandshakeTimer = nullptr;
portMUX_TYPE sProtocolMutex = portMUX_INITIALIZER_UNLOCKED;
PendingFrame sPendingFrame = PendingFrame::None;
bool sPulseActive = false;
bool sHandshakeLow = false;
uint8_t sRx[10]{};
size_t sRxLength = 0;
size_t sExpectedLength = 0;

void sendFrame(PendingFrame frame) {
  protocol::Command command = protocol::Command::Heartbeat;
  const char *description = "heartbeat";
  if (frame == PendingFrame::Open) {
    command = protocol::Command::Open;
    description = "open";
  } else if (frame == PendingFrame::Close) {
    command = protocol::Command::Close;
    description = "close";
  }
  const auto &bytes = protocol::commandFrame(command);
  const int written = uart_write_bytes(kUart, bytes.data(), bytes.size());
  if (written != static_cast<int>(bytes.size())) {
    ESP_LOGW(kLogTag, "Touch2O %s frame write failed (%d/%u)", description, written,
             static_cast<unsigned>(bytes.size()));
  } else {
    ESP_LOGI(kLogTag, "Touch2O %s frame sent", description);
  }
}

void startPulseIfIdle() {
  bool start = false;
  portENTER_CRITICAL(&sProtocolMutex);
  if (!sPulseActive) {
    sPulseActive = true;
    sHandshakeLow = true;
    start = true;
  }
  portEXIT_CRITICAL(&sProtocolMutex);
  if (!start) return;

  // Pin 7 is a shared 3.3 V handshake. Open-drain makes HIGH a release, so
  // the Touch2O module can also use the line without contention.
  gpio_set_level(kHandshakePin, 0);
  if (esp_timer_start_once(sHandshakeTimer, kHandshakeLowUs) != ESP_OK) {
    ESP_LOGW(kLogTag, "Unable to schedule Touch2O handshake");
    gpio_set_level(kHandshakePin, 1);
    portENTER_CRITICAL(&sProtocolMutex);
    sPulseActive = false;
    sHandshakeLow = false;
    portEXIT_CRITICAL(&sProtocolMutex);
  }
}

void handshakeTimerCallback(void *) {
  bool sendAfterDelay = false;
  PendingFrame frame = PendingFrame::None;
  portENTER_CRITICAL(&sProtocolMutex);
  if (sHandshakeLow) {
    sHandshakeLow = false;
    sendAfterDelay = true;
  } else {
    frame = sPendingFrame;
    sPendingFrame = PendingFrame::None;
    sPulseActive = false;
  }
  portEXIT_CRITICAL(&sProtocolMutex);

  if (sendAfterDelay) {
    gpio_set_level(kHandshakePin, 1);
    if (esp_timer_start_once(sHandshakeTimer, kPostHandshakeDelayUs) != ESP_OK) {
      ESP_LOGW(kLogTag, "Unable to complete Touch2O handshake");
    }
    return;
  }

  // A command takes precedence over the regular heartbeat when both are due.
  if (frame == PendingFrame::None) frame = PendingFrame::Heartbeat;
  sendFrame(frame);
}

void heartbeatTimerCallback(void *) {
  portENTER_CRITICAL(&sProtocolMutex);
  if (sPendingFrame == PendingFrame::None) sPendingFrame = PendingFrame::Heartbeat;
  portEXIT_CRITICAL(&sProtocolMutex);
  startPulseIfIdle();
}

void resetParser() {
  sRxLength = 0;
  sExpectedLength = 0;
}

void logStatusFrame(const uint8_t *frame, size_t length, const char *description) {
  // Three characters per byte (two hex digits plus a separator), with one byte
  // saved for the terminating null character.
  char hex[(protocol::kStatusFrameSize * 3)]{};
  size_t offset = 0;
  for (size_t i = 0; i < length && offset < sizeof(hex); ++i) {
    const int written = snprintf(hex + offset, sizeof(hex) - offset, i == 0 ? "%02X" : " %02X", frame[i]);
    if (written <= 0 || static_cast<size_t>(written) >= sizeof(hex) - offset) break;
    offset += static_cast<size_t>(written);
  }
  ESP_LOGI(kLogTag, "RX status [%s]: %s", description, hex);
}

void consumeByte(uint8_t byte) {
  if (sRxLength == 0) {
    if (byte == protocol::kFrameHeader) sRx[sRxLength++] = byte;
    return;
  }

  if (sRxLength >= sizeof(sRx)) {
    resetParser();
    return;
  }
  sRx[sRxLength++] = byte;
  if (sRxLength == 2) {
    if (sRx[1] == protocol::kStatusFrameId) {
      sExpectedLength = protocol::kStatusFrameSize;
    } else if (sRx[1] == protocol::kCommandFrameId) {
      sExpectedLength = protocol::kCommandFrameSize;
    } else {
      resetParser();
    }
    return;
  }
  if (sExpectedLength == 0 || sRxLength != sExpectedLength) return;

  if (sExpectedLength == protocol::kStatusFrameSize) {
    const auto state = protocol::parseStatusFrame(sRx, sRxLength);
    if (state == protocol::ValveState::Open) {
      logStatusFrame(sRx, sRxLength, "valve open");
      if (sOwner) sOwner->publishState(true);
    } else if (state == protocol::ValveState::Closed) {
      logStatusFrame(sRx, sRxLength, "valve closed");
      if (sOwner) sOwner->publishState(false);
    } else {
      logStatusFrame(sRx, sRxLength, "unknown");
    }
  }
  resetParser();
}

}  // namespace

void Touch2OController::begin() {
  uart_config_t config = {};
  config.baud_rate = 9600;
  config.data_bits = UART_DATA_8_BITS;
  config.parity = UART_PARITY_DISABLE;
  config.stop_bits = UART_STOP_BITS_1;
  config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
  config.source_clk = UART_SCLK_DEFAULT;
  ESP_ERROR_CHECK(uart_param_config(kUart, &config));
  ESP_ERROR_CHECK(uart_set_pin(kUart, kTxPin, kRxPin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
  ESP_ERROR_CHECK(uart_driver_install(kUart, 2048, 2048, 0, nullptr, 0));

  gpio_config_t handshakeConfig = {};
  handshakeConfig.pin_bit_mask = 1ULL << kHandshakePin;
  handshakeConfig.mode = GPIO_MODE_OUTPUT_OD;
  handshakeConfig.pull_up_en = GPIO_PULLUP_ENABLE;
  handshakeConfig.pull_down_en = GPIO_PULLDOWN_DISABLE;
  handshakeConfig.intr_type = GPIO_INTR_DISABLE;
  ESP_ERROR_CHECK(gpio_config(&handshakeConfig));
  gpio_set_level(kHandshakePin, 1);

  const esp_timer_create_args_t handshakeArgs = {
      .callback = handshakeTimerCallback, .arg = nullptr, .dispatch_method = ESP_TIMER_TASK,
      .name = "touch2o_hs", .skip_unhandled_events = false};
  ESP_ERROR_CHECK(esp_timer_create(&handshakeArgs, &sHandshakeTimer));
  const esp_timer_create_args_t heartbeatArgs = {
      .callback = heartbeatTimerCallback, .arg = nullptr, .dispatch_method = ESP_TIMER_TASK,
      .name = "touch2o_hb", .skip_unhandled_events = true};
  ESP_ERROR_CHECK(esp_timer_create(&heartbeatArgs, &sHeartbeatTimer));

  sOwner = this;
  ESP_ERROR_CHECK(esp_timer_start_periodic(sHeartbeatTimer, kHeartbeatPeriodUs));
  // Bring the solenoid protocol up immediately; do not wait five seconds for
  // the first heartbeat after power-up.
  heartbeatTimerCallback(nullptr);
  ESP_LOGI(kLogTag, "Touch2O UART ready: 9600 8N1, RX=%d TX=%d handshake=%d", kRxPin, kTxPin,
           kHandshakePin);
}

void Touch2OController::poll() {
  uint8_t bytes[64];
  const int received = uart_read_bytes(kUart, bytes, sizeof(bytes), 0);
  for (int i = 0; i < received; ++i) consumeByte(bytes[i]);
}

void Touch2OController::requestValve(bool open) {
  portENTER_CRITICAL(&sProtocolMutex);
  sPendingFrame = open ? PendingFrame::Open : PendingFrame::Close;
  portEXIT_CRITICAL(&sProtocolMutex);
  ESP_LOGI(kLogTag, "Queued Touch2O valve %s", open ? "open" : "close");
  startPulseIfIdle();
}

void Touch2OController::setStateCallback(StateCallback callback) { stateCallback_ = std::move(callback); }

void Touch2OController::publishState(bool open) {
  if (!stateCallback_) return;
  State state;
  state.valveOpen = open;
  stateCallback_(state);
}

}  // namespace faucet
