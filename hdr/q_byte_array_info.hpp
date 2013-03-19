#ifndef QBYTE_ARRAY_INFO_HPP
#define QBYTE_ARRAY_INFO_HPP

#include <QByteArray>
#include "cppa/cppa.hpp"

class q_byte_array_info :
        public cppa::util::abstract_uniform_type_info<QByteArray> {
 protected:
    void serialize(const void* ptr, cppa::serializer* sink) const;
    void deserialize(void* ptr, cppa::deserializer* source) const;
};

#endif
