#ifndef CONTROLLER_HPP
#define CONTROLLER_HPP

#include <vector>

#include "cppa/cppa.hpp"

class controller : public cppa::event_based_actor {

 public:

    controller(cppa::actor_ptr server);
    virtual void init() override;

 private:

    void init(cppa::actor_ptr widget);

    cppa::actor_ptr m_server;

    std::vector<cppa::actor_ptr> m_opencl;
    std::vector<cppa::actor_ptr> m_normal;
};

#endif // CONTROLLER_HPP
