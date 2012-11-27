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

#include <QImage>
#include <QLabel>
#include <QBuffer>
#include <QPicture>
#include <QByteArray>
#include <QMainWindow>
#include <QScrollArea>
#include <QApplication>

#include "cppa/opt.hpp"
#include "cppa/cppa.hpp"

#include "ui_main.h"
//#include "client.hpp"
#include "config_map.hpp"
#include "mainwidget.hpp"
#include "fractal_cppa.hpp"
#include "fractal_server.hpp"
#include "q_byte_array_info.hpp"

using namespace cppa;
using namespace std;

namespace {
    constexpr uint32_t DEFAULT_WIDTH{1024};
    constexpr uint32_t DEFAULT_HEIGHT{768};
    constexpr uint32_t DEFAULT_INTERVAL{2000};
    constexpr uint32_t DEFAULT_ITERATION{500};
    constexpr uint32_t DEFAULT_QUEUESIZE{15};
    const long double DEFAULT_MIN_REAL{-1.9}; // must be <= 0.0
    const long double DEFAULT_MAX_REAL{1.0}; // must be >= 0.0
    const long double DEFAULT_MIN_IMAG{-0.9}; // must be <= 0.0
    const long double DEFAULT_MAX_IMAG{DEFAULT_MIN_IMAG+(DEFAULT_MAX_REAL-DEFAULT_MIN_REAL)*DEFAULT_HEIGHT/DEFAULT_WIDTH};
    const long double DEFAULT_ZOOM_FACTOR{0.5}; // must be <= 0.0
    const complex_d DEFAULT_POWER{2,0}; // used by the mandelbrot calculation
}

bool condition_once(const long double, const long double, const long double, const long double) {
    cout << "[?] condition once: true" << endl;
    return true;
}

bool condition_zoomed_out(const long double min_re, const long double max_re, const long double, const long double) {
    auto default_width = (abs(DEFAULT_MAX_REAL + (-1*DEFAULT_MIN_REAL)));
    auto current_width = abs(max_re + (-1*min_re));
    auto rc = (current_width >= default_width);
    cout << "[?] condition zoomed out: " << boolalpha << rc << endl;
    return rc;
}

bool condition_zoomed_in(const long double min_re, const long double max_re, const long double, const long double) {
    auto default_width = (abs(DEFAULT_MAX_REAL + (-1*DEFAULT_MIN_REAL)));
    auto current_width = abs(max_re + (-1*min_re));
    auto rc = (current_width <= (pow(DEFAULT_ZOOM_FACTOR, 50))* default_width);
    cout << "[?] condition zoomed in: " << boolalpha << rc << endl;
    return  rc;// todo: exchange to some double value
}

bool condition_idle(const long double, const long double, const long double, const long double) {
    cout << "[?] condition idle: " << boolalpha << false << endl;
    return false;
}

struct condition_zoomed_in_to {
    long double m_min_width;
    condition_zoomed_in_to(int zoom_step) {
        auto default_width = (abs(DEFAULT_MAX_REAL + (-1*DEFAULT_MIN_REAL)));
        m_min_width = pow(DEFAULT_ZOOM_FACTOR, zoom_step) * default_width;
        cout << "[?] condition zoomed in min size: " << m_min_width << "." << endl;
    }
    bool operator()(const long double min_re, const long double max_re, const long double, const long double) const {
        auto width  = abs(max_re + (-1*min_re));
        auto rc = (width <= m_min_width);
        cout << "[?] condition zoomed in to, current: " << width << ", min: " << m_min_width << "." << endl;
        return rc;
    }
};

struct condition_zoomed_out_to {
    long double m_max_width;
    condition_zoomed_out_to(int zoom_step) {
        auto default_width = (abs(DEFAULT_MAX_REAL + (-1*DEFAULT_MIN_REAL)));
        m_max_width = pow(DEFAULT_ZOOM_FACTOR, zoom_step) * default_width;
        cout << "[?] condition zoomed out max size: " << m_max_width << "." << endl;
    }
    bool operator()(const long double min_re, const long double max_re, const long double, const long double) const {
        auto width  = abs(max_re + (-1*min_re));
        auto rc = (width >= m_max_width);
        cout << "[?] condition zoomed out to, current: " << width << ", max: " << m_max_width << "." << endl;
        return rc;
    }
};

struct condition_equal_point {
    long double my_re;
    long double my_im;
    condition_equal_point(long double re, long double im) : my_re(re), my_im(im) { }
    bool operator()(const long double min_re, const long double max_re, const long double min_im, const long double max_im) const {
        auto half_width  = abs(max_re + (-1*min_re))/2;
        auto half_height = abs(max_im + (-1*min_im))/2;
        auto re = min_re + half_width;
        auto im = min_im + half_height;
        if ( re == my_re && im == my_im) {
            cout << "[?] conditon equal point: true" << endl;
            return true;
        }
        else {
            cout << "[?] conditon equal point: false (" << re << "/" << im << ") != (" << my_re << "/" << my_im << ")." << endl;
            return false;
        }
    }
};

struct condition_near {
    long double m_re;
    long double m_im;
    long double m_radius;
    condition_near(long double re, long double im, long double radius) : m_re(re), m_im(im), m_radius(radius) { }
    bool operator()(const long double min_re, const long double max_re, const long double min_im, const long double max_im) const {
        auto half_width  = abs(max_re + (-1*min_re))/2;
        auto half_height = abs(max_im + (-1*min_im))/2;
        auto re = min_re + half_width;
        auto im = min_im + half_height;
        if (abs(m_re-re) < m_radius && abs(m_im-im) < m_radius) {
            cout << "[?] conditon near: true" << endl;
            return true;
        }
        else {
            cout << "[?] conditon near: false" << endl;
            return false;
        }
    }
};

ld_tuple reset_view(long double, long double, long double, long double) {
    cout << "[~] resetting view" << endl;
    auto half_width  = abs(DEFAULT_MAX_REAL + (-1*DEFAULT_MIN_REAL))/2;
    auto half_height = abs(DEFAULT_MAX_IMAG + (-1*DEFAULT_MIN_IMAG))/2;
    long double re{0.0};
    long double im{0.0};
    return make_tuple(re-half_width, re+half_width, im-half_height, im+half_height);
}

ld_tuple idle(long double min_re, long double max_re, long double min_im, long double max_im) {
    cout << "[~] idleing" << endl;
    return make_tuple(min_re, max_re, min_im, max_im);
}

ld_tuple zoom_in(long double min_re, long double max_re, long double min_im, long double max_im) {
    cout << "[~] zooming in" << endl;
    auto half_width  = abs(max_re + (-1*min_re))/2;
    auto half_height = abs(max_im + (-1*min_im))/2;
    auto re = min_re + half_width;
    auto im = min_im + half_height;
    half_width  *= DEFAULT_ZOOM_FACTOR;
    half_height *= DEFAULT_ZOOM_FACTOR;
    return make_tuple(re-half_width, re+half_width, im-half_height, im+half_height);
}

ld_tuple zoom_out(long double min_re, long double max_re, long double min_im, long double max_im) {
    cout << "[~] zooming out" << endl;
    auto half_width  = abs(max_re + (-1*min_re))/2;
    auto half_height = abs(max_im + (-1*min_im))/2;
    auto re = min_re + half_width;
    auto im = min_im + half_height;
    half_width  *= (1/DEFAULT_ZOOM_FACTOR);
    half_height *= (1/DEFAULT_ZOOM_FACTOR);
    return make_tuple(re-half_width, re+half_width, im-half_height, im+half_height);
}

struct move_line {
    long double m_to_re;
    long double m_to_im;
    long double m_move_re;
    long double m_move_im;
    long double m_move_dist;
    move_line(long double from_re, long double from_im, long double to_re, long double to_im) : m_to_re(to_re), m_to_im(to_im) {
        m_move_re = (to_re - from_re)/5;
        m_move_im = (to_im - from_im)/5;
        m_move_dist = sqrt(pow(m_move_re, 2) + pow(m_move_im, 2));
    }
    ld_tuple operator()(long double min_re, long double max_re, long double min_im, long double max_im) const {
        cout << "[~] moveing along line" << endl;
        auto half_width  = abs(max_re + (-1*min_re))/2;
        auto half_height = abs(max_im + (-1*min_im))/2;
        auto current_re = min_re + half_width;
        auto current_im = min_im + half_height;
        auto dist = sqrt(pow(m_to_re - current_re, 2) + pow(m_to_im - current_im, 2));
        if (dist <= m_move_dist) {
            current_re = m_to_re;
            current_im = m_to_im;
        }
        else {
            auto change_x = (m_to_re - current_re);
            auto change_y = (m_to_im - current_im);
            auto change_length = sqrt(pow(change_x, 2) + pow(change_y, 2));
            change_x = (change_x / change_length) * m_move_dist;
            change_y = (change_y / change_length) * m_move_dist;
            current_re += change_x;
            current_im += change_y;
        }
        return make_tuple(current_re-half_width, current_re+half_width, current_im-half_height, current_im+half_height);
    }
};

struct move_line_zoom_in {
    move_line m_move_line;
    move_line_zoom_in(long double from_re, long double from_im, long double to_re, long double to_im) : m_move_line(from_re, from_im, to_re, to_im) { }
    ld_tuple operator()(long double min_re, long double max_re, long double min_im, long double max_im) const {
        auto tmp = m_move_line(min_re, max_re, min_im, max_im);
        if (condition_zoomed_in(get<0>(tmp), get<1>(tmp), get<2>(tmp), get<3>(tmp))) { // zoomed in
            return tmp;
        }
        else { // not zoomed in -> zoom in
            return zoom_in(get<0>(tmp), get<1>(tmp), get<2>(tmp), get<3>(tmp));
        }
    }
};

struct move_line_zoom_out {
    move_line m_move_line;
    move_line_zoom_out(long double from_re, long double from_im, long double to_re, long double to_im) : m_move_line(from_re, from_im, to_re, to_im) { }
    ld_tuple operator()(long double min_re, long double max_re, long double min_im, long double max_im) const {
        auto tmp = m_move_line(min_re, max_re, min_im, max_im);
        if (condition_zoomed_out(get<0>(tmp), get<1>(tmp), get<2>(tmp), get<3>(tmp))) { // zoomed out
            return tmp;
        }
        else { // not zoomed out -> zoom out
            return zoom_out(get<0>(tmp), get<1>(tmp), get<2>(tmp), get<3>(tmp));
        }
    }
};

void server::initialize_stack() {
    m_operations.clear();
    m_operations.emplace_back(idle, condition_idle);
}

void server::add_start_move(long double from_x, long double from_y, long double to_x, long double to_y, int max_zoom) {
    m_operations.emplace_back(zoom_in, condition_zoomed_in_to(max_zoom));
    m_operations.emplace_back(move_line_zoom_in(from_x, from_y, to_x, to_y), condition_equal_point(to_x, to_y));
}

void server::add_move_from_to(long double from_x, long double from_y, long double to_x, long double to_y, int max_zoom) {
    long double mid_x{from_x + ((to_x - from_x)/2)};
    long double mid_y{from_y + ((to_y - from_y)/2)};
    m_operations.emplace_back(zoom_in, condition_zoomed_in_to(max_zoom));
    m_operations.emplace_back(move_line_zoom_in(mid_x, mid_y, to_x, to_y),condition_equal_point(to_x, to_y));
    m_operations.emplace_back(move_line_zoom_out(from_x, from_y, mid_x, mid_y), condition_equal_point(mid_x, mid_y));
    m_operations.emplace_back(zoom_out, condition_zoomed_out_to(5));
}

void server::add_end_move(long double from_x, long double from_y, long double to_x, long double to_y) {
    m_operations.emplace_back(move_line_zoom_out(from_x, from_y, to_x, to_y), condition_equal_point(to_x, to_y));
    m_operations.emplace_back(zoom_out, condition_zoomed_out_to(5));
}

void server::add_chain(std::vector<std::pair<long double, long double> >& chain, int zoom) {
    if (chain.size() > 0) {
        reverse(begin(chain), end(chain));
        long double start_end_x{0.0};
        long double start_end_y{0.0};
        auto first = chain.begin();
        auto second = chain.begin()+1;
        auto end = chain.end();
//        cout << "[CHAIN] (" << first->first << "/" << first -> second << ") => (" << start_end_x << "/" << start_end_y << ")" << endl;
        add_end_move(first->first, first->second, start_end_x, start_end_y);
        for (; second != end; ++first, ++second) {
//            cout << "[CHAIN] (" << second->first << "/" << second->second << ") => (" << first->first << "/" << first -> second << ")" << endl;
            add_move_from_to(second->first, second->second, first->first, first->second, zoom);
        }
//        cout << "[CHAIN] (" << start_end_x << "/" << start_end_y << ") => (" << first->first << "/" << first -> second << ")" << endl;
        add_start_move(start_end_x, start_end_y, first->first, first->second, zoom);
    }
    else {
        cout << "Chain is empty, no moves added." << endl;
    }
}

void server::init() {
    trap_exit(true);
    send(m_gui, atom("server"), this);
    delayed_send(self, chrono::milliseconds(m_interval), atom("next"));
    become (
        on(atom("enqueue")) >> [=] {
            m_available_workers.push_back(last_sender());
            cout << "Worker enqueued. Available workers: " << m_available_workers.size() << "." << endl;
            reply(atom("ack"), atom("enqueue"));
        },
        on(atom("next")) >> [=] {
            cout << "Looking for next picture. " << m_next_id << flush;
            auto id_itr = m_missing_ids.find(m_next_id);
            auto e = m_missing_ids.end();
            while(id_itr != e) {
                ++m_next_id;
                m_missing_ids.erase(id_itr);
                id_itr = m_missing_ids.find(m_next_id);
                cout << ", " << m_next_id << flush;
            }
            auto ba_itr = m_results.find(m_next_id);
            if (ba_itr != m_results.end()) {
                cout << ", found." << endl;
                send(m_gui, atom("display"), ba_itr->second);
//                emit(setPixmapWithByteArray(ba_itr->second));
                m_results.erase(ba_itr);
                ++m_next_id;
            }
            else {
                cout << ", not calculated yet." << endl;
            }
            send(self, atom("dequeue"));
            delayed_send(self, chrono::milliseconds(m_interval), atom("next"));
        },
        on(atom("dequeue")) >> [=]{
            int counter{0};
            for (bool done{false}; !done;) {
                if (counter >= 10) {
                    cout << "Let's do something else for a while." << endl;
                    done = true;
                } else if (m_available_workers.empty()) {
                    cout << "No more workers available." << endl;
                    done = true;
                }
                else if (m_results.size() > m_queuesize) {
                    cout << "Got enought pictures stored." << endl;
                    done = true;
                }
                else {
                    if (m_operations.back().second(m_min_re, m_max_re, m_min_im, m_max_im)) {
                        m_operations.pop_back();
                    }
                    auto image = m_operations.back().first(m_min_re, m_max_re, m_min_im, m_max_im);
                    if (m_min_re == get<0>(image) && m_max_re == get<1>(image) && m_min_im == get<2>(image) && m_max_im == get<3>(image)) {
                        cout << "Next picture woud look the same." << endl;
                        done = true;
                    }
                    else {
                        auto worker = m_available_workers.back();
                        cout << "Assigning calculations, image id: " << m_assign_id << "." << endl;
                        m_min_re = get<0>(image);
                        m_max_re = get<1>(image);
                        m_min_im = get<2>(image);
                        m_max_im = get<3>(image);
                        send(worker, atom("assign"), m_width, m_height, m_min_re, m_max_re, m_min_im, m_max_im, m_iterations, m_assign_id);
                        cout << "[ASSIGNED] picture: " << m_assign_id
//                             << "\n\tm_min_re: " << m_min_re
//                             << "\n\tm_max_re: " << m_max_re
//                             << "\n\tm_min_im: " << m_min_im
//                             << "\n\tm_max_im: " << m_max_im
                             << endl;
                        ++m_assign_id;
                        m_available_workers.pop_back();
                        cout << "Available workers: " << m_available_workers.size() << "." << endl;
                    }
                }
                ++counter;
            }
        },
        on(atom("result"), arg_match) >> [=](uint32_t id, const QByteArray& ba) {
            cout << "Received image with size: " << ba.size() << "." << endl;
            auto itr = m_assignments.find(id);
            if (itr != m_assignments.end()) {
                m_assignments.erase(itr);
            }
            if (ba.size() > 0) {
                cout << "Received image with id: " << id << "." << endl;
                m_results[id] = ba;
            }
            else {
                m_missing_ids.insert(id);
                cout << "Received empty image with id " << id << "." << endl;
            }
            cout << "Stored " << m_results.size() << " picture." << endl;
        },
        on(atom("resize"), arg_match) >> [=](int width, int height) {
            m_width = width;
            m_height = height;
            m_max_im = m_min_im+(m_max_re-m_min_re)*m_height/m_width;
            cout << "Received new width: " << width << " and hew height: " << height << "." << endl;
        },
        on(atom("link")) >> [=] {
            cout << "Connected to new worker." << endl;
            link_to(last_sender());
        },
        on(atom("quit")) >> [=] {
            quit();
        },
        on(atom("EXIT"), arg_match) >> [=](std::uint32_t err) {
            cout << "[!!!] Worker disconnectd: 0x" << hex << err << "." << flush;
            auto sender = last_sender();
            auto as_itr = find_if(begin(m_assignments), end(m_assignments), [&](const map<int, cppa::actor_ptr>::value_type& kvp) {
                return kvp.second == sender;
            });
            if (as_itr != m_assignments.end()) {
                auto id = as_itr->first;
                cout << " Was working on assignment: " << dec << id << "." << endl;
                m_assignments.erase(as_itr);
                m_missing_ids.insert(id);
            }
            auto b = m_available_workers.begin();
            auto e = m_available_workers.end();
            auto aw_itr = find(b, e, sender);
            if (aw_itr != e) {
                cout << " Was waiting for work." << endl;
                m_available_workers.erase(aw_itr);
            }
        },
        others() >> [=]() {
            cout << "[!!!] unexpected message: '" << to_string(last_dequeued()) << "'." << endl;
        }
    );
}

server::server(const cppa::actor_ptr& gui, uint32_t interval, uint32_t iterations, uint32_t queuesize, double zoom)
    : m_gui(gui),
      m_interval{interval},
      m_iterations{iterations},
      m_queuesize{queuesize},
      m_zoom{zoom},
      m_next_id{0},
      m_assign_id{0},
      m_power{DEFAULT_POWER},
      m_width{DEFAULT_WIDTH},
      m_height{DEFAULT_HEIGHT},
      m_min_re{0},
      m_max_re{0},
      m_min_im{0},
      m_max_im{0}
    {
        cout << "Initializing operations stack." << endl;
        initialize_stack();
//            vector<pair<long double, long double> > chain {make_pair(-1.86572513851221765677, 0.0)};
        vector<pair<long double, long double> > chain;
        chain.push_back(make_pair(-1.86572513851221765677, 0.0));
        chain.push_back(make_pair(-0.7458555,  0.10550365));
//            chain.push_back(make_pair(-0.0012 / 0.7383));
//            chain.push_back(make_pair(-0.13856524454488, -0.64935990748190));
        add_chain(chain, 15);
        m_operations.emplace_back(reset_view, condition_once);
        m_operations.emplace_back(reset_view, condition_once);
    }

server::~server() { }

void print_usage() {
    cout << "Usage: fractal_server\n -p, --port=\t\tport to use" << endl;
}

auto main(int argc, char* argv[]) -> int {
    announce(typeid(QByteArray), new q_byte_array_info);
    announce<complex_d>(make_pair(static_cast<complex_getter>(&complex_d::real),
                                  static_cast<complex_setter>(&complex_d::real)),
                        make_pair(static_cast<complex_getter>(&complex_d::imag),
                                  static_cast<complex_setter>(&complex_d::imag)));

    cout.unsetf(std::ios_base::unitbuf);
    cout.precision(numeric_limits<long double>::max_digits10);

    uint16_t port{20283};
    options_description desc;
    bool args_valid = match_stream<string>(argv + 1, argv + argc) (
        on_opt1('p', "port", &desc, "set port (default: 20283)") >> rd_arg(port),
        on_opt0('h', "help", &desc, "print help") >> print_desc_and_exit(&desc)
    );

    if (!args_valid) print_desc_and_exit(&desc)();

    config_map server_ini;
    server_ini.read_ini("fractal_server.ini");
    if (!server_ini.has_group("fractals"))
    {
        cerr << "no [fractals] section found in ini file" << endl;
        return -1;
    }

    uint32_t interval{server_ini.get_or_else<uint32_t>("fractals", "interval", DEFAULT_INTERVAL)};
    uint32_t iterations{server_ini.get_or_else<uint32_t>("fractals", "iterations", DEFAULT_ITERATION)};
    uint32_t queuesize{server_ini.get_or_else<uint32_t>("fractals", "queuesize", DEFAULT_QUEUESIZE)};
    double zoom{server_ini.get_or_else<double>("fractals", "zoom", DEFAULT_ZOOM_FACTOR)};

    QApplication app{argc, argv};
    QMainWindow window;
    Ui::Main main;
    main.setupUi(&window);
    auto gui = main.mainWidget->as_actor();
    send(gui, atom("main"), main);
//    send(gui, atom("imagelabel"), main.imgLabel);
    cout << "interval: " << interval << "\niterations: " << iterations << "\nqueuesize: " << queuesize << "\nzoom: " << zoom << endl;
    auto server_actor = spawn<server>(gui, interval, iterations, queuesize, zoom);
    try {
        publish(server_actor, port);
        cout << "Now running on port: '" << port << "'." << endl;
//        window.setWindowState(Qt::WindowFullScreen);
        window.show();
        app.quitOnLastWindowClosed();
        app.exec();
    } catch(bind_failure&) {
        cout << "problem binding server to port: " << port << "'." << endl;
    }
    send(server_actor, atom("quit"));
    await_all_others_done();
    shutdown();
    return 0;
}
