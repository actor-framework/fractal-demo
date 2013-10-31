#ifndef COUNTER_HPP
#define COUNTER_HPP

#include "cppa/cppa.hpp"

class counter : public cppa::event_based_actor {

 public:

    counter();
    virtual void init() override;

 private:

    void init(cppa::actor_ptr gui);

};

#endif // COUNTER_HPP
