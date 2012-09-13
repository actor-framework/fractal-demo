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

#include "cppa/cppa.hpp"
#include "cppa/match.hpp"

#include "ui_main.h"
#include "server.hpp"
#include "client.hpp"
#include "mainwidget.hpp"
#include "fractal_cppa.hpp"
#include "q_byte_array_info.hpp"

using namespace cppa;
using namespace std;

bool condition_zoomed_out(const long double min_re, const long double max_re, const long double min_im, const long double max_im) {
    auto width  = abs(max_re + (-1*min_re));
    auto height = abs(max_im + (-1*min_im));
    auto rc = (width >= 2.5);
    cout << "[?] condition zoomed out: " << boolalpha << rc << endl;
    return rc;
}

bool condition_zoomed_in(const long double min_re, const long double max_re, const long double min_im, const long double max_im) {
    auto width  = abs(max_re + (-1*min_re));
    auto height = abs(max_im + (-1*min_im));
    auto rc = (width <= (3.0*pow(0.5, 50)));
        cout << "[?] condition zoomed in: " << boolalpha << rc << endl;
    return  rc;// todo: exchange to some double value
}

bool condition_idle(const long double min_re, const long double max_re, const long double min_im, const long double max_im) {
    cout << "[?] condition idle: " << boolalpha << false << endl;
    return false;
}

struct condition_equal_point {
    long double my_x;
    long double my_y;
    condition_equal_point(long double x, long double y) : my_x(x), my_y(y) { }
    bool operator()(const long double min_re, const long double max_re, const long double min_im, const long double max_im) const {
        auto x = min_re + (abs(max_re + (-1*min_re))/2);
        auto y = min_im + (abs(max_im + (-1*min_im))/2);
        if( x == my_x && y == my_y) {
            cout << "[?] conditon equal point: true" << endl;
            return true;
        }
        else {
            cout << "[?] conditon equal point: false (" << x << " != " << my_x << " / " << y << " != " << my_y << ")" << endl;
            return false;
        }
    }
};

struct condition_near {
    long double m_x;
    long double m_y;
    long double m_radius;
    condition_near(long double x, long double y, long double radius) : m_x(x), m_y(y), m_radius(radius) { }
    bool operator()(const long double min_re, const long double max_re, const long double min_im, const long double max_im) const {
        auto x = min_re + (abs(max_re + (-1*min_re))/2);
        auto y = min_im + (abs(max_im + (-1*min_im))/2);
        if( abs(m_x-x) < m_radius && abs(m_y-y) < m_radius) {
            cout << "[?] conditon near: true" << endl;
            return true;
        }
        else {
            cout << "[?] conditon near: false" << endl;
            return false;
        }
    }
};

ld_tuple idle(long double min_re, long double max_re, long double min_im, long double max_im) {
    cout << "[~] idleing" << endl;
    return make_tuple(min_re, max_re, min_im, max_im);
}

ld_tuple zoom_in(long double min_re, long double max_re, long double min_im, long double max_im) {
    cout << "[~] zooming in" << endl;
    auto width  = abs(max_re + (-1*min_re));
    auto height = abs(max_im + (-1*min_im));
    auto x = min_re + (width /2);
    auto y = min_im + (height/2);
    width  /= 4.0; //2.0;
    height /= 4.0; //2.0;
    return make_tuple(x-width, x+width, y-height, y+height);
}

ld_tuple zoom_out(long double min_re, long double max_re, long double min_im, long double max_im) {
    cout << "[~] zooming out" << endl;
    auto width  = abs(max_re + (-1*min_re));
    auto height = abs(max_im + (-1*min_im));
    auto x = min_re + (width /2);
    auto y = min_im + (height/2);
    width  *= 2.0;
    height *= 2.0;
    return make_tuple(x-width, x+width, y-height, y+height);
}

struct move_line {
    long double m_to_x;
    long double m_to_y;
    long double m_move_re;
    long double m_move_im;
    long double m_move_dist;
    move_line(long double from_x, long double from_y, long double to_x, long double to_y) : m_to_x(to_x), m_to_y(to_y) {
        m_move_re = (to_x - from_x)/5;
        m_move_im = (to_y - from_y)/5;
        m_move_dist = sqrt(pow(m_move_re, 2) + pow(m_move_im, 2));
//        auto length = sqrt(pow(m_move_re, 2) + pow(m_move_im,2));
//        m_move_re = (m_move_re/length)/10;
//        m_move_im = (m_move_im/length)/10;
    }
    ld_tuple operator()(long double min_re, long double max_re, long double min_im, long double max_im) const {
        cout << "[~] moveing along line" << endl;
        auto width  = abs(max_re + (-1*min_re));
        auto height = abs(max_im + (-1*min_im));
        auto current_x = min_re + (width /2);
        auto current_y = min_im + (height/2);
        auto dist_x = m_to_x - current_x;
        auto dist_y = m_to_y - current_y;
        auto dist = sqrt(pow(dist_x, 2) + pow(dist_y, 2));
//        if(abs(dist_x) <= abs(m_move_re) || abs(dist_y) <= abs(m_move_im)) {
        if(dist <= m_move_dist) {
            current_x = m_to_x;
            current_y = m_to_y;
        }
        else {
            current_x += m_move_re;
            current_y += m_move_im;
        }
        width  /= 2;
        height /= 2;
        return make_tuple(current_x-width, current_x+width, current_y-height, current_y+height);
    }
};

struct move_line_zoom_in {
    long double m_to_x;
    long double m_to_y;
    long double m_move_re;
    long double m_move_im;
    long double m_move_dist;
    move_line_zoom_in(long double from_x, long double from_y, long double to_x, long double to_y) : m_to_x(to_x), m_to_y(to_y) {
        m_move_re = (to_x - from_x)/5;
        m_move_im = (to_y - from_y)/5;
        m_move_dist = sqrt(pow(m_move_re, 2) + pow(m_move_im, 2));
//        auto length = sqrt(pow(m_move_re, 2) + pow(m_move_im,2));
//        m_move_re = (m_move_re/length)/10;
//        m_move_im = (m_move_im/length)/10;
    }
    ld_tuple operator()(long double min_re, long double max_re, long double min_im, long double max_im) const {
        cout << "[~] moving along line (zoom in)" << endl;
        auto width  = abs(max_re + (-1*min_re));
        auto height = abs(max_im + (-1*min_im));
        auto current_x = min_re + (width /2);
        auto current_y = min_im + (height/2);
        auto dist_x = m_to_x - current_x;
        auto dist_y = m_to_y - current_y;
        auto dist = sqrt(pow(dist_x, 2) + pow(dist_y, 2));
//        if(abs(dist_x) <= abs(m_move_re) || abs(dist_y) <= abs(m_move_im)) {
        if(dist <= m_move_dist) {
            current_x = m_to_x;
            current_y = m_to_y;
        }
        else {
            current_x += m_move_re;
            current_y += m_move_im;
        }
        if(condition_zoomed_in(max_re, min_re, max_im, min_im)) { // zoomed in
            width  /= 2;
            height /= 2;
        }
        else { // not zoomed in zoomed in
            width  /= 4;
            height /= 4;
        }
        return make_tuple(current_x-width, current_x+width, current_y-height, current_y+height);
    }
};

struct move_line_zoom_out {
    long double m_to_x;
    long double m_to_y;
    long double m_move_re;
    long double m_move_im;
    long double m_move_dist;
    move_line_zoom_out(long double from_x, long double from_y, long double to_x, long double to_y) : m_to_x(to_x), m_to_y(to_y) {
        m_move_re = (to_x - from_x)/5;
        m_move_im = (to_y - from_y)/5;
        m_move_dist = sqrt(pow(m_move_re, 2) + pow(m_move_im, 2));
//        auto length = sqrt(pow(m_move_re, 2) + pow(m_move_im,2));
//        m_move_re = (m_move_re/length)/10;
//        m_move_im = (m_move_im/length)/10;
    }
    ld_tuple operator()(long double min_re, long double max_re, long double min_im, long double max_im) const {
        cout << "[~] moving along line (zoom out)" << endl;
        auto width  = abs(max_re + (-1*min_re));
        auto height = abs(max_im + (-1*min_im));
        auto current_x = min_re + (width /2);
        auto current_y = min_im + (height/2);
        auto dist_x = m_to_x - current_x;
        auto dist_y = m_to_y - current_y;
        auto dist = sqrt(pow(dist_x, 2) + pow(dist_y, 2));
//        if(abs(dist_x) <= abs(m_move_re) || abs(dist_y) <= abs(m_move_im)) {
        if(dist <= m_move_dist) {
            current_x = m_to_x;
            current_y = m_to_y;
        }
        else {
            current_x += m_move_re;
            current_y += m_move_im;
        }
        if(condition_zoomed_out(min_re, max_re, min_im, max_im)) { // zoomed out
            width  /= 2;
            height /= 2;
        }
//        width  /= 2;
//        height /= 2;
        return make_tuple(current_x-width, current_x+width, current_y-height, current_y+height);
    }
};

void server::initialize_stack() {
    m_operations.clear();
    m_operations.emplace_back(idle, condition_idle);
}

void server::init() {
    trap_exit(true);
    delayed_send(self, chrono::seconds(3), atom("next"));
    become (
        on(atom("next")) >> [=] {
            stringstream strstr;
            strstr << "Looking for next picture. " << m_next_id;
            auto id_itr = m_missing_ids.find(m_next_id);
            auto e = m_missing_ids.end();
            while(id_itr != e) {
                ++m_next_id;
                m_missing_ids.erase(id_itr);
                id_itr = m_missing_ids.find(m_next_id);
                strstr << ", " << m_next_id;
            }
            auto ba_itr = m_results.find(m_next_id);
            if(ba_itr != m_results.end()) {
                strstr << ", found.";
                emit(setPixmapWithByteArray(ba_itr->second));
                m_results.erase(ba_itr);
                ++m_next_id;
            }
            else {
                strstr << ", not calculated yet.";
            }
            send(m_printer, strstr.str());
            delayed_send(self, chrono::seconds(3), atom("next"));
            // check for available workers
            if(!m_available_workers.empty() && m_results.size() < MIN_RESULTS_STORED) {
                send(self, atom("dequeue"));
            }
        },
        on(atom("enqueue")) >> [=] {
            m_available_workers.push_back(last_sender());
        },
        on(atom("dequeue")) >> [=]{
            for(bool done = false; !done;) {
                auto image = m_operations.back().first(m_min_re, m_max_re, m_min_im, m_max_im);
                if(m_operations.back().second(get<0>(image), get<1>(image), get<2>(image), get<3>(image))) {
                    m_operations.pop_back();
                }

                if(m_available_workers.empty()) {
                    send(m_printer, "No more workers available.");
                    done = true;
                }
                else if (m_results.size() > MAX_RESULTS_STORED) {
                    send(m_printer, "Got enought pictures stored.");
                    done = true;
                }
                else if (m_min_re == get<0>(image) && m_max_re == get<1>(image) && m_min_im == get<2>(image) && m_max_im == get<3>(image)) {
                    send(m_printer, "Next picture woud look the same.");
                    done = true;
                }
                else {
                    auto worker = m_available_workers.back();
                    stringstream strstr;
                    strstr << "Assigning calculations, image id: " << m_assign_id << ".";
                    m_min_re = get<0>(image);
                    m_max_re = get<1>(image);
                    m_min_im = get<2>(image);
                    m_max_im = get<3>(image);
                    send(worker, atom("init"), atom("assign"), m_width, m_height, m_min_re, m_max_re, m_min_im, m_max_im, m_iterations, m_assign_id);
                    ++m_assign_id;
                    m_available_workers.pop_back();

                    auto width  = abs(m_max_re + (-1*m_min_re));
                    auto height = abs(m_max_im + (-1*m_min_im));
                    auto x = m_min_re + (width /2);
                    auto y = m_min_im + (height/2);
                    strstr << " Current pos: (" << x << "/" << y << ").";
                    send(m_printer, strstr.str());
                }
            }
        },
        on(atom("jumpto"), arg_match) >> [=](const long double& target_x, const long double& target_y) {
            stringstream strstr;
            strstr << "Initiating jump to (" << target_x << "/" << target_y << ").";
            send(m_printer, strstr.str());

            initialize_stack();

            auto width  = abs(m_max_re + (-1*m_min_re));
            auto height = abs(m_max_im + (-1*m_min_im));

            auto current_x = m_min_re + (width /2);
            auto current_y = m_min_im + (height/2);

//            auto mid_x = current_x + ((target_x - current_x)/2);
//            auto mid_y = current_y + ((target_y - current_y)/2);

            /* MOVE ZOOM OUT, MOVE ZOOM IN */
//            move_line_zoom_in to_target(mid_x, mid_y, target_x, target_y);
//            move_line_zoom_out to_mid(current_x, current_y, mid_x, mid_y);

//            condition_equal_point target(target_x, target_y);
//            condition_equal_point mid(mid_x, mid_y);

//            m_operations.emplace_back(zoom_in, condition_zoomed_in);
//            m_operations.emplace_back(to_target,target);
//            m_operations.emplace_back(to_mid, mid);

            /* ZOOM OUT, MOVE, ZOOM IN */
//            move_line moveing(current_x, current_y, target_x, target_y);
//            condition_equal_point target(target_x, target_y);

//            m_operations.emplace_back(zoom_in, condition_zoomed_in);
//            m_operations.emplace_back(moveing, target);
//            m_operations.emplace_back(zoom_out, condition_zoomed_out);

            /* MOVE ZOOM OUT MID, MOVE ZOOM IN TARGET */
            move_line_zoom_out to_mid(current_x, current_y, 0.0, 0.0);
            move_line_zoom_in to_target(0.0, 0.0, target_x, target_y);

            condition_equal_point cond_mid(0.0, 0.0);
            condition_equal_point cond_target(target_x, target_y);

            m_operations.emplace_back(zoom_in, condition_zoomed_in);
            m_operations.emplace_back(to_target, cond_target);
            m_operations.emplace_back(zoom_out, condition_zoomed_out);
            m_operations.emplace_back(to_mid, cond_mid);
        },
        on(atom("result"), arg_match) >> [=](uint32_t id, const QByteArray& ba) {
            auto itr = m_assignments.find(id);
            if(itr != m_assignments.end()) {
                m_assignments.erase(itr);
            }
            stringstream strstr;
            if(ba.size() > 0) {
                strstr << "Received image with id: " << id << ".";
                m_results[id] = ba;
            }
            else {
                m_missing_ids.insert(id);
                strstr << "Received empty image with id " << id << ".";
            }
            send(m_printer, strstr.str());
        },
        on(atom("resize"), arg_match) >> [=](int width, int height) {
            m_width = width;
            m_height = height;
            m_max_im = m_min_im+(m_max_re-m_min_re)*m_height/m_width;
            stringstream strstr;
            strstr << "Received new width: " << width << " and hew height: " << height;
            send(m_printer, strstr.str());
        },
        on(atom("init")) >> [=] {
            send(m_printer, "Initializing operations stack.");
            initialize_stack();
            m_operations.emplace_back(zoom_out, condition_zoomed_out);
        },
        on(atom("connect")) >> [=] {
            link_to(last_sender());
        },
        on(atom("reset")) >> [=] {
            send(m_printer, "Resetting!");
            initialize_stack();
            m_iterations = DEFAULT_ITERATIONS;
            m_width = DEFAULT_WIDTH;
            m_height = DEFAULT_HEIGHT;
            m_min_re = DEFAULT_MIN_REAL;
            m_max_re = DEFAULT_MAX_REAL;
            m_min_im = DEFAULT_MIN_IMAG;
            m_max_im = DEFAULT_MAX_IMAG;
        },
        on(atom("quit")) >> [=] {
            quit();
        },
        on(atom("EXIT"), arg_match) >> [=](std::uint32_t err) {
            stringstream strstr;
            strstr << "[!!!] Worker disconnectd: 0x" << hex << err << ".";
            auto sender = last_sender();
            auto as_itr = find_if(begin(m_assignments), end(m_assignments), [&](const map<int, cppa::actor_ptr>::value_type& kvp) {
                return kvp.second == sender;
            });
            if(as_itr != m_assignments.end()) {
                auto id = as_itr->first;
                strstr << " Was working on assignment: " << dec << id << ".";
                m_assignments.erase(as_itr);
                m_missing_ids.insert(id);
            }
            auto b = m_available_workers.begin();
            auto e = m_available_workers.end();
            auto aw_itr = find(b, e, sender);
            if(aw_itr != e) {
                strstr << " Was waiting for work.";
                m_available_workers.erase(aw_itr);
            }
            send(m_printer, strstr.str());
        },
        others() >> [=]() {
            cout << "[!!!] unexpected message: '" << to_string(last_dequeued()) << "'." << endl;
        }
    );
}

server::server(actor_ptr printer, ImageLabel* lbl, MainWidget* mw)
        : m_printer(printer),
          m_next_id(0),
          m_assign_id(0),
          m_power(2,0),
          m_width(DEFAULT_WIDTH),
          m_height(DEFAULT_HEIGHT),
          m_min_re(DEFAULT_MIN_REAL),
          m_max_re(DEFAULT_MAX_REAL),
          m_min_im(DEFAULT_MIN_IMAG),
          m_max_im(DEFAULT_MAX_IMAG),
          m_iterations(DEFAULT_ITERATIONS)
    {
        connect(this, SIGNAL(setPixmapWithByteArray(QByteArray)),
                lbl, SLOT(setPixmapFromByteArray(QByteArray)),
                Qt::QueuedConnection);
        mw->setServer(this);
    }

server::~server() { }

void print_usage(actor_ptr printer) {
    send(printer, "Usage: fractal_server\n -p, --port=\t\tport to use");
}

auto main(int argc, char* argv[]) -> int {

    announce(typeid(QByteArray), new q_byte_array_info);

    announce<complex_d>(make_pair(static_cast<complex_getter>(&complex_d::real),
                                  static_cast<complex_setter>(&complex_d::real)),
                        make_pair(static_cast<complex_getter>(&complex_d::imag),
                                  static_cast<complex_setter>(&complex_d::imag)));

    vector<string> args(argv + 1, argv + argc);

    cout.unsetf(std::ios_base::unitbuf);

    cout.precision(numeric_limits<long double>::max_digits10);

    auto printer = spawn_printer();

    bool has_port = false;

    uint16_t port;

    auto toint = [](const string& str) -> option<int> {
        char* endptr = nullptr;
        int result = static_cast<int>(strtol(str.c_str(), &endptr, 10));
        if (endptr != nullptr && *endptr == '\0') {
            return result;
        }
        return {};
    };

    bool success = match_stream<string>(begin(args), end(args)) (
        (on("-p", arg_match) || on(get_extractor("port"))) >> [&](const string& input) -> bool {
            auto tmp = toint(input);
            if(tmp) {
                port = *tmp;
                has_port = true;
                return true;
            }
            else {
                return false;
            }
        }
    );

    if(!success) {
        print_usage(printer);
        send(printer, atom("quit"));
        await_all_others_done();
        shutdown();
        return 0;
    }

    if(!has_port) {
        srand(time(NULL));
        send(printer, "Randomness 4tw!");
        port = (rand()%(static_cast<int>(numeric_limits<uint16_t>::max()) - 1024))+1024;
    }

    QApplication app(argc, argv);
    QMainWindow window;
    Ui::Main main;
    main.setupUi(&window);

    auto server_actor = spawn<server>(printer, main.imgLabel, main.mainWidget);
    send(server_actor, atom("init"));

    try {
        publish(server_actor, port);
        stringstream strstr;
        strstr << "Now running on port: '" << port << "'.";
        send(printer, strstr.str());
//        window.setWindowState(Qt::WindowFullScreen);
        window.show();
        app.quitOnLastWindowClosed();
        app.exec();
    } catch(bind_failure&) {
        stringstream strstr;
        strstr << "problem binding server to port: " << port << "'.";
        send(printer, strstr.str());
    }
    send(server_actor, atom("quit"));
    send(printer, atom("quit"));
    await_all_others_done();
    shutdown();
    return 0;
}
