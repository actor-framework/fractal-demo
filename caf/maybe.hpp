#include "caf/variant.hpp"

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

template <class T>
using maybe = caf::variant<T, std::error_condition>;

} // namespace caf

namespace std {

template <>
struct is_error_condition_enum<caf::cec> : true_type { };

inline error_condition make_error_condition(caf::cec code) {
  return error_condition(static_cast<int>(code), caf::cec_category());
}

} // namespace std

