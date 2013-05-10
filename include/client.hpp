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

cppa::actor_ptr spawn_opencl_client();

class client : public cppa::event_based_actor {

 public:

    void init();

 private:

    std::vector<QColor> m_palette;

};

#endif // CLIENT_HPP
