#include "Utf16Sequence.hpp"
#include "../Unicode.hpp"


namespace File::Unicode {
    Utf16Sequence::Utf16Sequence(const Utf16Type data) : m_data {data} { }

    std::uint32_t Utf16Sequence::getCodepoint() const {
        if (std::holds_alternative<Bmp>(m_data)) {
            return std::get<Bmp>(m_data);
        }
        const std::array data {std::get<Surrogate>(m_data).data};;
        const std::uint32_t high {data[0]};
        const std::uint32_t low {data[1]};
        return ((high - 0xD800) * 0x400) + (low - 0xDC00) + 0x10000;
    }

    std::optional<Utf16Sequence> Utf16Sequence::build(std::uint16_t point) {
        if (0xD800 <= point && point <= 0xDBFF) {
            return Utf16Sequence(Surrogate {.data = {point, 0}, .isComplete = false});
        }
        return Utf16Sequence(point);
    }

    bool Utf16Sequence::isComplete() const {
        if (std::holds_alternative<Bmp>(m_data)) {
            return true;
        }
        return std::get<Surrogate>(m_data).isComplete;
    }

    bool Utf16Sequence::addPoint(Point point) {
        if (std::holds_alternative<Bmp>(m_data)) {
            return false;
        }
        if (!(0xDC00 <= point && point <= 0xDFFF)) {
            return false;
        }
        std::get<Surrogate>(m_data).data[1] = point;
        std::get<Surrogate>(m_data).isComplete = true;
        return true;
    }

    bool Utf16Sequence::isValid() const {
        const std::uint32_t codepoint {getCodepoint()};
        if (std::holds_alternative<Bmp>(m_data)) {
            return (codepoint <= 0xD7FF || (0xE000 <= codepoint && codepoint <= 0xFFFF)) &&
                   isText(codepoint);
        }
        const bool isComplete {std::get<Surrogate>(m_data).isComplete};
        return isComplete && (0x10000 <= codepoint && codepoint <= 0x10FFFF) && isText(codepoint);
    }
}
