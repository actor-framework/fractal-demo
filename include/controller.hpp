#ifndef CONTROLLER_HPP
#define CONTROLLER_HPP

#include "cppa/cppa.hpp"

class controller : public cppa::event_based_actor {

 public:

    controller();
    virtual void init() override;

 private:

    void init(cppa::actor_ptr widget);

};

#endif // CONTROLLER_HPP
