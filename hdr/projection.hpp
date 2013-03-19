#ifndef PROJECTION_HPP
#define PROJECTION_HPP

#include <string>
#include <type_traits>
#include "cppa/option.hpp"

template<typename T>
cppa::option<T> projection(const std::string& arg,
                           typename std::enable_if
                           <std::is_integral<T>::value>::type* = 0) {
    char* endptr = nullptr;
    int result = static_cast<T>(strtoll(arg.c_str(), &endptr, 10));
    if (endptr != nullptr && *endptr == '\0') {
        return result;
    }
    return {};
}

template<typename T>
cppa::option<T> projection(const std::string& arg,
                           typename std::enable_if
                           <std::is_floating_point<T>::value>::type* = 0) {
    char* endptr = nullptr;
    int result = static_cast<T>(strtold(arg.c_str(), &endptr));
    if (endptr != nullptr && *endptr == '\0') {
        return result;
    }
    return {};
}

#endif // PROJECTION_HPP
