#include "fujinet/io/devices/json_content_translator.h"

#include "cJSON.h"
#include "cJSON_Utils.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <sstream>

namespace fujinet::io {

namespace {

bool is_approx_integer(double v)
{
    double i = 0.0;
    return std::modf(v, &i) == 0.0 && v >= -9007199254740992.0 && v <= 9007199254740992.0;
}

std::string json_item_to_string(cJSON* item)
{
    if (!item) return {};

    std::ostringstream ss;

    if (cJSON_IsString(item)) {
        char* s = cJSON_GetStringValue(item);
        if (s) ss << s;
    } else if (cJSON_IsBool(item)) {
        ss << (cJSON_IsTrue(item) ? "TRUE" : "FALSE");
    } else if (cJSON_IsNull(item)) {
        ss << "NULL";
    } else if (cJSON_IsNumber(item)) {
        double num = cJSON_GetNumberValue(item);
        if (is_approx_integer(num)) {
            ss << static_cast<std::int64_t>(num);
        } else {
            ss << std::setprecision(10) << num;
        }
    } else if (cJSON_IsObject(item)) {
        cJSON* child = item->child;
        if (child) {
            do {
                ss << child->string << "\n" << json_item_to_string(child);
                if (child->next) ss << "\n";
            } while ((child = child->next) != nullptr);
        }
    } else if (cJSON_IsArray(item)) {
        cJSON* child = item->child;
        bool first = true;
        if (child) {
            do {
                if (!first) ss << "\n";
                ss << json_item_to_string(child);
                first = false;
            } while ((child = child->next) != nullptr);
        }
    }

    return ss.str();
}

} // namespace

StatusCode JsonContentTranslator::configure(const TranslationConfig& config)
{
    if (config.type != ContentTranslationType::Json) {
        return StatusCode::InvalidRequest;
    }

    _config = config;
    _body.clear();
    _translated.clear();
    return StatusCode::Ok;
}

void JsonContentTranslator::reset()
{
    _body.clear();
    _translated.clear();
}

StatusCode JsonContentTranslator::append_body(const std::uint8_t* data, std::size_t len)
{
    if (len == 0) {
        return StatusCode::Ok;
    }

    if (!data) {
        return StatusCode::InvalidRequest;
    }

    _body.append(reinterpret_cast<const char*>(data), len);
    return StatusCode::Ok;
}

StatusCode JsonContentTranslator::finalize()
{
    _translated.clear();

    cJSON* json = cJSON_Parse(_body.c_str());
    if (!json) {
        return StatusCode::Ok;
    }

    cJSON* item = cJSONUtils_GetPointer(json, _config.selector.c_str());
    if (item) {
        _translated = json_item_to_string(item);
    }

    cJSON_Delete(json);
    return StatusCode::Ok;
}

std::uint64_t JsonContentTranslator::translated_size() const
{
    return static_cast<std::uint64_t>(_translated.size());
}

StatusCode JsonContentTranslator::read(std::uint32_t offset,
                                       std::uint8_t* out,
                                       std::size_t maxBytes,
                                       std::uint16_t& actual,
                                       bool& eof) const
{
    actual = 0;
    eof = false;

    const auto total = static_cast<std::uint32_t>(_translated.size());
    if (offset > total) {
        return StatusCode::InvalidRequest;
    }

    const auto remaining = total - offset;
    const auto n = std::min<std::size_t>(remaining, maxBytes);
    if (n > 0 && out) {
        std::memcpy(out, _translated.data() + offset, n);
    }

    actual = static_cast<std::uint16_t>(n);
    eof = (offset + actual) >= total;
    return StatusCode::Ok;
}

} // namespace fujinet::io
