#include "matter/matter_water_valve.h"

#include <cstring>

#include <app-common/zap-generated/cluster-enums.h>
#include <app/clusters/valve-configuration-and-control-server/valve-configuration-and-control-delegate.h>
#include <esp_log.h>
#include <esp_matter.h>
#include <esp_matter_attribute_utils.h>
#include <esp_matter_endpoint.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#include <platform/CHIPDeviceLayer.h>
#include <setup_payload/OnboardingCodesUtil.h>

#include "faucet/touch2o_controller.h"

namespace matter_water_valve {
namespace {

constexpr const char *kManufacturer = "DIY";
constexpr const char *kProductName = "Delta Touch2O Faucet";
constexpr const char *kModel = "VoiceIQ replacement";

using namespace chip::app::Clusters;

faucet::Touch2OController *sFaucet = nullptr;
uint16_t sEndpointId = 0;
bool sValveOpen = false;
bool sReportPending = false;
portMUX_TYPE sStateMutex = portMUX_INITIALIZER_UNLOCKED;

class FaucetValveDelegate final : public ValveConfigurationAndControl::Delegate {
 public:
  chip::app::DataModel::Nullable<chip::Percent> HandleOpenValve(
      chip::app::DataModel::Nullable<chip::Percent>) override {
    if (sFaucet) sFaucet->requestValve(true);
    // This is a binary valve. Report its physical state only after the Touch2O
    // status packet confirms it, rather than claiming a level immediately.
    return chip::app::DataModel::Nullable<chip::Percent>();
  }

  CHIP_ERROR HandleCloseValve() override {
    if (sFaucet) sFaucet->requestValve(false);
    return CHIP_NO_ERROR;
  }

  void HandleRemainingDurationTick(uint32_t) override {}
};

FaucetValveDelegate sValveDelegate;

void reportValveStateOnMatterThread(intptr_t) {
  bool open;
  portENTER_CRITICAL(&sStateMutex);
  open = sValveOpen;
  sReportPending = false;
  portEXIT_CRITICAL(&sStateMutex);
  if (sEndpointId == 0) return;

  // Update the endpoint attribute directly. The ESP-Matter generated water-valve
  // device type provides this attribute, while the CHIP CodegenIntegration helper
  // is not linked into generated-data-model builds.
  const auto state = open ? ValveConfigurationAndControl::ValveStateEnum::kOpen
                          : ValveConfigurationAndControl::ValveStateEnum::kClosed;
  esp_matter_attr_val_t value = esp_matter_nullable_enum8(
      nullable<uint8_t>(static_cast<uint8_t>(state)));
  const esp_err_t error = esp_matter::attribute::update(
      sEndpointId, ValveConfigurationAndControl::Id,
      ValveConfigurationAndControl::Attributes::CurrentState::Id, &value);
  if (error != ESP_OK) {
    ESP_LOGW("matter", "Unable to publish physical valve state: %s", esp_err_to_name(error));
  }
}

void configureMetadata() {
  esp_matter_attr_val_t value = esp_matter_char_str(const_cast<char *>(kManufacturer), strlen(kManufacturer));
  esp_matter::attribute::update(0, BasicInformation::Id, BasicInformation::Attributes::VendorName::Id, &value);
  value = esp_matter_char_str(const_cast<char *>(kProductName), strlen(kProductName));
  esp_matter::attribute::update(0, BasicInformation::Id, BasicInformation::Attributes::ProductName::Id, &value);
  value = esp_matter_char_str(const_cast<char *>(kModel), strlen(kModel));
  esp_matter::attribute::update(0, BasicInformation::Id, BasicInformation::Attributes::ProductLabel::Id, &value);
  value = esp_matter_char_str(const_cast<char *>(kProductName), strlen(kProductName));
  esp_matter::attribute::update(0, BasicInformation::Id, BasicInformation::Attributes::NodeLabel::Id, &value);
}

chip::RendezvousInformationFlags rendezvousFlags() {
  chip::RendezvousInformationFlags flags;
  flags.Set(chip::RendezvousInformationFlag::kBLE);
  flags.Set(chip::RendezvousInformationFlag::kOnNetwork);
  return flags;
}

}  // namespace

bool begin(faucet::Touch2OController &faucet) {
  sFaucet = &faucet;
  esp_matter::node::config_t nodeConfig;
  esp_matter::node_t *node = esp_matter::node::create(&nodeConfig, nullptr, nullptr, nullptr);
  if (node == nullptr) {
    ESP_LOGE("matter", "Failed to create Matter node");
    return false;
  }

  esp_matter::endpoint::water_valve::config_t config;
  config.valve_configuration_and_control.delegate = &sValveDelegate;
  // The faucet does not support timed dispensing via the serial protocol.
  config.valve_configuration_and_control.open_duration = nullable<uint32_t>();
  config.valve_configuration_and_control.default_open_duration = nullable<uint32_t>();
  config.valve_configuration_and_control.remaining_duration = nullable<uint32_t>();
  config.valve_configuration_and_control.current_state =
      nullable<uint8_t>(static_cast<uint8_t>(ValveConfigurationAndControl::ValveStateEnum::kClosed));
  config.valve_configuration_and_control.target_state = nullable<uint8_t>();

  esp_matter::endpoint_t *endpoint = esp_matter::endpoint::water_valve::create(
      node, &config, esp_matter::ENDPOINT_FLAG_NONE, nullptr);
  if (endpoint == nullptr) {
    ESP_LOGE("matter", "Failed to create water-valve endpoint");
    return false;
  }
  sEndpointId = esp_matter::endpoint::get_id(endpoint);
  configureMetadata();
  return true;
}

bool start() {
  const esp_err_t error = esp_matter::start(nullptr);
  if (error != ESP_OK) {
    ESP_LOGE("matter", "Failed to start Matter: %s", esp_err_to_name(error));
    return false;
  }
  return true;
}

void publishValveState(const faucet::State &state) {
  bool shouldSchedule = false;
  portENTER_CRITICAL(&sStateMutex);
  sValveOpen = state.valveOpen;
  if (!sReportPending) {
    sReportPending = true;
    shouldSchedule = true;
  }
  portEXIT_CRITICAL(&sStateMutex);
  if (!shouldSchedule) return;

  const CHIP_ERROR error = chip::DeviceLayer::PlatformMgr().ScheduleWork(reportValveStateOnMatterThread, 0);
  if (error != CHIP_NO_ERROR) {
    ESP_LOGW("matter", "Unable to schedule valve state report: %" CHIP_ERROR_FORMAT, error.Format());
    portENTER_CRITICAL(&sStateMutex);
    sReportPending = false;
    portEXIT_CRITICAL(&sStateMutex);
  }
}

void printCommissioningInfo() {
  char qrPayload[128] = {};
  chip::MutableCharSpan qrSpan(qrPayload);
  char manualCode[32] = {};
  chip::MutableCharSpan manualSpan(manualCode);
  if (GetQRCode(qrSpan, rendezvousFlags()) != CHIP_NO_ERROR ||
      GetManualPairingCode(manualSpan, rendezvousFlags()) != CHIP_NO_ERROR) {
    ESP_LOGW("matter", "Unable to generate Matter onboarding codes");
    return;
  }
  printf("\nDelta Touch2O Matter controller is ready for commissioning.\n");
  printf("Manual pairing code: %s\n", manualCode);
  printf("QR payload: %s\n", qrPayload);
  printf("QR code URL: https://project-chip.github.io/connectedhomeip/qrcode.html?data=%s\n", qrPayload);
}

}  // namespace matter_water_valve
