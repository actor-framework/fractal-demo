#include <system_error>

#include "caf/maybe.hpp"

namespace caf {

const char* cec_category_impl::name() const noexcept {
  return "CAF";
}

std::string cec_category_impl::message(int) const {
  return "--unknown-error--";
}

const std::error_category& cec_category() {
  static cec_category_impl impl;
  return impl;
}

} // namespace caf
