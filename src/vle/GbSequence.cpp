#include "GbSequence.hpp"

namespace File {
    GbSequence::GbSequence(std::array<std::uint8_t, 4>&& data,
                           const bool isComplete) : m_data {data},
                                                    m_currentLength
                                                            {1},
                                                    m_isComplete
                                                            {isComplete} { }

    std::optional<GbSequence> GbSequence::build(const Point byte) {
        if (byte == 0x80 || byte == 0xFF) {
            return std::nullopt;
        }
        return GbSequence {{byte, 0, 0, 0}, byte <= 0x7F};
    }

    bool GbSequence::isComplete() const {
        return m_isComplete;
    }

    bool GbSequence::addPoint(const Point point) {
        m_data.at(m_currentLength) = point;
        m_currentLength++;
        switch (m_currentLength) {
            case 2:
                if (0x81 <= m_data[0] && m_data[0] <= 0xFE) {
                    if ((0x40 <= point && point <= 0xFE) && point != 0x7F) {
                        m_isComplete = true;
                        return true;
                    }
                    return ((0x81 <= m_data[0] && m_data[0] <= 0x84) ||
                            (0x90 <= m_data[0] && m_data[0] <= 0xE3)) &&
                           (0x30 <= point && point <= 0x39);
                }
                return false;
            case 3:
                return 0x81 <= point && point <= 0xFE;
            case 4:
                m_isComplete = true;
                return 0x30 <= point && point <= 0x39;
            default:
                return false;
        }
    }

    bool GbSequence::isValid() const {
        if (m_isComplete && (m_currentLength == 1)) {
            return (0x08 <= m_data[0] && m_data[0] <= 0x0D) || (m_data[0] == 0x1B) |
                                                               (0x20 <= m_data[0] && m_data[0] <= 0x7E);
        }
        return m_isComplete;
    }
} // File
