#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <vector>
#include <cstdlib>

#include <QColor>

#include "cppa/cppa.hpp"

cppa::actor_ptr spawn_opencl_client();

class client : public cppa::event_based_actor {

 public:

    void init();

 private:

    std::vector<QColor> m_palette;

};

#endif // CLIENT_HPP
