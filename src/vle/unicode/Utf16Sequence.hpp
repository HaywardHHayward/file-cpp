#ifndef UTF16SEQUENCE_HPP
#define UTF16SEQUENCE_HPP

#include <array>
#include <cstdint>
#include <optional>
#include <variant>

namespace File::Unicode {
    class Utf16Sequence {
        struct Surrogate {
            std::array<uint16_t, 2> data;
            bool isComplete;
        };

        using Bmp = std::uint16_t;
        using Utf16Type = std::variant<Bmp, Surrogate>;

        Utf16Type m_data;

        explicit Utf16Sequence(Utf16Type data);

        [[nodiscard]] std::uint32_t getCodepoint() const;

    public:
        using Point = std::uint16_t;

        static std::optional<Utf16Sequence> build(Point point);

        [[nodiscard]] bool isComplete() const;

        bool addPoint(Point point);

        [[nodiscard]] bool isValid() const;
    };
}

#endif //UTF16SEQUENCE_HPP
