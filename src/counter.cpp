
#include <chrono>
#include <algorithm>

#include <QByteArray>

#include "cppa/cppa.hpp"

#include "include/counter.hpp"

using namespace std;
using namespace cppa;
using namespace chrono;


counter::counter() :
 m_next(0), // image id, should start with 0
 m_delay(20), // delay between images, is adjusted later on
 m_buffer_limit(300), // images buffers, overhead is discarded
 m_idx(0),
 m_last(steady_clock::now()),
 m_intervals(100) // number of values used to calculate mean fps
{
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

void counter::init(actor_ptr widget, actor_ptr ctrl) {
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
            if (m_buffer.size() > m_buffer_limit) {
                aout << "[!!!] buffer is too full, dropping images" << endl;
                m_dropped.insert(id);
            }
            else {
                m_buffer.insert(make_pair(id, ba));
            }
            steady_clock::time_point now = steady_clock::now();
            m_intervals[m_idx] = duration_cast<milliseconds>(now - m_last);
            m_idx = (m_idx + 1) % m_intervals.size();
            auto total = accumulate(begin(m_intervals), end(m_intervals), milliseconds(0));
            m_delay = total.count() / m_intervals.size();
            m_last = now;
            aout << "adjusted delay to: " << m_delay << endl;
            send(ctrl, atom("fps"), m_delay);
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
        on(atom("init"), arg_match) >> [=] (actor_ptr widget, actor_ptr ctrl) {
            delayed_send(self, chrono::milliseconds(m_delay), atom("tick"));
            init(widget, ctrl);
        }
    );
}

