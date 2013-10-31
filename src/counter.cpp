
#include <chrono>
#include <algorithm>

#include <QByteArray>

#include "cppa/cppa.hpp"

#include "include/counter.hpp"

using namespace std;
using namespace cppa;


counter::counter() : m_next(0), m_delay(300) { }

bool counter::probe() {
    auto i = m_dropped.find(m_next);
    if (i == m_dropped.end()) {
        return false;
    }
    else {
        m_dropped.erase(i);
        ++m_next;
        return true;
    }
}

void counter::init(actor_ptr widget) {
    become(
        on(atom("tick")) >> [=] {
            while (probe());
            auto j = m_buffer.find(m_next);
            if (j != m_buffer.end()) {
                send(widget, move(j->second)); // todo is this valid?
                m_buffer.erase(j);
            }
            delayed_send(self, chrono::milliseconds(m_delay), atom("tick"));
        },
        on(atom("image"), arg_match) >> [=] (uint32_t id,
                                             const QByteArray& ba) {
            m_buffer.insert(make_pair(id, ba));
            // todo track ips, adjust framerate
        },
        on(atom("dropped"), arg_match) >> [=] (uint32_t id) {
            if (id > m_next) { m_dropped.insert(id); }
            else if (id == m_next) { ++m_next; }
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
        on(atom("init"), arg_match) >> [=] (actor_ptr widget) {
            delayed_send(self, chrono::milliseconds(m_delay), atom("tick"));
            init(widget);
        }
    );
}

