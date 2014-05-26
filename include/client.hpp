#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <vector>
#include <cstdlib>

#include <QColor>

#include "cppa/cppa.hpp"

cppa::actor spawn_opencl_client(uint32_t);

class client : public cppa::event_based_actor {

 public:

    cppa::behavior make_behavior() override;

 private:

    cppa::actor         m_current_server;
    cppa::atom_value    m_current_fractal_type;
    std::vector<QColor> m_palette;

};

#endif // CLIENT_HPP
