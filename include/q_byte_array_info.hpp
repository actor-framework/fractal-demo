#ifndef QBYTE_ARRAY_INFO_HPP
#define QBYTE_ARRAY_INFO_HPP

#include <QByteArray>
#include "cppa/cppa.hpp"

class q_byte_array_info : public cppa::util::abstract_uniform_type_info<QByteArray> {

 protected:

    virtual void serialize(const void* ptr, cppa::serializer* sink) const override;
    virtual void deserialize(void* ptr, cppa::deserializer* source) const override;

};

#endif
