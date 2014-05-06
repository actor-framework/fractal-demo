#include <map>
#include <tuple>
#include <chrono>
#include <time.h>
#include <vector>
#include <limits>
#include <complex>
#include <cstdlib>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <functional>

#include "cppa/opt.hpp"
#include "cppa/cppa.hpp"

#include "ui_main.h"
#include "server.hpp"
#include "config_map.hpp"
#include "mainwidget.hpp"
#include "q_byte_array_info.hpp"

using namespace std;
using namespace cppa;

void server::send_next_job(const actor& worker) {
    if (!worker) {
        cerr << "Invalid actor (send_next_job())" << endl;
    }
    if (!worker || not m_stream.next()) return;
    auto& fr = m_stream.request();

    auto next_id = m_next_id++;
    send(worker,
         atom("assign"),
         width(fr),
         height(fr),
         m_iterations,
         next_id,
         min_re(fr),
         max_re(fr),
         min_im(fr),
         max_im(fr));
    m_jobs.insert(make_pair(worker, next_id));
}

behavior server::make_behavior() {
    trap_exit(true);
    return {
        on(atom("workers"), arg_match) >> [=] (std::set<actor> workers) {
            set<actor> new_workers;
            set_difference(begin(workers),   end(workers),
                           begin(m_workers), end(m_workers),
                           inserter(new_workers, end(new_workers)));
            for (auto& w : new_workers) {
                send_next_job(w);
            }
            m_workers.swap(workers);
        },
        on(atom("quit")) >> [=] {
            quit();
        },
        on(atom("result"), arg_match) >> [=](const actor& worker, uint32_t id, const QByteArray& ba) {
            send(m_counter, atom("image"), id, ba);
            if (m_stream.at_end()) {
                // provides endless stream
                m_stream.loop_stack();
            }

            auto w = m_workers.find(worker);
            if (w != m_workers.end()) {
                send_next_job(worker);
                aout(this) << "[server] worker found" << endl;
            }
            else {
                aout(this) << "[server] worker not found" << endl;
                aout(this) << to_string(*m_workers.begin())
                           << " != "
                           << to_string(worker)
                           << " (should be equal)"
                           << endl;
                aout(this) << "last_sender: "
                           << to_string(last_sender())
                           << endl;
            }
            auto j = m_jobs.find(worker);
            if (j != m_jobs.end()) {
                m_jobs.erase(j);
                aout(this) << "[server] job found" << endl;
            }
        },
        on(atom("resize"), arg_match) >> [=](uint32_t new_width,
                                             uint32_t new_height) {
            m_stream.resize(new_width, new_height);
        },

        on(atom("EXIT"), arg_match) >> [=](const exit_msg& msg) {
            cout << "[!!!] someone disconnected" << endl;
            auto sender_adress = msg.source;
            auto a = find_if(m_jobs.begin(), m_jobs.end(), [sender_adress](const pair<actor,uint32_t>& element) {
                return element.first == sender_adress;
            });
            if(a != m_jobs.end()) {
                send(m_counter, atom("dropped"), a->second);
                m_jobs.erase(a->first);
            }
        },
        others() >> [=] {
            aout(this) << "[!!!] server received unexpected message: '"
                       << to_string(last_dequeued())
                       << "'." << endl;
        }
    };
}

struct ini_helper {
    config_map& ini;
    ini_helper(config_map& cmap) : ini(cmap) { }
    template<typename T>
    T operator()(const char* field, const T& default_value) {
        return ini.get_or_else("fractals", field, default_value);
    }
};

server::server(config_map& ini, cppa::actor counter)
: m_next_id(0)
, m_counter(counter) {
    ini_helper rd{ini};
    auto width      = rd("width",      default_width);
    auto height     = rd("height",     default_height);
    m_stream.init(width,
                  height,
                  rd("min_real",   default_min_real),
                  rd("max_real",   default_max_real),
                  rd("min_imag",   default_min_imag),
                  rd("max_imag",   default_max_imag),
                  rd("zoom",       default_zoom));
    m_interval   = rd("interval",   default_interval);
    m_queuesize  = rd("queuesize",  default_queuesize);
    m_iterations = rd("iterations", default_iterations);
    // make sure ini contains width and height
    if (width == default_width) ini.set("fractals", "width", to_string(width));
    if (height == default_height) ini.set("fractals", "height", to_string(height));
}
