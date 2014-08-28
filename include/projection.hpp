#ifndef PROJECTION_HPP
#define PROJECTION_HPP

#include <string>
#include <type_traits>
#include "caf/optional.hpp"

template<typename T>
caf::optional<T> projection(const std::string& arg) {
    char* endptr = nullptr;
    int result = std::is_integral<T>::value
               ? static_cast<T>(strtoll(arg.c_str(), &endptr, 10))
               : static_cast<T>(strtold(arg.c_str(), &endptr));
    if (endptr != nullptr && *endptr == '\0') {
        return result;
    }
    return {};
}

#endif // PROJECTION_HPP

