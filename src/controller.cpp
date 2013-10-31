#include <set>

#include "include/controller.hpp"

using namespace std;
using namespace cppa;

controller::controller(actor_ptr server)
: m_server(server)
, m_use_normal(0)
, m_use_opencl(0) { }

void controller::send_worker_config() {
    if (m_use_normal < m_normal.size() || m_use_opencl < m_opencl.size()) {
        cout << "[!!!] not enough workers known" << endl;
        return;
    }
    set<actor_ptr> new_workers;
    new_workers.insert(m_normal.begin(), m_normal.begin()+m_use_normal);
    new_workers.insert(m_opencl.begin(), m_opencl.begin()+m_use_opencl);
    send(m_server, atom("workers"), move(new_workers));
}

void controller::init(actor_ptr widget) {
    become(
        on(atom("add")) >> [=] {
            send(last_sender(), atom("identity"));
        },
        on(atom("opencl")) >> [=] {
            auto a = last_sender();
            link_to(a);
            m_opencl.push_back(a);
        },
        on(atom("normal")) >> [=] {
            auto a = last_sender();
            link_to(a);
            m_normal.push_back(a);
        },
        on(atom("resize"), arg_match) >> [=] (uint32_t, uint32_t) {
            forward_to(m_server);
        },
        on(atom("limit"), atom("opencl"), arg_match) >> [=] (uint32_t limit) {
            m_use_opencl = limit;
            send_worker_config();
        },
        on(atom("limit"), atom("normal"), arg_match) >> [=] (uint32_t limit) {
            m_use_normal = limit;
            send_worker_config();
        },
        on(atom("FPS"), arg_match) >> [=] (uint32_t) {
            forward_to(widget);
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
