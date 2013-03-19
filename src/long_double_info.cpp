#include "long_double_info.hpp"

using namespace std;
using namespace cppa;

void long_double_info::serialize(const void* ptr,
                                 cppa::serializer* sink) const {
    sink->begin_object(name());
//    auto ld_ptr = reinterpret_cast<const long double*>(ptr);
    auto ld_size = static_cast<uint32_t>(sizeof(long double));
    sink->write_value(ld_size);
    sink->write_raw(ld_size, ptr);
    sink->end_object();
}

void long_double_info::deserialize(void* ptr,
                                   cppa::deserializer* source) const {
    string cname = source->seek_object();
    if (cname != name()) {
        throw logic_error("wrong type name found");
    }
    source->begin_object(cname);
    auto ld_ptr = reinterpret_cast<long double*>(ptr);
    auto value = get<std::uint32_t>(source->read_value(pt_uint32));
    source->read_raw(value, ld_ptr);
    source->end_object();
}
