#ifndef VLE_HPP
#define VLE_HPP
#include <concepts>
#include <optional>

namespace File {
    template<class Encoding, typename Point>
    concept Vle = requires(Encoding e, Point p)
    {
        { Encoding::build(p) } -> std::convertible_to<std::optional<Encoding> >;
        { e.isComplete() } -> std::convertible_to<bool>;
        { e.addPoint(p) } -> std::convertible_to<bool>;
        { e.isValid() } -> std::convertible_to<bool>;
    };
}

#endif //VLE_HPP
