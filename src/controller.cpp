
#include "include/controller.hpp"

using namespace std;
using namespace cppa;

controller::controller(actor_ptr server) : m_server(server) { }

void controller::init(actor_ptr widget) {
    become(
        on(atom("add")) >> [=] {
            send(last_sender(), atom("opencl"));
//            sync_send(last_sender(), atom("opencl")).then(
//                on_arg_match >>[=] (bool is_opencl_enabled) {
//                    if(is_opencl_enabled) {
//                        m_opencl.push_back(last_sender());
//                    }
//                    else {
//                        m_normal.push_back(last_sender());
//                    }
//                }
//            );
        },
        on(atom("opencl"), arg_match) >> [=] (bool is_opencl_enabled) {
            if(is_opencl_enabled) {
                m_opencl.push_back(last_sender());
            }
            else {
                m_normal.push_back(last_sender());
            }
        },
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
