#include "fujinet/platform/esp32/https_trust_esp32.h"

#include "fujinet/core/logging.h"
#include "fujinet/net/https_trust_config.h"
#include "fujinet/net/test_ca_cert.h"
#include "fujinet/platform/esp32/tls_diagnostics.h"

extern "C" {
#include "esp_crt_bundle.h"

#if CONFIG_ESP_TLS_USING_MBEDTLS
#include "mbedtls/md.h"
#include "mbedtls/pk.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/x509.h"

int esp_crt_verify_callback(void* buf, mbedtls_x509_crt* const crt, int depth, uint32_t* flags);
#endif
}

namespace fujinet::platform::esp32 {

namespace {

static constexpr const char* TAG = "tls_diag";

#if CONFIG_ESP_TLS_USING_MBEDTLS && defined(CONFIG_MBEDTLS_CERTIFICATE_BUNDLE)

static mbedtls_x509_crt s_test_ca;
static bool s_test_ca_ready = false;

static bool ensure_test_ca_parsed()
{
    if (s_test_ca_ready) {
        return true;
    }

    mbedtls_x509_crt_init(&s_test_ca);
    const int ret = mbedtls_x509_crt_parse(
        &s_test_ca,
        reinterpret_cast<const unsigned char*>(fujinet::net::test_ca_cert_pem),
        sizeof(fujinet::net::test_ca_cert_pem));
    if (ret != 0) {
        FN_LOGE(TAG, "HTTPS: failed to parse FujiNet test CA (-0x%04x)", -ret);
        mbedtls_x509_crt_free(&s_test_ca);
        return false;
    }

    s_test_ca_ready = true;
    return true;
}

static int verify_cert_signed_by_test_ca(const mbedtls_x509_crt* child)
{
    if (!mbedtls_pk_can_do(&s_test_ca.pk, child->MBEDTLS_PRIVATE(sig_pk))) {
        return MBEDTLS_ERR_PK_TYPE_MISMATCH;
    }

    const mbedtls_md_info_t* md_info = mbedtls_md_info_from_type(child->MBEDTLS_PRIVATE(sig_md));
    if (!md_info) {
        return MBEDTLS_ERR_X509_FEATURE_UNAVAILABLE;
    }

    unsigned char hash[MBEDTLS_MD_MAX_SIZE];
    const size_t md_size = mbedtls_md_get_size(md_info);
    int ret = mbedtls_md(md_info, child->tbs.p, child->tbs.len, hash);
    if (ret != 0) {
        return ret;
    }

    const mbedtls_x509_buf& sig = child->MBEDTLS_PRIVATE(sig);
    return mbedtls_pk_verify_ext(child->MBEDTLS_PRIVATE(sig_pk), child->MBEDTLS_PRIVATE(sig_opts),
                                 &s_test_ca.pk, child->MBEDTLS_PRIVATE(sig_md),
                                 hash, md_size, sig.p, sig.len);
}

static int verify_with_test_ca_fallback(void* /*data*/,
                                        mbedtls_x509_crt* const crt,
                                        int depth,
                                        uint32_t* flags)
{
    const int bundle_ret = esp_crt_verify_callback(nullptr, crt, depth, flags);
    if (bundle_ret == 0) {
        return 0;
    }

    if ((*flags & MBEDTLS_X509_BADCERT_NOT_TRUSTED) == 0) {
        return bundle_ret;
    }

    if (!ensure_test_ca_parsed()) {
        return bundle_ret;
    }

    const int sig = verify_cert_signed_by_test_ca(crt);
    if (sig == 0) {
        FN_LOGI(TAG, "HTTPS: certificate verified via FujiNet test CA fallback");
        *flags = 0;
        return 0;
    }

    return bundle_ret;
}

static esp_err_t crt_bundle_plus_test_ca_attach(void* conf)
{
    const esp_err_t err = esp_crt_bundle_attach(conf);
    if (err != ESP_OK) {
        return err;
    }

    if (!conf) {
        return ESP_OK;
    }

    auto* ssl_conf = static_cast<mbedtls_ssl_config*>(conf);
    mbedtls_ssl_conf_verify(ssl_conf, verify_with_test_ca_fallback, nullptr);
    return ESP_OK;
}

#endif // CONFIG_ESP_TLS_USING_MBEDTLS && CONFIG_MBEDTLS_CERTIFICATE_BUNDLE

static HttpsTrustMode trust_mode_for_policy(fujinet::net::HttpsTrustPolicy policy)
{
    switch (policy) {
    case fujinet::net::HttpsTrustPolicy::TestCaOnly:
        return HttpsTrustMode_EmbeddedTestCaOnly;
    case fujinet::net::HttpsTrustPolicy::PlatformPlusTestCa:
        return HttpsTrustMode_EspCrtBundlePlusTestCa;
    case fujinet::net::HttpsTrustPolicy::PlatformDefault:
        return HttpsTrustMode_EspCrtBundle;
    }
    return HttpsTrustMode_EspCrtBundle;
}

static void apply_tls_trust_cfg(esp_tls_cfg_t& cfg, fujinet::net::HttpsTrustPolicy policy)
{
    cfg.crt_bundle_attach = nullptr;
    cfg.cacert_buf = nullptr;
    cfg.cacert_bytes = 0;
    cfg.use_global_ca_store = false;

    switch (policy) {
    case fujinet::net::HttpsTrustPolicy::TestCaOnly:
        cfg.cacert_buf = reinterpret_cast<const unsigned char*>(fujinet::net::test_ca_cert_pem);
        cfg.cacert_bytes = sizeof(fujinet::net::test_ca_cert_pem);
        break;

    case fujinet::net::HttpsTrustPolicy::PlatformPlusTestCa:
#if CONFIG_ESP_TLS_USING_MBEDTLS && defined(CONFIG_MBEDTLS_CERTIFICATE_BUNDLE)
        cfg.crt_bundle_attach = crt_bundle_plus_test_ca_attach;
#else
        FN_LOGW(TAG, "HTTPS: additive test CA requested but certificate bundle is unavailable; using test CA only");
        cfg.cacert_buf = reinterpret_cast<const unsigned char*>(fujinet::net::test_ca_cert_pem);
        cfg.cacert_bytes = sizeof(fujinet::net::test_ca_cert_pem);
#endif
        break;

    case fujinet::net::HttpsTrustPolicy::PlatformDefault:
#if defined(CONFIG_MBEDTLS_CERTIFICATE_BUNDLE)
        cfg.crt_bundle_attach = esp_crt_bundle_attach;
#else
        FN_LOGW(TAG, "HTTPS: certificate bundle unavailable; TLS verification may fail");
#endif
        break;
    }
}

} // namespace

void configure_https_trust(esp_http_client_config_t& cfg)
{
    const auto policy = fujinet::net::https_trust_policy();
    log_https_trust_mode(trust_mode_for_policy(policy));

    cfg.cert_pem = nullptr;
    cfg.crt_bundle_attach = nullptr;

    switch (policy) {
    case fujinet::net::HttpsTrustPolicy::TestCaOnly:
        cfg.cert_pem = fujinet::net::test_ca_cert_pem;
        break;

    case fujinet::net::HttpsTrustPolicy::PlatformPlusTestCa:
#if CONFIG_ESP_TLS_USING_MBEDTLS && defined(CONFIG_MBEDTLS_CERTIFICATE_BUNDLE)
        cfg.crt_bundle_attach = crt_bundle_plus_test_ca_attach;
#else
        FN_LOGW(TAG, "HTTPS: additive test CA requested but certificate bundle is unavailable; using test CA only");
        cfg.cert_pem = fujinet::net::test_ca_cert_pem;
#endif
        break;

    case fujinet::net::HttpsTrustPolicy::PlatformDefault:
#if defined(CONFIG_MBEDTLS_CERTIFICATE_BUNDLE)
        cfg.crt_bundle_attach = esp_crt_bundle_attach;
#else
        FN_LOGW(TAG, "HTTPS: certificate bundle unavailable; TLS verification may fail");
#endif
        break;
    }
}

void configure_tls_trust(esp_tls_cfg_t& cfg)
{
    const auto policy = fujinet::net::https_trust_policy();
    log_https_trust_mode(trust_mode_for_policy(policy));
    apply_tls_trust_cfg(cfg, policy);
}

} // namespace fujinet::platform::esp32
