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

#include <system_error>

namespace caf {

/// CAF Error Code.
enum class cec {
  remote_node_unreachable = 1,
  expected_dynamically_typed_remote_actor,
  expected_statically_typed_remote_actor,
  incompatible_statically_typed_remote_actor,
  unknown_error
};

class cec_category_impl : public std::error_category {
public:
  const char* name() const noexcept override;
  std::string message(int ev) const override;
};

const std::error_category& cec_category();

} // namespace caf

namespace std {

template <>
struct is_error_condition_enum<caf::cec> : true_type { };

inline error_condition make_error_condition(caf::cec code) {
  return error_condition(static_cast<int>(code), caf::cec_category());
}

} // namespace std
