#ifndef LONG_DOUBLE_INFO_HPP
#define LONG_DOUBLE_INFO_HPP

#include "cppa/cppa.hpp"

class long_double_info :
        public cppa::util::abstract_uniform_type_info<long double> {
 protected:
    void serialize(const void* ptr, cppa::serializer* sink) const;
    void deserialize(void* ptr, cppa::deserializer* source) const;
};

#endif // LONG_DOUBLE_INFO_HPP
