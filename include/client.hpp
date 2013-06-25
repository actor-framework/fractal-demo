#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <vector>
#include <cstdlib>

#include <QColor>

#include "cppa/cppa.hpp"

cppa::actor_ptr spawn_opencl_client(uint32_t);

class client : public cppa::event_based_actor {

 public:

    void init();

 private:

    cppa::actor_ptr m_current_server;
    std::vector<QColor> m_palette;

};

#endif // CLIENT_HPP
