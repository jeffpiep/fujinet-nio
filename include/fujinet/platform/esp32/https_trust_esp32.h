#pragma once

extern "C" {
#include "esp_err.h"
#include "esp_http_client.h"
#include "esp_tls.h"
}

namespace fujinet::platform::esp32 {

// Apply compile-time HTTPS trust policy to esp_http_client_config_t.
void configure_https_trust(esp_http_client_config_t& cfg);

// Apply compile-time TLS trust policy to esp_tls_cfg_t.
void configure_tls_trust(esp_tls_cfg_t& cfg);

} // namespace fujinet::platform::esp32
