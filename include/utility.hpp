#include <limits>
#include <string>
#include <cstdlib>
#include <utility>

#include "caf/maybe.hpp"
#include "caf/optional.hpp"
#include "caf/string_algorithms.hpp"

#include "config.hpp"

namespace {

class is_invalid_t {
public:
  constexpr is_invalid_t() {
    // nop
  }

  inline bool operator()(const caf::node_id& x) const {
    return x == caf::invalid_node_id;
  }

  inline bool operator()(const caf::actor& x) const {
    return x == caf::invalid_actor;
  }

  inline bool operator()(const caf::actor_addr& x) const {
    return x == caf::invalid_actor_addr;
  }

  template <class T>
  bool operator()(const caf::optional<T>& x) const {
    return ! x;
  }
};

constexpr is_invalid_t is_invalid = is_invalid_t{};

template <class Container, class F>
Container filter_not(Container xs, F f) {
  xs.erase(std::remove_if(xs.begin(), xs.end(), f), xs.end());
  return xs;
}

template <class F>
auto filter_not(F f) {
  return [f](const auto& xs) {
    return filter_not(xs, f);
  };
}

// like split, but returns a new vector instead of using an output argument
template <class... Ts>
std::vector<std::string> explode(const std::string& str, Ts&&... xs) {
  std::vector<std::string> result;
  caf::split(result, str, std::forward<Ts>(xs)...);
  return result;
}

template <class Container, class F>
auto map(const Container& xs, F f)
-> std::vector<decltype(f(std::declval<typename Container::value_type>()))> {
  using vtype = typename Container::value_type;
  std::vector<decltype(f(std::declval<vtype>()))> result;
  std::transform(xs.begin(), xs.end(), std::back_inserter(result), f);
  return result;
}

template <class F>
auto map(F f) {
  return [f](const auto& xs) {
    return map(xs, f);
  };
}

auto to_pair = [](const auto& xs) -> caf::optional<decltype(std::make_pair(xs.front(), xs.back()))> {
  if (xs.size() != 2)
    return caf::none;
  return std::make_pair(xs.front(), xs.back());
};

caf::optional<uint16_t> to_u16(const std::string& str) {
  char* e;
  auto res = strtoul(str.c_str(), &e, 10);
  if (e == (str.c_str() + str.size())
      && res <= std::numeric_limits<uint16_t>::max())
    return static_cast<uint16_t>(res);
  return caf::none;
}

auto flatten = [](const auto& xs) {
  std::vector<typename std::decay<decltype(*xs.front())>::type> result;
  for (auto& x : xs)
    if (x)
      result.push_back(*x);
  return result;
};

template <class Container, class T>
void append(Container& xs, T x) {
  xs.emplace_back(std::move(x));
}

template <class Container, class T>
void append(Container& xs, caf::maybe<T> x) {
  auto xptr = caf::get<T>(&x);
  if (xptr)
    xs.emplace_back(std::move(*xptr));
}

template <class T, class F>
void repeat(T until, F fun) {
  for (T i = 0; i < until; ++i)
    fun();
}

} // namespace <anonymous>

namespace vector_operators {

template <class T, class F>
auto operator|(std::vector<T> xs, F fun) -> decltype(fun(xs)) {
  return fun(std::move(xs));
}

} // vector_operators
