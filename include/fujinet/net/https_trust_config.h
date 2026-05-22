#pragma once

#include <cstdint>

// HTTPS/TLS trust policy (compile-time):
//
//   (default)                 Platform roots only
//                             ESP32: ESP-IDF certificate bundle
//                             POSIX: system CA store via curl/OpenSSL
//
//   FN_HTTPS_TEST_CA=1        Trust only the embedded FujiNet test CA
//                             (local integration/regression servers)
//
//   FN_HTTPS_TEST_CA_ADDITIVE=1
//                             Platform roots PLUS the FujiNet test CA
//                             (matches POSIX dev behaviour)
//
// Define at most one of FN_HTTPS_TEST_CA or FN_HTTPS_TEST_CA_ADDITIVE.

#ifndef FN_HTTPS_TEST_CA
#define FN_HTTPS_TEST_CA 0
#endif

#ifndef FN_HTTPS_TEST_CA_ADDITIVE
#define FN_HTTPS_TEST_CA_ADDITIVE 0
#endif

#if FN_HTTPS_TEST_CA && FN_HTTPS_TEST_CA_ADDITIVE
#error "Define at most one of FN_HTTPS_TEST_CA or FN_HTTPS_TEST_CA_ADDITIVE"
#endif

namespace fujinet::net {

enum class HttpsTrustPolicy : std::uint8_t {
    PlatformDefault = 0,
    TestCaOnly,
    PlatformPlusTestCa,
};

inline constexpr HttpsTrustPolicy https_trust_policy()
{
#if FN_HTTPS_TEST_CA
    return HttpsTrustPolicy::TestCaOnly;
#elif FN_HTTPS_TEST_CA_ADDITIVE
    return HttpsTrustPolicy::PlatformPlusTestCa;
#else
    return HttpsTrustPolicy::PlatformDefault;
#endif
}

} // namespace fujinet::net
