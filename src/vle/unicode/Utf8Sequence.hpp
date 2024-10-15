#ifndef UTF8SEQUENCE_HPP
#define UTF8SEQUENCE_HPP

#include <array>
#include <cstdint>
#include <optional>
#include <variant>

namespace File::Unicode {
    class Utf8Sequence {
        using Ascii = std::uint8_t;
        using Western = std::array<std::uint8_t, 2>;
        using Bmp = std::array<std::uint8_t, 3>;
        using Other = std::array<std::uint8_t, 4>;
        using Utf8Type = std::variant<Ascii, Western, Bmp, Other>;

        static constexpr bool isInvalid(std::uint8_t byte) noexcept;

        explicit Utf8Sequence(Utf8Type type);

        Utf8Type m_data;
        std::uint8_t m_currentLength;

        std::uint8_t& at(std::size_t index);

        [[nodiscard]] std::size_t fullLen() const;

        std::uint32_t getCodepoint();

    public:
        using Point = std::uint8_t;

        static std::optional<Utf8Sequence> build(Point byte);

        [[nodiscard]] bool isComplete() const;

        bool addPoint(Point point);

        [[nodiscard]] bool isValid();
    };
}

#endif //UTF8SEQUENCE_HPP
