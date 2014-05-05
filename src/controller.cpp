
#include <set>
#include <iterator>
#include <algorithm>

#include "include/controller.hpp"

using namespace std;
using namespace cppa;

controller::controller(actor server)
: m_server(server)
, m_use_normal(0)
, m_use_opencl(0) { }

void controller::send_worker_config() {
    if (m_use_normal > m_normal.size() || m_use_opencl > m_opencl.size()) {
        aout(this) << "[!!!] only "  << m_normal.size()
                   << " normal and " << m_opencl.size()
                   << " workers known" << endl;
        return;
    }
    set<actor> new_workers;
    copy_n(m_normal.begin(), m_use_normal, inserter(new_workers, new_workers.end()));
    copy_n(m_opencl.begin(), m_use_opencl, inserter(new_workers, new_workers.end()));
    send(m_server, atom("workers"), move(new_workers));
}

behavior controller::make_behavior() {
    trap_exit(true);
    return {
        on(atom("widget"), arg_match) >> [=] (const actor& widget){
            m_widget = widget;

            send(m_widget, atom("max_cpu"), m_normal.size());
            send(m_widget, atom("max_gpu"), m_opencl.size());
        },
        on(atom("add")) >> [=] {
            return atom("identity");
        },
        on(atom("normal"), arg_match) >> [=] (const actor& client){
//            cout << "normal actor connected" << endl;
            auto a = client;
            link_to(a);
            m_normal.insert(a);
            send(m_widget, atom("max_cpu"), m_normal.size());
        },
        on(atom("opencl"), arg_match) >> [=] (const actor& worker){
//            cout << "opencl actor connected" << endl;
            auto a = worker;
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
        on(atom("fps"), arg_match) >> [=] (uint32_t) {
            forward_to(m_widget);
        },

        on(atom("EXIT"), arg_match) >> [=](const exit_msg& msg) {
            auto dead_addr = msg.source;

            if(dead_addr == m_widget)
            {
                aout(this) << "[!!!] GUI died" << endl;
            }else if(dead_addr == m_server) {
                aout(this) << "[!!!] server died" << endl;
            }
            else {
                auto n = find_if(m_normal.begin(), m_normal.end(), [&](const actor& element) {
                    return element == dead_addr;
                });
                if (n != m_normal.end()) {
                    m_normal.erase(n);
                    send(m_widget, atom("max_cpu"), m_normal.size());
                }
                else {
                    auto o = find_if(m_opencl.begin(), m_opencl.end(), [&](const actor& element) {
                        return element == dead_addr;
                    });
                    if (o != m_opencl.end()) {
                        m_opencl.erase(o);
                        send(m_widget, atom("max_gpu"), m_opencl.size());
                    }
                }
                send_worker_config();
            }
        },
        on(atom("quit")) >> [=] {
            quit();
        },
        others() >> [=] {
            aout(this) << "[!!!] controller received unexpected message: '"
                 << to_string(last_dequeued())
                 << "'." << endl;
        }
    };
}
