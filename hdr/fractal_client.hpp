#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <vector>
#include <cstdlib>

#include <QColor>

#include "cppa/cppa.hpp"

#include "fractal_cppa.hpp"

class client : public cppa::event_based_actor {

    cppa::actor_ptr m_server;
    cppa::actor_ptr m_printer;
    uint32_t m_client_id;
    std::string m_prefix;

    uint32_t m_iterations;

    std::vector<QColor> m_palette;
    void init();

 public:

    client(cppa::actor_ptr printer, cppa::actor_ptr server, uint32_t client_id);

};

#endif // CLIENT_HPP
