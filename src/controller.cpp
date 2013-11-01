
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
    if (m_use_normal > m_normal.size() || m_use_opencl > m_opencl.size()) {
        cout << "[!!!] only "  << m_normal.size()
             << " normal and " << m_opencl.size()
             << " workers known" << endl;
        return;
    }
    set<actor_ptr> new_workers;
    copy_n(m_normal.begin(), m_use_normal, inserter(new_workers, new_workers.end()));
    copy_n(m_opencl.begin(), m_use_opencl, inserter(new_workers, new_workers.end()));
    send(m_server, atom("workers"), move(new_workers));
}

void controller::init() {
    trap_exit(true);
    become(
        on(atom("widget")) >> [=] {
            m_widget = last_sender();
            send(m_widget, atom("max_cpu"), m_normal.size());
            send(m_widget, atom("max_gpu"), m_opencl.size());
        },
        on(atom("add")) >> [=] {
            return atom("identity");
        },
        on(atom("normal")) >> [=] {
//            cout << "normal actor connected" << endl;
            auto a = last_sender();
            link_to(a);
            m_normal.insert(a);
            send(m_widget, atom("max_cpu"), m_normal.size());
        },
        on(atom("opencl")) >> [=] {
//            cout << "opencl actor connected" << endl;
            auto a = last_sender();
            link_to(a);
            m_opencl.insert(a);
            send(m_widget, atom("max_gpu"), m_opencl.size());
        },
        on(atom("resize"), arg_match) >> [=] (uint32_t, uint32_t) {
            forward_to(m_server);
        },
        on(atom("limit"), atom("normal"), arg_match) >> [=] (uint32_t limit) {
            m_use_normal = limit;
            send_worker_config();
        },
        on(atom("limit"), atom("opencl"), arg_match) >> [=] (uint32_t limit) {
            m_use_opencl = limit;
            send_worker_config();
        },
        on(atom("FPS"), arg_match) >> [=] (uint32_t) {
            forward_to(m_widget);
        },
        on(atom("EXIT"), arg_match) >> [=](std::uint32_t) {
            auto dead = last_sender();
            if (dead == m_widget)        { cout << "[!!!] GUI died" << endl; }
            else if (dead == m_server) { cout << "[!!!] server died" << endl; }
            else {
                auto n = m_normal.find(dead);
                if (n != m_normal.end()) {
                    m_normal.erase(n);
                    send(m_widget, atom("max_cpu"), m_normal.size());
                }
                else {
                    auto o = m_opencl.find(dead);
                    if (o != m_opencl.end()) {
                        m_opencl.erase(o);
                        send(m_widget, atom("max_gpu"), m_opencl.size());
                    }
                }
                send_worker_config();
            }
        },
        on(atom("quit")) >> [=] {
            self->quit();
        },
        others() >> [=] {
            cout << "[!!!] controller received unexpected message: '"
                 << to_string(last_dequeued())
                 << "'." << endl;
        }
    );
}
