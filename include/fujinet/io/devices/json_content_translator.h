#pragma once

#include "fujinet/io/devices/content_translator.h"

#include <string>

namespace fujinet::io {

class JsonContentTranslator final : public IContentTranslator {
public:
    StatusCode configure(const TranslationConfig& config) override;
    void reset() override;
    StatusCode append_body(const std::uint8_t* data, std::size_t len) override;
    StatusCode finalize() override;
    std::uint64_t translated_size() const override;
    StatusCode read(std::uint32_t offset,
                    std::uint8_t* out,
                    std::size_t maxBytes,
                    std::uint16_t& actual,
                    bool& eof) const override;

private:
    TranslationConfig _config{};
    std::string _body;
    std::string _translated;
};

} // namespace fujinet::io
