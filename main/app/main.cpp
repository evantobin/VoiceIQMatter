#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <nvs_flash.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "debug/web_log.h"
#include "faucet/touch2o_controller.h"
#include "matter/matter_water_valve.h"

extern "C" void app_main() {
  esp_err_t error = nvs_flash_init();
  if (error == ESP_ERR_NVS_NO_FREE_PAGES || error == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    error = nvs_flash_init();
  }
  ESP_ERROR_CHECK(error);
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  web_log::begin();

  faucet::Touch2OController faucet;
  if (!matter_water_valve::begin(faucet)) {
    ESP_LOGE("app", "Matter faucet endpoint setup failed");
    return;
  }
  faucet.setStateCallback(matter_water_valve::publishValveState);
  faucet.begin();

  if (!matter_water_valve::start()) {
    ESP_LOGE("app", "Matter stack failed to start");
    return;
  }
  matter_water_valve::printCommissioningInfo();

  while (true) {
    faucet.poll();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
