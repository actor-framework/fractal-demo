#include <map>
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

  template <class T>
  bool operator()(const caf::maybe<T>& x) const {
    return ! x;
  }

  template <class T>
  bool operator()(const caf::optional<T>& x) const {
    return ! x;
  }

  inline bool operator()(const caf::actor& x) const {
    return x == caf::invalid_actor;
  }

  inline bool operator()(const caf::node_id& x) const {
    return x == caf::invalid_node_id;
  }

  inline bool operator()(const caf::actor_addr& x) const {
    return x == caf::invalid_actor_addr;
  }
};

constexpr is_invalid_t is_invalid = is_invalid_t{};

} // namespace <anonymous>

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
auto map(const Container& xs, F f) {
  using vtype = typename Container::value_type;
  std::vector<decltype(f(std::declval<vtype&>()))> result;
  std::transform(xs.begin(), xs.end(), std::back_inserter(result), f);
  return result;
}

template <class F>
auto map(F f) {
  return [f](const auto& xs) {
    return map(xs, f);
  };
}

auto to_pair = [](const auto& xs)
-> caf::optional<decltype(std::make_pair(xs.front(), xs.back()))> {
  if (xs.size() != 2)
    return caf::none;
  return std::make_pair(xs.front(), xs.back());
};

template <class T>
caf::optional<T> to_uint(caf::optional<const std::string&> x) {
  if (! x)
    return caf::none;
  auto& str = *x;
  char* e;
  auto res = strtoull(str.c_str(), &e, 10);
  if (e == (str.c_str() + str.size())
      && res <= std::numeric_limits<T>::max())
    return static_cast<T>(res);
  return caf::none;
}

caf::optional<uint16_t> to_u16(caf::optional<const std::string&> x) {
  return to_uint<uint16_t>(x);
}

caf::optional<uint32_t> to_u32(caf::optional<const std::string&> x) {
  return to_uint<uint32_t>(x);
}

caf::optional<uint64_t> to_u64(caf::optional<const std::string&> x) {
  return to_uint<uint64_t>(x);
}

template <class T>
caf::optional<T> to_int(caf::optional<const std::string&> x) {
  if (! x)
    return caf::none;
  auto& str = *x;
  char* e;
  auto res = strtoll(str.c_str(), &e, 10);
  if (e == (str.c_str() + str.size())
      && res <= std::numeric_limits<T>::max()
      && res >= std::numeric_limits<T>::min())
    return static_cast<T>(res);
  return caf::none;
}

caf::optional<int16_t> to_i16(caf::optional<const std::string&> x) {
  return to_int<int16_t>(x);
}

caf::optional<int32_t> to_i32(caf::optional<const std::string&> x) {
  return to_int<int32_t>(x);
}

caf::optional<int64_t> to_i64(caf::optional<const std::string&> x) {
  return to_int<int64_t>(x);
}

struct flatten_t {
  template <class T>
  auto operator()(std::vector<caf::optional<T>> xs) const {
    std::vector<T> result;
    for (auto& x : xs)
      if (x)
        result.push_back(std::move(*x));
    return result;
  };
  template <class T>
  auto operator()(std::vector<std::vector<T>> xs) const {
    std::vector<T> result;
    for (auto& x : xs)
      for (auto& e : x)
        result.push_back(std::move(e));
    return result;
  };
};

constexpr flatten_t flatten = flatten_t{};

template <class Container, class T>
void append(Container& xs, T x) {
  xs.emplace_back(std::move(x));
}

template <class Container, class T>
void append(Container& xs, caf::optional<T> x) {
  if (x)
    xs.emplace_back(std::move(*x));
}

template <class Container, class T>
void append(Container& xs, caf::maybe<T> x) {
  if (x)
    xs.emplace_back(std::move(*x));
}

template <class T, class F>
void repeat(T until, F fun) {
  for (T i = 0; i < until; ++i)
    fun();
}

template <size_t X, class T>
auto oget(caf::optional<T>& x) -> caf::optional<decltype(std::get<X>(*x))> {
  if (x)
    return std::get<X>(*x);
  return caf::none;
}

template <size_t X, class T>
auto oget(const caf::optional<T>& x)
-> caf::optional<decltype(std::get<X>(*x))> {
  if (x)
    return std::get<X>(*x);
  return caf::none;
}

auto ohead = [](const auto& xs) -> caf::optional<decltype(xs.front())> {
  if (! xs.empty())
    return xs.front();
  return caf::none;
};

struct eom_t { } eom; // end-of-main marker in case of an error

int operator<<(std::ostream& out, const eom_t&) {
  out << std::endl;
  return -1;
}

template <class T>
struct optify {
  using type = caf::optional<T>;
};

template <class T>
struct optify<caf::optional<T>> {
  using type = caf::optional<T>;
};

template <class T>
using optify_t = typename optify<T>::type;

bool any_none() {
  return false;
}

template <class T, class... Ts>
bool any_none(const T& x, const Ts&... xs) {
  if (! x)
    return true;
  return any_none(xs...);
}

template <class F,
          class Sig = typename caf::detail::get_callable_trait<F>::fun_type>
class lift_t;

template <class F, class R, class... Ts>
class lift_t<F, std::function<R (Ts...)>> {
public:
  lift_t(F f) : f_(std::move(f)) {
    // nop
  }

  optify_t<R> operator()(optify_t<Ts>... xs) const {
    if (any_none(xs...))
      return caf::none;
    try {
      return f_(*xs...);
    } catch (...) {
      return caf::none;
    }
  }

private:
  F f_;
};

template <class F>
struct is_lifted : std::false_type { };

template <class F, class Sig>
struct is_lifted<lift_t<F, Sig>> : std::true_type { };

template <class F, class Enable = std::enable_if_t<! is_lifted<F>::value>>
lift_t<F> lift(F f) {
  return {f};
}

template <class F, class Enable = std::enable_if_t<is_lifted<F>::value>>
F lift(F f) {
  return f;
}

namespace container_operators {

template <class T, class F>
auto operator|(std::vector<T> xs, F fun) -> decltype(fun(xs)) {
  return fun(std::move(xs));
}

template <class K, class V, class F>
auto operator|(std::map<K, V> xs, F fun) -> decltype(fun(xs)) {
  return fun(std::move(xs));
}

} // namespace vector_operators
