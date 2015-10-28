/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_MAYBE_HPP
#define CAF_MAYBE_HPP

#include <new>
#include <utility>
#include <system_error>

#include "caf/none.hpp"
#include "caf/unit.hpp"
#include "caf/config.hpp"

#include "caf/detail/safe_equal.hpp"

namespace caf {

/// Represents a computation returning either `T` or a `std::error_condition`.
template <class T>
class maybe {
public:
  /// Typdef for `T`.
  using type = T;
  using error_type = std::error_condition;

  /// Creates an empty instance with `valid() == false`.
  maybe(error_type err) {
    cr_err(std::move(err));
  }

  /// Creates an valid instance from `value`.
  maybe(T value) {
    cr(std::move(value));
  }

  maybe(const maybe& other) : valid_(other.valid_) {
    if (other.valid_)
      cr(other.value_);
    else
      cr_err(other.error_);
  }

  maybe(maybe&& other) : valid_(other.valid_) {
    if (other.valid_)
      cr(std::move(other.value_));
    else
      cr_err(std::move(other.error_));
  }

  ~maybe() {
    destroy();
  }

  maybe& operator=(const maybe& other) {
    if (valid_ != other.valid_) {
      destroy();
      valid_ = other.valid_;
      if (valid_)
        cr(other.value_);
      else
        cr_err(other.error_);
    } else if (valid_) {
      value_ = other.value_;
    } else {
      error_ = other.error_;
    }
    return *this;
  }

  maybe& operator=(maybe&& other) {
    if (valid_ != other.valid_) {
      destroy();
      valid_ = other.valid_;
      if (valid_)
        cr(std::move(other.value_));
      else
        cr_err(std::move(other.error_));
    } else if (valid_) {
      value_ = std::move(other.value_);
    } else {
      error_ = std::move(other.error_);
    }
    return *this;
  }

  /// Queries whether this instance holds a value.
  bool valid() const {
    return valid_;
  }

  /// Returns `valid()`.
  explicit operator bool() const {
    return valid();
  }

  /// Returns `!valid()`.
  bool operator!() const {
    return ! valid();
  }

  /// Returns the value.
  T& operator*() {
    CAF_ASSERT(valid());
    return value_;
  }

  /// Returns the value.
  const T& operator*() const {
    CAF_ASSERT(valid());
    return value_;
  }

  /// Returns the value.
  const T* operator->() const {
    CAF_ASSERT(valid());
    return &value_;
  }

  /// Returns the value.
  T* operator->() {
    CAF_ASSERT(valid());
    return &value_;
  }

  /// Returns the error.
  const error_type& err() const {
    CAF_ASSERT(! valid());
    return error_;
  }

private:
  void destroy() {
    if (valid_)
      value_.~type();
    else
      error_.~error_type();
  }

  template <class V>
  void cr(V&& value) {
    valid_ = true;
    new (&value_) type(std::forward<V>(value));
  }

  void cr_err(std::error_condition ec) {
    valid_ = false;
    new (&error_) error_type(std::move(ec));
  }

  bool valid_;
  union {
    type value_;
    error_type error_;
  };
};

/// @relates maybe
template <class T, typename U>
bool operator==(const maybe<T>& lhs, const maybe<U>& rhs) {
  if ((lhs) && (rhs)) {
    return detail::safe_equal(*lhs, *rhs);
  }
  return ! lhs && ! rhs;
}

/// @relates maybe
template <class T, typename U>
bool operator==(const maybe<T>& lhs, const U& rhs) {
  return (lhs) ? *lhs == rhs : false;
}

/// @relates maybe
template <class T, typename U>
bool operator==(const T& lhs, const maybe<U>& rhs) {
  return rhs == lhs;
}

/// @relates maybe
template <class T, typename U>
bool operator!=(const maybe<T>& lhs, const maybe<U>& rhs) {
  return !(lhs == rhs);
}

/// @relates maybe
template <class T, typename U>
bool operator!=(const maybe<T>& lhs, const U& rhs) {
  return !(lhs == rhs);
}

/// @relates maybe
template <class T, typename U>
bool operator!=(const T& lhs, const maybe<U>& rhs) {
  return !(lhs == rhs);
}

/// @relates maybe
template <class T>
bool operator==(const maybe<T>& val, const std::error_condition& err) {
  return ! val.valid() && val.err() == err;
}

/// @relates maybe
template <class T>
bool operator==(const std::error_condition& err, const maybe<T>& val) {
  return val == err;
}

/// @relates maybe
template <class T>
bool operator!=(const maybe<T>& lhs, const std::error_condition& rhs) {
  return ! (lhs == rhs);
}

/// @relates maybe
template <class T>
bool operator!=(const std::error_condition& lhs, const maybe<T>& rhs) {
  return ! (lhs == rhs);
}

} // namespace caf

#endif // CAF_MAYBE_HPP
