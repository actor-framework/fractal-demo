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

void server::send_next_job(const actor_ptr& worker) {
    if (worker == nullptr || not m_stream.next()) return;
    auto& fr = m_stream.request();

//    cout << to_string(make_any_tuple(         atom("assign"),
//             m_next_id+1,
//             width(fr),
//             height(fr),
//             m_iterations,
//             min_re(fr),
//             max_re(fr),
//             min_im(fr),
//             max_im(fr))) << endl;

    send(worker,
         atom("assign"),
         ++m_next_id,
         width(fr),
         height(fr),
         m_iterations,
         min_re(fr),
         max_re(fr),
         min_im(fr),
         max_im(fr));
}

void server::init(actor_ptr image_receiver) {
    become (
        on(atom("newWorker")) >> [=] {
            auto w = last_sender();
            if (w) {
                link_to(w);
                send_next_job(w);
            }
        },
        on(atom("quit")) >> [=] {
            quit();
        },
        on(atom("result"), arg_match) >> [=](uint32_t, const QByteArray&) {
            // don't use forward_to to hide workers
            send_tuple(image_receiver, last_dequeued());
            send_next_job(last_sender());
            if (m_stream.at_end()) {
                send(image_receiver, atom("done"), m_next_id);
            }
        },
        on(atom("resize"), arg_match) >> [=](uint32_t new_width, uint32_t new_height) {
            m_stream.resize(new_width, new_height);
        },
        on(atom("EXIT"), arg_match) >> [=](std::uint32_t err) {
            cout << "[!!!] Worker disconnectd: 0x"
                 << hex << err << "." << endl;
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

server::server(config_map& ini) : m_next_id(0) {
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
