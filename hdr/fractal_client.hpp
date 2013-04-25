#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <vector>
#include <cstdlib>

#include <QColor>

#include "cppa/cppa.hpp"

#include "cppa/opencl/global.hpp"
#include "cppa/opencl/program.hpp"
#include "cppa/opencl/actor_facade.hpp"
#include "cppa/opencl/command_dispatcher.hpp"

#include "fractal_cppa.hpp"

class client : public cppa::event_based_actor {

 public:

    client(cppa::actor_ptr server,
           uint32_t client_id,
           bool with_opencl,
           cppa::opencl::program& prog);

    void init();

 private:

    cppa::actor_ptr m_server;
    uint32_t m_client_id;
    std::string m_prefix;

    uint32_t m_iterations;

    std::vector<QColor> m_palette;

    bool m_with_opencl;
    cppa::opencl::program m_program;
    cppa::actor_ptr m_fractal;
    unsigned m_current_id;
    unsigned m_current_width;
    unsigned m_current_height;

};

#endif // CLIENT_HPP
