#include "fujinet/platform/esp32/tls_diagnostics.h"

#include <cstdio>
#include <cstring>

#include "fujinet/core/logging.h"

extern "C" {
#include "esp_err.h"
#include "esp_tls_errors.h"

#if CONFIG_ESP_TLS_USING_MBEDTLS
#include "mbedtls/error.h"
#include "mbedtls/x509.h"
#endif

#include <sys/time.h>
#include <time.h>
}

namespace fujinet::platform::esp32 {

namespace {

static constexpr const char* TAG = "tls_diag";

static const char* https_trust_mode_name(HttpsTrustMode mode)
{
    switch (mode) {
    case HttpsTrustMode_EmbeddedTestCaOnly: return "embedded_test_ca_only";
    case HttpsTrustMode_EspCrtBundle:       return "esp_crt_bundle";
    case HttpsTrustMode_EspCrtBundlePlusTestCa: return "esp_crt_bundle_plus_test_ca";
    case HttpsTrustMode_Unverified:         return "unverified";
    }
    return "unknown";
}

static const char* esp_tls_err_name(esp_err_t err)
{
    switch (err) {
    case ESP_OK: return "ESP_OK";
    case ESP_ERR_ESP_TLS_CANNOT_RESOLVE_HOSTNAME: return "ESP_ERR_ESP_TLS_CANNOT_RESOLVE_HOSTNAME";
    case ESP_ERR_ESP_TLS_CANNOT_CREATE_SOCKET: return "ESP_ERR_ESP_TLS_CANNOT_CREATE_SOCKET";
    case ESP_ERR_ESP_TLS_UNSUPPORTED_PROTOCOL_FAMILY: return "ESP_ERR_ESP_TLS_UNSUPPORTED_PROTOCOL_FAMILY";
    case ESP_ERR_ESP_TLS_FAILED_CONNECT_TO_HOST: return "ESP_ERR_ESP_TLS_FAILED_CONNECT_TO_HOST";
    case ESP_ERR_ESP_TLS_SOCKET_SETOPT_FAILED: return "ESP_ERR_ESP_TLS_SOCKET_SETOPT_FAILED";
    case ESP_ERR_ESP_TLS_CONNECTION_TIMEOUT: return "ESP_ERR_ESP_TLS_CONNECTION_TIMEOUT";
    case ESP_ERR_ESP_TLS_TCP_CLOSED_FIN: return "ESP_ERR_ESP_TLS_TCP_CLOSED_FIN";
    case ESP_ERR_MBEDTLS_CERT_PARTLY_OK: return "ESP_ERR_MBEDTLS_CERT_PARTLY_OK";
    case ESP_ERR_MBEDTLS_SSL_SET_HOSTNAME_FAILED: return "ESP_ERR_MBEDTLS_SSL_SET_HOSTNAME_FAILED";
    case ESP_ERR_MBEDTLS_X509_CRT_PARSE_FAILED: return "ESP_ERR_MBEDTLS_X509_CRT_PARSE_FAILED";
    case ESP_ERR_MBEDTLS_SSL_HANDSHAKE_FAILED: return "ESP_ERR_MBEDTLS_SSL_HANDSHAKE_FAILED";
    default: return "unknown";
    }
}

#if CONFIG_ESP_TLS_USING_MBEDTLS
static void append_x509_flag(char* out, std::size_t out_len, int flags, int bit, const char* name)
{
    if ((flags & bit) == 0) {
        return;
    }

    if (out[0] != '\0') {
        std::strncat(out, ", ", out_len - std::strlen(out) - 1);
    }
    std::strncat(out, name, out_len - std::strlen(out) - 1);
}

static void log_x509_verify_flags(int flags)
{
    if (flags == 0) {
        FN_LOGE(TAG, "  cert_verify_flags: none set");
        return;
    }

    char decoded[384];
    decoded[0] = '\0';

    append_x509_flag(decoded, sizeof(decoded), flags, MBEDTLS_X509_BADCERT_EXPIRED, "expired");
    append_x509_flag(decoded, sizeof(decoded), flags, MBEDTLS_X509_BADCERT_REVOKED, "revoked");
    append_x509_flag(decoded, sizeof(decoded), flags, MBEDTLS_X509_BADCERT_CN_MISMATCH, "cn_mismatch");
    append_x509_flag(decoded, sizeof(decoded), flags, MBEDTLS_X509_BADCERT_NOT_TRUSTED, "not_trusted");
    append_x509_flag(decoded, sizeof(decoded), flags, MBEDTLS_X509_BADCERT_MISSING, "missing");
    append_x509_flag(decoded, sizeof(decoded), flags, MBEDTLS_X509_BADCERT_SKIP_VERIFY, "skip_verify");
    append_x509_flag(decoded, sizeof(decoded), flags, MBEDTLS_X509_BADCERT_OTHER, "other");
    append_x509_flag(decoded, sizeof(decoded), flags, MBEDTLS_X509_BADCERT_FUTURE, "future");
    append_x509_flag(decoded, sizeof(decoded), flags, MBEDTLS_X509_BADCERT_KEY_USAGE, "key_usage");
    append_x509_flag(decoded, sizeof(decoded), flags, MBEDTLS_X509_BADCERT_EXT_KEY_USAGE, "ext_key_usage");
    append_x509_flag(decoded, sizeof(decoded), flags, MBEDTLS_X509_BADCERT_NS_CERT_TYPE, "ns_cert_type");
    append_x509_flag(decoded, sizeof(decoded), flags, MBEDTLS_X509_BADCERT_BAD_MD, "bad_md");
    append_x509_flag(decoded, sizeof(decoded), flags, MBEDTLS_X509_BADCERT_BAD_PK, "bad_pk");
    append_x509_flag(decoded, sizeof(decoded), flags, MBEDTLS_X509_BADCERT_BAD_KEY, "bad_key");

    FN_LOGE(TAG, "  cert_verify_flags: 0x%08x (%s)", flags, decoded[0] ? decoded : "unrecognized");
}

static void log_mbedtls_error_code(int code)
{
    if (code == 0) {
        FN_LOGE(TAG, "  mbedtls_code: 0");
        return;
    }

    char buf[192];
    mbedtls_strerror(code, buf, sizeof(buf));
    FN_LOGE(TAG, "  mbedtls_code: -0x%04x (%s)", static_cast<unsigned>(-code), buf);
}
#endif

static void log_tls_error_snapshot(const char* context,
                                   esp_err_t last_error,
                                   int mbedtls_code,
                                   int cert_flags)
{
    FN_LOGE(TAG, "TLS failure [%s]", context ? context : "unknown");
    FN_LOGE(TAG, "  esp_tls_last_error: 0x%x (%s)", static_cast<unsigned>(last_error),
            esp_tls_err_name(last_error));

#if CONFIG_ESP_TLS_USING_MBEDTLS
    log_mbedtls_error_code(mbedtls_code);
    log_x509_verify_flags(cert_flags);

    if (cert_flags & MBEDTLS_X509_BADCERT_NOT_TRUSTED) {
        FN_LOGE(TAG, "  hint: server chain not signed by configured trust anchor(s)");
    }
    if (cert_flags & MBEDTLS_X509_BADCERT_CN_MISMATCH) {
        FN_LOGE(TAG, "  hint: hostname/SNI does not match certificate CN/SAN");
    }
    if ((cert_flags & (MBEDTLS_X509_BADCERT_EXPIRED | MBEDTLS_X509_BADCERT_FUTURE)) != 0) {
        FN_LOGE(TAG, "  hint: certificate validity window issue; check SNTP/time sync");
    }
    if ((cert_flags & (MBEDTLS_X509_BADCERT_BAD_MD | MBEDTLS_X509_BADCERT_BAD_PK | MBEDTLS_X509_BADCERT_BAD_KEY)) != 0) {
        FN_LOGE(TAG, "  hint: signature/key algorithm may be disabled in sdkconfig (e.g. SHA-512/ECDSA)");
    }
#else
    FN_LOGE(TAG, "  mbedtls_code: %d (mbedTLS diagnostics unavailable)", mbedtls_code);
    FN_LOGE(TAG, "  cert_verify_flags: 0x%08x", cert_flags);
#endif
}

} // namespace

void log_https_trust_mode(HttpsTrustMode mode)
{
    FN_LOGI(TAG, "HTTPS trust mode: %s", https_trust_mode_name(mode));

    switch (mode) {
    case HttpsTrustMode_EmbeddedTestCaOnly:
        FN_LOGI(TAG, "  using cert_pem only (FujiNet test CA); public CA roots are NOT included");
        break;
    case HttpsTrustMode_EspCrtBundle:
#if defined(CONFIG_MBEDTLS_CERTIFICATE_BUNDLE)
        FN_LOGI(TAG, "  using ESP-IDF certificate bundle");
#else
        FN_LOGW(TAG, "  crt bundle requested but CONFIG_MBEDTLS_CERTIFICATE_BUNDLE is disabled");
#endif
        break;
    case HttpsTrustMode_EspCrtBundlePlusTestCa:
#if defined(CONFIG_MBEDTLS_CERTIFICATE_BUNDLE)
        FN_LOGI(TAG, "  using ESP-IDF certificate bundle plus FujiNet test CA fallback");
#else
        FN_LOGW(TAG, "  additive test CA requested but CONFIG_MBEDTLS_CERTIFICATE_BUNDLE is disabled");
#endif
        break;
    case HttpsTrustMode_Unverified:
        FN_LOGW(TAG, "  certificate verification disabled");
        break;
    }
}

void log_device_time_context()
{
    struct timeval tv {};
    if (gettimeofday(&tv, nullptr) != 0) {
        FN_LOGE(TAG, "device time: unavailable (gettimeofday failed)");
        return;
    }

    const time_t epoch = tv.tv_sec;
    struct tm utc {};
    gmtime_r(&epoch, &utc);

    char iso[64];
    std::snprintf(iso, sizeof(iso), "%04d-%02d-%02dT%02d:%02d:%02dZ",
                  utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday,
                  utc.tm_hour, utc.tm_min, utc.tm_sec);

    FN_LOGE(TAG, "device time: %s (epoch=%ld)", iso, static_cast<long>(epoch));

    if (epoch < 1577836800L) { // 2020-01-01
        FN_LOGE(TAG, "  hint: clock appears unsynced; TLS cert validation may fail");
    }
}

void log_mbedtls_build_config()
{
    static bool logged = false;
    if (logged) {
        return;
    }
    logged = true;

#if CONFIG_ESP_TLS_USING_MBEDTLS
#if defined(CONFIG_MBEDTLS_CERTIFICATE_BUNDLE)
    FN_LOGI(TAG, "build: CONFIG_MBEDTLS_CERTIFICATE_BUNDLE enabled");
#else
    FN_LOGI(TAG, "build: CONFIG_MBEDTLS_CERTIFICATE_BUNDLE disabled");
#endif

#if defined(CONFIG_MBEDTLS_SHA512_C)
    FN_LOGI(TAG, "build: CONFIG_MBEDTLS_SHA512_C enabled");
#else
    FN_LOGI(TAG, "build: CONFIG_MBEDTLS_SHA512_C disabled (SHA-384/512 cert signatures may fail)");
#endif
#else
    FN_LOGI(TAG, "build: mbedTLS not selected for ESP-TLS");
#endif
}

void log_tls_last_error(const char* context, const esp_tls_last_error_t* handle)
{
    log_mbedtls_build_config();
    log_device_time_context();

    if (!handle) {
        FN_LOGE(TAG, "TLS failure [%s]: no esp-tls error handle available",
                context ? context : "unknown");
        return;
    }

    log_tls_error_snapshot(context,
                           handle->last_error,
                           handle->esp_tls_error_code,
                           handle->esp_tls_flags);
}

void log_esp_tls_connection_failure(const char* context, esp_tls_t* tls, int conn_result)
{
    log_mbedtls_build_config();
    log_device_time_context();

    FN_LOGE(TAG, "TLS connect [%s] result=%d", context ? context : "unknown", conn_result);

    if (!tls) {
        FN_LOGE(TAG, "  esp_tls handle is null");
        return;
    }

    esp_tls_error_handle_t err_handle = nullptr;
    if (esp_tls_get_error_handle(tls, &err_handle) != ESP_OK || !err_handle) {
        FN_LOGE(TAG, "  esp-tls error handle unavailable");
        return;
    }

    log_tls_error_snapshot(context,
                           err_handle->last_error,
                           err_handle->esp_tls_error_code,
                           err_handle->esp_tls_flags);
}

} // namespace fujinet::platform::esp32
