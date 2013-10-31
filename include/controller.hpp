#ifndef CONTROLLER_HPP
#define CONTROLLER_HPP

#include <set>

#include "cppa/cppa.hpp"

class controller : public cppa::event_based_actor {

 public:

    controller(cppa::actor_ptr server);
    virtual void init() override;

 private:

    void init(cppa::actor_ptr widget);
    void send_worker_config();

    cppa::actor_ptr m_server;

    std::uint32_t m_use_normal;
    std::uint32_t m_use_opencl;

    std::set<cppa::actor_ptr> m_opencl;
    std::set<cppa::actor_ptr> m_normal;
};

#endif // CONTROLLER_HPP
