#ifndef CONTROLLER_HPP
#define CONTROLLER_HPP

#include "cppa/cppa.hpp"

class controller : public cppa::event_based_actor {

 public:

    controller();

    void init();
};

#endif // CONTROLLER_HPP
