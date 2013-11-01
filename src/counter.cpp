
#include <chrono>
#include <algorithm>

#include <QByteArray>

#include "cppa/cppa.hpp"

#include "include/counter.hpp"

using namespace std;
using namespace cppa;
using namespace chrono;


counter::counter() :
 m_next(0),
 m_delay(20),
 m_idx(0),
 m_last(steady_clock::now()),
 m_intervals(100) {
    fill(begin(m_intervals), end(m_intervals), std::chrono::milliseconds(20));
}

bool counter::probe() {
    auto i = m_dropped.find(m_next);
    if (i == m_dropped.end()) {
        return false;
    }
    else {
        aout << "dropped: " << m_next << endl;
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
                //aout << "found: " << m_next << endl;
                send(widget, move(j->second)); // todo is this valid?
                m_buffer.erase(j);
                ++m_next;
            }
            delayed_send(self, chrono::milliseconds(m_delay), atom("tick"));
        },
        on(atom("image"), arg_match) >> [=] (uint32_t id,
                                             const QByteArray& ba) {
            m_buffer.insert(make_pair(id, ba));
            steady_clock::time_point now = steady_clock::now();
            m_intervals[m_idx] = duration_cast<milliseconds>(now - m_last);
            m_idx = (m_idx + 1) % m_intervals.size();
            auto total = accumulate(begin(m_intervals), end(m_intervals), milliseconds(0));
            m_delay = total.count() / 100;
            m_last = now;
            aout << "adjust framerate to: " << 1000/m_delay << endl;
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

