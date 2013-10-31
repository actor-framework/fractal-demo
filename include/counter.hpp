#ifndef COUNTER_HPP
#define COUNTER_HPP

#include <set>
#include <vector>

#include "cppa/cppa.hpp"

class counter : public cppa::event_based_actor {

 public:

    counter();
    virtual void init() override;

 private:

    bool probe();
    void init(cppa::actor_ptr widget);

    std::uint32_t m_next;
    std::uint32_t m_delay; // in msec

    std::set<std::uint32_t> m_dropped;
    std::map<std::uint32_t, QByteArray> m_buffer;

};

#endif // COUNTER_HPP
