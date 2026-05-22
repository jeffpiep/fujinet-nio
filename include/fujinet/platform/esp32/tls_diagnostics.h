#pragma once

#include <cstdint>

extern "C" {
#include "esp_tls.h"
}

namespace fujinet::platform::esp32 {

enum HttpsTrustMode : std::uint8_t {
    HttpsTrustMode_EmbeddedTestCaOnly = 0,
    HttpsTrustMode_EspCrtBundle,
    HttpsTrustMode_EspCrtBundlePlusTestCa,
    HttpsTrustMode_Unverified,
};

// Log which CA trust source HTTPS/TLS will use for the upcoming connection.
void log_https_trust_mode(HttpsTrustMode mode);

// Log current device time (helps diagnose cert expiry / not-yet-valid failures).
void log_device_time_context();

// Log compile-time mbedTLS options relevant to TLS handshake failures.
void log_mbedtls_build_config();

// Log details from an esp-tls last-error record (HTTP client passes this on HTTP_EVENT_ERROR).
void log_tls_last_error(const char* context, const esp_tls_last_error_t* error);

// Log TLS failure details from a live esp_tls connection handle.
void log_esp_tls_connection_failure(const char* context, esp_tls_t* tls, int conn_result);

} // namespace fujinet::platform::esp32
