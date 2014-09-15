#include "q_byte_array_info.hpp"

using namespace std;
using namespace caf;

void q_byte_array_info::serialize(const void* ptr,
                                  caf::serializer* sink) const {
  auto ba_ptr = reinterpret_cast<const QByteArray*>(ptr);
  auto ba_size = static_cast<uint32_t>(ba_ptr->size());
  sink->write_value(ba_size);
  sink->write_raw(ba_ptr->size(), ba_ptr->constData());
}

void q_byte_array_info::deserialize(void* ptr,
                                    caf::deserializer* source) const {
  auto ba_ptr = reinterpret_cast<QByteArray*>(ptr);
  auto value = source->read<uint32_t>();
  ba_ptr->resize(value);
  source->read_raw(value, ba_ptr->data());
}
