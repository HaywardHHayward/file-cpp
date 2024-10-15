#ifndef GBSEQUENCE_HPP
#define GBSEQUENCE_HPP

#include <array>
#include <cstdint>
#include <optional>

namespace File {
    class GbSequence {
        std::array<std::uint8_t, 4> m_data;
        std::uint8_t m_currentLength : 3;
        bool m_isComplete : 1;

        GbSequence(std::array<std::uint8_t, 4>&& data, bool isComplete) noexcept;

    public:
        using Point = std::uint8_t;

        static std::optional<GbSequence> build(Point byte) noexcept;

        [[nodiscard]] bool isComplete() const;

        bool addPoint(Point point);

        [[nodiscard]] bool isValid() const;
    };
} // File

#endif //GBSEQUENCE_HPP
