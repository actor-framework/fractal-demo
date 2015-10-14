#ifndef FRACTAL_REQUEST_HPP
#define FRACTAL_REQUEST_HPP

#include <tuple>
#include <cstdint>

#include "config.hpp"

using fractal_request = std::tuple<std::uint32_t,    // 0: width,
                                   std::uint32_t,    // 1: height
                                   float,       // 2: min real
                                   float,       // 3: max real
                                   float,       // 4: min img
                                   float>;      // 5: max img

template <typename R>
auto width(R& req) -> decltype(std::get<0>(req)) {
  return std::get<0>(req);
}

template <typename R>
auto height(R& req) -> decltype(std::get<1>(req)) {
  return std::get<1>(req);
}

template <typename R>
auto min_re(R& req) -> decltype(std::get<2>(req)) {
  return std::get<2>(req);
}

template <typename R>
auto max_re(R& req) -> decltype(std::get<3>(req)) {
  return std::get<3>(req);
}

template <typename R>
auto min_im(R& req) -> decltype(std::get<4>(req)) {
  return std::get<4>(req);
}

template <typename R>
auto max_im(R& req) -> decltype(std::get<5>(req)) {
  return std::get<5>(req);
}

#endif // FRACTAL_REQUEST_HPP

