#include "matter/matter_water_valve.h"

#include <cstring>

#include <esp_log.h>
#include <esp_matter.h>
#include <esp_matter_attribute_utils.h>
#include <esp_matter_endpoint.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#include <app/server/Server.h>
#include <platform/CHIPDeviceLayer.h>
#include <setup_payload/OnboardingCodesUtil.h>

#include "faucet/touch2o_controller.h"
#include "matter/project_commissionable_data_provider.h"
#include "project_config.h"

namespace matter_water_valve {
namespace {

using namespace chip::app::Clusters;

faucet::Touch2OController *sFaucet = nullptr;
uint16_t sEndpointId = 0;
bool sValveOpen = false;
bool sReportPending = false;
bool sReportingPhysicalState = false;
portMUX_TYPE sStateMutex = portMUX_INITIALIZER_UNLOCKED;

void matterEventCallback(const ChipDeviceEvent *event, intptr_t) {
  if (event->Type == chip::DeviceLayer::DeviceEventType::kCommissioningComplete && sFaucet != nullptr) {
    sFaucet->enableProtocolTraffic();
  }
}

void reportValveStateOnMatterThread(intptr_t) {
  bool open;
  portENTER_CRITICAL(&sStateMutex);
  open = sValveOpen;
  sReportPending = false;
  portEXIT_CRITICAL(&sStateMutex);
  if (sEndpointId == 0) return;

  // Attribute updates also invoke the pre-update callback used for controller
  // commands. Mark this as physical feedback so it is not sent back to Touch2O.
  sReportingPhysicalState = true;
  esp_matter_attr_val_t value = esp_matter_bool(open);
  const esp_err_t error = esp_matter::attribute::update(
      sEndpointId, OnOff::Id, OnOff::Attributes::OnOff::Id, &value);
  sReportingPhysicalState = false;
  if (error != ESP_OK) {
    ESP_LOGW("matter", "Unable to publish physical valve state: %s", esp_err_to_name(error));
  }
}

esp_err_t attributeUpdateCallback(
    esp_matter::attribute::callback_type_t type,
    uint16_t endpointId,
    uint32_t clusterId,
    uint32_t attributeId,
    esp_matter_attr_val_t *value,
    void *) {
  if (type != esp_matter::attribute::PRE_UPDATE || endpointId != sEndpointId || sFaucet == nullptr ||
      sReportingPhysicalState) {
    return ESP_OK;
  }
  if (clusterId == OnOff::Id && attributeId == OnOff::Attributes::OnOff::Id) {
    ESP_LOGI("matter", "Apple Home requested faucet %s", value->val.b ? "on" : "off");
    sFaucet->requestValve(value->val.b);
  }
  return ESP_OK;
}

void configureMetadata() {
  esp_matter_attr_val_t value = esp_matter_char_str(
      const_cast<char *>(project_config::kManufacturer), strlen(project_config::kManufacturer));
  esp_matter::attribute::update(0, BasicInformation::Id, BasicInformation::Attributes::VendorName::Id, &value);
  value = esp_matter_char_str(
      const_cast<char *>(project_config::kProductName), strlen(project_config::kProductName));
  esp_matter::attribute::update(0, BasicInformation::Id, BasicInformation::Attributes::ProductName::Id, &value);
  value = esp_matter_char_str(const_cast<char *>(project_config::kModel), strlen(project_config::kModel));
  esp_matter::attribute::update(0, BasicInformation::Id, BasicInformation::Attributes::ProductLabel::Id, &value);
  value = esp_matter_char_str(
      const_cast<char *>(project_config::kProductName), strlen(project_config::kProductName));
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
  esp_matter::node_t *node = esp_matter::node::create(
      &nodeConfig, attributeUpdateCallback, nullptr, nullptr);
  if (node == nullptr) {
    ESP_LOGE("matter", "Failed to create Matter node");
    return false;
  }

  // Apple Home does not currently accept Matter's Water Valve device type.
  // Advertise a standard on/off plug-in unit so the faucet is controllable in
  // Apple Home, while the physical status packet remains authoritative.
  esp_matter::endpoint::on_off_plug_in_unit::config_t config;
  config.on_off.on_off = false;
  esp_matter::endpoint_t *endpoint = esp_matter::endpoint::on_off_plug_in_unit::create(
      node, &config, esp_matter::ENDPOINT_FLAG_NONE, nullptr);
  if (endpoint == nullptr) {
    ESP_LOGE("matter", "Failed to create Apple Home-compatible faucet endpoint");
    return false;
  }
  sEndpointId = esp_matter::endpoint::get_id(endpoint);
  configureMetadata();
  return true;
}

bool start() {
  installProjectCommissionableDataProvider();
  const esp_err_t error = esp_matter::start(matterEventCallback);
  if (error != ESP_OK) {
    ESP_LOGE("matter", "Failed to start Matter: %s", esp_err_to_name(error));
    return false;
  }
  // CommissioningComplete is not emitted again after a normal reboot. A
  // stored fabric means commissioning was completed on an earlier boot.
  if (sFaucet != nullptr && chip::Server::GetInstance().GetFabricTable().FabricCount() > 0) {
    sFaucet->enableProtocolTraffic();
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
