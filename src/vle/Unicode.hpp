#ifndef UNICODE_HPP
#define UNICODE_HPP

#include <cuchar>
#include <cstdint>

namespace File::Unicode {
    [[nodiscard]] constexpr bool isText(const std::uint32_t codepoint) {
        return !((codepoint < 0xFF)
                 && !(0x08 <= codepoint && 0x0D >= codepoint)
                 && codepoint != 0x1B
                 && !(0x20 <= codepoint && 0x7E >= codepoint)
                 && 0xA0 > codepoint);
    }

    enum class Endianness {
        bigEndian,
        littleEndian
    };
}


#endif //UNICODE_HPP
