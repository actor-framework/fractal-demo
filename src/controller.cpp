
#include <set>
#include <iterator>
#include <algorithm>

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
    copy_n(m_normal.begin(), m_use_normal, inserter(new_workers, new_workers.end()));
    copy_n(m_opencl.begin(), m_use_opencl, inserter(new_workers, new_workers.end()));
    send(m_server, atom("workers"), move(new_workers));
}

void controller::init(actor_ptr widget) {
    become(
        on(atom("add")) >> [=] {
            return atom("identity");
        },
        on(atom("opencl")) >> [=] {
            auto a = last_sender();
            link_to(a);
            m_opencl.insert(a);
            send(widget, atom("max_gpu"), m_opencl.size());
        },
        on(atom("normal")) >> [=] {
            auto a = last_sender();
            link_to(a);
            m_normal.insert(a);
            send(widget, atom("max_cpu"), m_normal.size());
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
        on(atom("EXIT"), arg_match) >> [=](std::uint32_t) {
            auto dead = last_sender();
            if (dead == widget)        { cout << "[!!!] GUI died" << endl; }
            else if (dead == m_server) { cout << "[!!!] server died" << endl; }
            else {
                auto n = m_normal.find(dead);
                if (n != m_normal.end()) {
                    m_normal.erase(n);
                    send(widget, atom("max_cpu"), m_normal.size());
                }
                else {
                    auto o = m_opencl.find(dead);
                    if (o != m_opencl.end()) {
                        m_opencl.erase(o);
                        send(widget, atom("max_gpu"), m_opencl.size());
                    }
                }
                send_worker_config();
            }
        },
        others() >> [=] {
            cout << "[!!!] controller received unexpected message: '"
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
