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
#include <functional>

#include "cppa/opt.hpp"
#include "cppa/cppa.hpp"

#include "ui_main.h"
#include "server.hpp"
#include "config_map.hpp"
#include "mainwidget.hpp"
#include "q_byte_array_info.hpp"

using namespace cppa;
using namespace std;

void server::send_next_job(const actor_ptr& worker, bool is_opencl_enabled) {
    if (worker == nullptr) {
        cerr << "send_next_job(nullptr) called" << endl;
    }
    if (worker == nullptr || not m_stream.next()) return;
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
    //m_current_jobs.insert(make_pair(worker, next_id));
    if (is_opencl_enabled) { ++m_cur_opencl; }
    else                   { ++m_cur_normal; }
    cout << "working actors:" << endl
         << "normal "   << m_cur_normal << "/ " << m_max_normal
         << "\nopencl " << m_cur_opencl << "/ " << m_max_opencl
         << endl;
}

void server::init(actor_ptr image_receiver) {
    become (
        on(atom("newWorker"), arg_match) >> [=] (bool is_opencl_enabled) {
            auto w = last_sender();
            if (w) {
                link_to(w);
                if (is_opencl_enabled) {
                    ++m_max_opencl;
                    m_opencl_actor_buffer.push_back(last_sender());
                }
                else {
                    ++m_max_normal;
                    m_normal_actor_buffer.push_back(last_sender());
                }
                //send_next_job(w, is_opencl_enabled);
                send(self, atom("distribute"));
            }
        },
        on(atom("distribute")) >> [=] {
            while(!m_normal_actor_buffer.empty() &&
                   m_cur_normal < m_lim_normal) {
                send_next_job(m_normal_actor_buffer.back(), false);
                m_normal_actor_buffer.pop_back();
            }
            while(!m_opencl_actor_buffer.empty() &&
                   m_cur_opencl < m_lim_opencl) {
                send_next_job(m_opencl_actor_buffer.back(), true);
                m_opencl_actor_buffer.pop_back();
            }
        },
        on(atom("limWorkers"), arg_match) >> [=] (uint32_t limit, bool is_opencl) {
            if (is_opencl) { m_lim_opencl = limit; }
            else           { m_lim_normal = limit; }
            send(self, atom("distribute"));
        },
        on(atom("quit")) >> [=] {
            quit();
        },
        on(atom("result"), arg_match) >> [=](uint32_t id, const QByteArray& image,
                                             bool is_opencl_enabled) {
            if (is_opencl_enabled) {
                cout << "result from opencl worker" << endl;
                --m_cur_opencl;
                m_opencl_actor_buffer.push_back(last_sender());
            }
            else {
                cout << "result from normal worker" << endl;
                --m_cur_normal;
                m_normal_actor_buffer.push_back(last_sender());
            }
            // don't use forward_to to hide workers
            // send_tuple(image_receiver, last_dequeued());
            send(image_receiver, atom("result"), id, image);
            //send_next_job(last_sender(), is_opencl_enabled);
            if (m_stream.at_end()) {
                send(image_receiver, atom("done"), m_next_id);
            }
            else {
                send(self, atom("distribute"));
            }
        },
        on(atom("resize"), arg_match) >> [=](uint32_t new_width,
                                             uint32_t new_height) {
            m_stream.resize(new_width, new_height);
        },
        on(atom("EXIT"), arg_match) >> [=](std::uint32_t err) {
            cout << "[!!!] Worker disconnectd: 0x"
                 << hex << err << "." << endl;
            // todo decrement worker count accurdingly
            auto w = m_current_jobs.find(last_sender());
            if (w != m_current_jobs.end()) {
                // todo remove from next picture list
                send(image_receiver, atom("dropped"), w->second);
                m_current_jobs.erase(w);
            }
        },
        others() >> [=] {
            cout << "[!!!] unexpected message: '"
                 << to_string(last_dequeued())
                 << "'." << endl;
        }
    );
}

void server::init() {
    trap_exit(true);
    // wait for init message
    become (
        on(atom("init"), arg_match) >> [=](actor_ptr image_receiver) {
            init(image_receiver);
        }
    );
}

struct ini_helper {
    config_map& ini;
    ini_helper(config_map& cmap) : ini(cmap) { }
    template<typename T>
    T operator()(const char* field, const T& default_value) {
        return ini.get_or_else("fractals", field, default_value);
    }
};

server::server(config_map& ini)
: m_next_id(0)
, m_max_normal(0)
, m_max_opencl(0)
, m_cur_normal(0)
, m_cur_opencl(0)
, m_lim_normal(0)
, m_lim_opencl(0) {
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
