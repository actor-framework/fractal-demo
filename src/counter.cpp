
#include "cppa/cppa.hpp"

#include "include/counter.hpp"

#include "q_byte_array_info.hpp"

using namespace std;
using namespace cppa;


counter::counter() { }

void counter::init(actor_ptr gui) {
    become(
        on(atom("image"), arg_match) >> [=] (uint32_t,
                                             const QByteArray&) {
            // todo save in buffer, track ips, adjust framerate
        },
        on(atom("dropped"), arg_match) >> [=] (uint32_t) {
            // todo find next available image
        },
        others() >> [=] {
            cout << "[!!!] counter received unexpected message: '"
                 << to_string(last_dequeued())
                 << "'." << endl;
        }
    );
}

void counter::init() {
    trap_exit(true);
    become (
        on(atom("init"), arg_match) >> [=] (actor_ptr gui) {
            init(gui);
        }

    );
}

