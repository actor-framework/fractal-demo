


#include "include/controller.hpp"

controller::controller() { }

void controller::init(actor_ptr widget) {
    become(
        others() >> [=] {
            cout << "[!!!] counter received unexpected message: '"
                 << to_string(last_dequeued())
                 << "'." << endl;
        }
    );
}

void controller::init() {
    trap_exit(true);
    become (
        on(atom("init"), arg_match) >> [=] (actor_ptr widget) {
            init(widget);
        }
    );
}
