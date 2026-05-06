#pragma once

#include "fujinet/io/core/io_message.h"
#include "fujinet/io/devices/network_translation.h"

#include <cstddef>
#include <cstdint>

namespace fujinet::io {

class IContentTranslator {
public:
    virtual ~IContentTranslator() = default;

    virtual StatusCode configure(const TranslationConfig& config) = 0;
    virtual void reset() = 0;

    [[nodiscard]] virtual bool needs_full_body() const
    {
        return true;
    }

    virtual StatusCode append_body(const std::uint8_t* data, std::size_t len) = 0;
    virtual StatusCode finalize() = 0;

    [[nodiscard]] virtual std::uint64_t translated_size() const = 0;

    virtual StatusCode read(std::uint32_t offset,
                            std::uint8_t* out,
                            std::size_t maxBytes,
                            std::uint16_t& actual,
                            bool& eof) const = 0;
};

} // namespace fujinet::io
