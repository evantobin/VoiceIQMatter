#include "debug/web_log.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>

#include <esp_event.h>
#include <esp_http_server.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>

namespace web_log {
namespace {

constexpr size_t kLogBufferSize = 12 * 1024;
constexpr char kPage[] = R"HTML(<!doctype html><meta name="viewport" content="width=device-width,initial-scale=1"><title>Delta Touch2O Matter logs</title><style>body{margin:16px;background:#111;color:#ddd;font:14px ui-monospace,Menlo,monospace}pre{white-space:pre-wrap;word-break:break-word}</style><h2>Delta Touch2O Matter — live log</h2><pre id="log">Loading…</pre><script>const l=document.querySelector('#log');async function r(){try{const x=await fetch('/logs',{cache:'no-store'});l.textContent=await x.text()}catch(e){l.textContent='Connection lost; retrying…'}}r();setInterval(r,1000)</script>)HTML";

char sLogBuffer[kLogBufferSize]{};
size_t sLogStart = 0;
size_t sLogLength = 0;
portMUX_TYPE sLogMutex = portMUX_INITIALIZER_UNLOCKED;
vprintf_like_t sOriginalVprintf = nullptr;
httpd_handle_t sServer = nullptr;

void appendLog(const char *text, size_t length) {
  portENTER_CRITICAL(&sLogMutex);
  for (size_t i = 0; i < length; ++i) {
    if (sLogLength < kLogBufferSize) {
      sLogBuffer[(sLogStart + sLogLength++) % kLogBufferSize] = text[i];
    } else {
      sLogBuffer[sLogStart] = text[i];
      sLogStart = (sLogStart + 1) % kLogBufferSize;
    }
  }
  portEXIT_CRITICAL(&sLogMutex);
}

int logVprintf(const char *format, va_list arguments) {
  va_list copy;
  va_copy(copy, arguments);
  const int result = sOriginalVprintf ? sOriginalVprintf(format, arguments) : 0;
  char line[256];
  const int written = vsnprintf(line, sizeof(line), format, copy);
  va_end(copy);
  if (written > 0) appendLog(line, static_cast<size_t>(written < 256 ? written : 255));
  return result;
}

esp_err_t rootHandler(httpd_req_t *request) {
  httpd_resp_set_type(request, "text/html");
  return httpd_resp_send(request, kPage, HTTPD_RESP_USE_STRLEN);
}

esp_err_t logsHandler(httpd_req_t *request) {
  char *snapshot = static_cast<char *>(malloc(kLogBufferSize + 1));
  if (!snapshot) return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
  portENTER_CRITICAL(&sLogMutex);
  const size_t length = sLogLength;
  for (size_t i = 0; i < length; ++i) snapshot[i] = sLogBuffer[(sLogStart + i) % kLogBufferSize];
  portEXIT_CRITICAL(&sLogMutex);
  snapshot[length] = '\0';
  httpd_resp_set_type(request, "text/plain; charset=utf-8");
  const esp_err_t result = httpd_resp_send(request, snapshot, length);
  free(snapshot);
  return result;
}

void startServer(void *, esp_event_base_t, int32_t, void *) {
  if (sServer) return;
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.lru_purge_enable = true;
  if (httpd_start(&sServer, &config) != ESP_OK) return;
  const httpd_uri_t root = {.uri = "/", .method = HTTP_GET, .handler = rootHandler, .user_ctx = nullptr};
  const httpd_uri_t logs = {.uri = "/logs", .method = HTTP_GET, .handler = logsHandler, .user_ctx = nullptr};
  httpd_register_uri_handler(sServer, &root);
  httpd_register_uri_handler(sServer, &logs);
  ESP_LOGI("web_log", "Log page available at http://<device-ip>/");
}

}  // namespace

void begin() {
  sOriginalVprintf = esp_log_set_vprintf(logVprintf);
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, startServer, nullptr));
  ESP_LOGI("web_log", "Web log capture enabled; waiting for Wi-Fi");
}

}  // namespace web_log
