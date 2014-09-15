#ifndef QBYTE_ARRAY_INFO_HPP
#define QBYTE_ARRAY_INFO_HPP

#include <QByteArray>
#include "caf/all.hpp"
#include "caf/detail/abstract_uniform_type_info.hpp"

class q_byte_array_info
    : public caf::detail::abstract_uniform_type_info<QByteArray> {
 protected:
  virtual void serialize(const void* ptr, caf::serializer* sink) const override;
  virtual void deserialize(void* ptr, caf::deserializer* source) const override;
};

#endif

