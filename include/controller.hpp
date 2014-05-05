#ifndef CONTROLLER_HPP
#define CONTROLLER_HPP

#include <set>

#include "cppa/cppa.hpp"

class controller : public cppa::event_based_actor {

 public:

    controller(cppa::actor server);
    cppa::behavior make_behavior() override;

 private:

    void send_worker_config();

    cppa::actor m_server;
    cppa::actor m_widget;

    std::uint32_t m_use_normal;
    std::uint32_t m_use_opencl;

    std::set<cppa::actor> m_opencl;
    std::set<cppa::actor> m_normal;
};

#endif // CONTROLLER_HPP
