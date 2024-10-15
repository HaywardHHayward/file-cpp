#include "Utf8Sequence.hpp"
#include <bit>
#include <cassert>
#include "../Unicode.hpp"

template<class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};

template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

namespace File::Unicode {
    constexpr bool Utf8Sequence::isInvalid(const std::uint8_t byte) noexcept {
        return byte == 0xC0 || byte == 0xC1 || byte == 0xF5;
    }

    Utf8Sequence::Utf8Sequence(const Utf8Type type): m_data{type}, m_currentLength{1} { }

    std::uint8_t& Utf8Sequence::at(std::size_t index) {
        assert(index < fullLen());
        return std::visit(overloaded{
                              [index](auto& arg) {
                                  return std::ref(arg.at(index));
                              },
                              [](Ascii& arg) {
                                  return std::ref(arg);
                              },
                          }, m_data);
    }

    std::size_t Utf8Sequence::fullLen() const {
        return std::visit(overloaded{
                              [](auto& arg) {
                                  return arg.size();
                              },
                              [](Ascii _) {
                                  return static_cast<std::size_t>(1);
                              },
                          }, m_data);
    }

    std::size_t Utf8Sequence::currentLen() const {
        return m_currentLength;
    }

    std::uint32_t Utf8Sequence::getCodepoint() {
        std::uint32_t codepoint = std::visit(overloaded{
                                                 [](const Ascii arg) {
                                                     return static_cast<std::uint32_t>(arg);
                                                 },
                                                 [this](const Western& arg) {
                                                     return static_cast<std::uint32_t>(
                                                         arg.at(0) ^ 0b1100'0000);
                                                 },
                                                 [this](const Bmp& arg) {
                                                     return static_cast<std::uint32_t>(
                                                         arg.at(0) ^ 0b1110'0000);
                                                 },
                                                 [this](const Other& arg) {
                                                     return static_cast<std::uint32_t>(
                                                         arg.at(0) ^ 0b1111'0000);
                                                 },
                                             }, m_data);
        for (int i = 1; i < fullLen(); i++) {
            codepoint = (codepoint << 6) | (at(i) ^ 0b10'000000);
        }
        return codepoint;
    }

    std::optional<Utf8Sequence> Utf8Sequence::build(Point byte) {
        if ((0x80 <= byte && byte <= 0xBF) || isInvalid(byte)) {
            return std::nullopt;
        }
        Utf8Type type;
        switch (std::countl_one(byte)) {
            case 0:
                type = byte;
                break;
            case 2:
                type = Western{byte, 0};
                break;
            case 3:
                type = Bmp{byte, 0, 0};
                break;
            case 4:
                type = Other{byte, 0, 0, 0};
                break;
            default:
                return std::nullopt;
        }
        return Utf8Sequence{type};
    }

    bool Utf8Sequence::isComplete() const {
        return fullLen() == m_currentLength;
    }

    bool Utf8Sequence::addPoint(const Point point) {
        if (m_currentLength >= fullLen()) {
            return false;
        }
        if ((0b10'000000 > point || point >= 0b11'000000) || isInvalid(point)) {
            return false;
        }
        at(m_currentLength) = point;
        m_currentLength++;
        return true;
    }

    bool Utf8Sequence::isValid() {
        const std::uint32_t codepoint = getCodepoint();
        if (!isText(codepoint)) {
            return false;
        }
        if (std::holds_alternative<Ascii>(m_data)) {
            return codepoint <= 0x7F;
        }
        if (std::holds_alternative<Western>(m_data)) {
            return 0x80 <= codepoint && codepoint <= 0x7FF;
        }
        if (std::holds_alternative<Bmp>(m_data)) {
            return 0x800 <= codepoint && codepoint <= 0xFFFF;
        }
        return 0x10000 <= codepoint && codepoint <= 0x10FFFF;
    }
}
