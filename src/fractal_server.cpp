#include <map>
#include <chrono>
#include <time.h>
#include <vector>
#include <complex>
#include <cstdlib>
#include <sstream>
#include <iostream>

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
#include "fractal_cppa.hpp"
#include "q_byte_array_info.hpp"

using namespace cppa;
using namespace std;

void server::init() {
        trap_exit(true);
        become (
            on(atom("next")) >> [=] {
                stringstream strstr;
                strstr << "Looking for next picture. Trying ids: " << m_next_id;
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
                    strstr << " -> found.";
                    emit(setPixmapWithByteArray(ba_itr->second));
                    m_results.erase(ba_itr);
                    ++m_next_id;
                }
                else {
                    strstr << " -> picture not calculated yet.";
                }
                send(m_printer, strstr.str());
            },
            on(atom("enqueue")) >> [=] {
                reply(atom("init"), atom("assign"), m_width, m_height, m_min_re_shifting, m_max_re_shifting, m_min_im_shifting, m_max_im_shifting, m_iterations, m_assign_id);
                m_assignments[m_assign_id] = last_sender();
                stringstream strstr;
                double step = m_zoom_steps[m_zoom_idx];
                strstr << "Worker enqueued, sent assingment with id: " << m_assign_id << "."
                       << " Current zoom  (" << m_zoom_idx << "): " << step << endl;
                auto old_re = m_max_re_shifting;
//                auto old_im = m_min_im_shifting;
                m_min_re_shifting = m_min_re * step;
                m_max_re_shifting = m_max_re * step;
                m_min_im_shifting = m_min_im * step;
                m_max_im_shifting = m_min_im_shifting+(m_max_re_shifting-m_min_re_shifting)*m_height/m_width; //m_min_im+(m_max_re-m_min_re)*m_height/m_width;
                auto shift_re = abs(old_re - m_max_re_shifting)/2*8/10;
//                auto shift_im = abs(old_im - m_min_im_shifting);
                m_min_re_shifting += shift_re;
                m_max_re_shifting += shift_re;
//                m_min_im_shifting -= shift_im;
//                m_max_im_shifting -= shift_im;
                m_re_factor = (m_max_re_shifting-m_min_re_shifting)/(m_width-1);
                m_im_factor = (m_max_im_shifting-m_min_im_shifting)/(m_height-1);
                ++m_assign_id;
                ++m_zoom_idx;
                if(m_zoom_idx == m_zoom_steps.size()) {
                    m_zoom_idx = 0;
                }
                send(m_printer, strstr.str());
            },
            on(atom("result"), arg_match) >> [=](uint32_t id, QByteArray ba) {
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
                    strstr << "Received empty image witt id " << id << ".";
                }
                send(m_printer, strstr.str());
                send(self, atom("next"));
            },
            on(atom("quit")) >> [=]() {
                quit();
            },
            on(atom("connect")) >> [=] {
                link_to(last_sender());
            },
            on(atom("EXIT"), arg_match) >> [=](std::uint32_t err) {
                stringstream strstr;
                strstr << "[!!!] Worker disconnectd: " << err << endl;
                auto sender = last_sender();
                auto itr = find_if(begin(m_assignments), end(m_assignments), [&](const map<int, cppa::actor_ptr>::value_type& kvp) {
                    return kvp.second == sender;
                });
                if(itr != m_assignments.end()) {
                    auto id = itr->first;
                    strstr << " Was working on assignment: " << id << ".";
                    m_assignments.erase(itr);
                    m_missing_ids.insert(id);
                }
                send(m_printer, strstr.str());
                send(self, atom("next"));
            },
            on(atom("init")) >> [=] {
                for(int i = 0; i < m_max_zoom_steps; ++i) {
                    m_zoom_steps.push_back(pow(0.9, i));
                }
                stringstream strstr;
                strstr << "Initilized zoom steps, added " << m_zoom_steps.size() << " steps.";
                send(m_printer, strstr.str());
            },
            on(atom("config")) >> [=] {
                stringstream strstr;
                strstr << "·················································\n"
                       << "config:\n"
                       << "-------\n"
                       << "width:  " << m_width << "\n"
                       << "height: " << m_height << "\n"
                       << "-------\n"
                       << "top left  (" << m_min_re << "/" << m_max_im << ")\n"
                       << "top right (" << m_max_re << "/" << m_max_im << ")\n"
                       << "bot left  (" << m_min_re << "/" << m_min_im << ")\n"
                       << "bot right (" << m_max_re << "/" << m_min_im << ")\n"
                       << "-------\n"
                       << "re_factor: " << m_re_factor << "\n"
                       << "im_factor: " << m_im_factor << "\n"
                       << "-------\n"
                       << "iterations for clients: " << m_iterations << "\n"
                       << "·················································";
                send(m_printer, strstr.str());
            },
            on(atom("config"), atom("small")) >> [=] {
                stringstream strstr;
                strstr << "·················································\n"
                       << "config:\n"
                       << "-------\n"
                       << "top left  (" << m_min_re << "/" << m_max_im << ")\n"
                       << "top right (" << m_max_re << "/" << m_max_im << ")\n"
                       << "bot left  (" << m_min_re << "/" << m_min_im << ")\n"
                       << "bot right (" << m_max_re << "/" << m_min_im << ")\n"
                       << "-------\n"
                       << "iterations for clients: " << m_iterations << "\n"
                       << "·················································";
                send(m_printer, strstr.str());
            },
            others() >> [=]() {
                cout << "[!!!] unexpected message: '" << to_string(last_dequeued()) << "'." << endl;
            },
            after(chrono::seconds(5)) >> [=]{
                send(this, atom("next"));
            }
        );
}

server::server(actor_ptr printer, uint32_t width, uint32_t height, double min_real, double max_real, double min_imag, uint32_t iterations, ImageLabel* lbl)
        : m_printer(printer),
          m_assign_id(0),
          m_next_id(0),
          m_power(2,0),
          m_constant(2,0),
          m_width(width),
          m_height(height),
          m_min_re(min_real),
          m_max_re(max_real),
          m_min_im(min_imag),
          m_max_im(min_imag+(max_real-min_real)*m_height/m_width),
          m_re_factor((m_max_re-m_min_re)/(m_width-1)),
          m_im_factor((m_max_im-m_min_im)/(m_height-1)),
          m_iterations(iterations),
          m_real_mult(1.0),
          m_imag_mult(1.0),
          m_min_re_shifting(m_min_re),
          m_max_re_shifting(m_max_re),
          m_min_im_shifting(m_min_im),
          m_max_im_shifting(m_max_im),
          m_zoom_idx(0),
          m_max_zoom_steps(50)
    {
        connect(this, SIGNAL(setPixmapWithByteArray(QByteArray)),
                lbl, SLOT(setPixmapFromByteArray(QByteArray)),
                Qt::QueuedConnection);
    }

server::~server() { }

void print_usage(actor_ptr printer) {
    send(printer, "blub");
}

auto main(int argc, char* argv[]) -> int {

    announce(typeid(QByteArray), new q_byte_array_info);

    announce<complex_d>(make_pair(static_cast<complex_getter>(&complex_d::real),
                                  static_cast<complex_setter>(&complex_d::real)),
                        make_pair(static_cast<complex_getter>(&complex_d::imag),
                                  static_cast<complex_setter>(&complex_d::imag)));

    vector<string> args(argv + 1, argv + argc);

    cout.unsetf(std::ios_base::unitbuf);

    auto printer = spawn_printer();

    bool has_host = false;
    bool has_port = false;

    string host;
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
        (on("-h", arg_match) || on(get_extractor("host"))) >> [&](const string& input) -> bool {
            if(input.find(' ') == string::npos && input.size() > 0) {
                host = input;
                has_host = true;
                return true;
            }
            else {
                send(printer, "Not a valid hostname.");
                return false;
            }
        },
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

    uint32_t width      =  1920;
    uint32_t height     =  1200;
    double   min_real   =  -1.9; //  2.0
    double   max_real   =   1.0; //  1.0
    double   min_imag   =  -0.9; // -1.2
    uint32_t iterations =    50;

    QApplication app(argc, argv);
    QWidget window;
    Ui::Main main;
    main.setupUi(&window);

    auto server_actor = spawn<server>(printer, width, height, min_real, max_real, min_imag, iterations, main.imgLabel);

    send(server_actor, atom("init"));
    send(server_actor, atom("config"));

    try {
        publish(server_actor, port);
        stringstream strstr;
        strstr << "Now running on port: '" << port << "'.";
        send(printer, strstr.str());
    } catch(bind_failure&) {
        stringstream strstr;
        strstr << "problem binding server to port: " << port << "'.";
        send(printer, strstr.str());
    }

    window.setWindowState(Qt::WindowFullScreen);
    window.show();
    app.quitOnLastWindowClosed();
    app.exec();
    // for(bool done = false; !done; ) {
    //     string input;
    //     getline(cin, input);
    //     if(input.size() > 0) {
    //         input.erase(input.begin());
    //         vector<string> values = split(input, ' ');
    //         match (values) (
    //             on("quit") >> [&] {
    //                 done = true;
    //             },
    //             others() >> [&] {
    //                 send(printer, "available commands:\n /quit\n");
    //             }
    //         );
    //     }
    // }
    send(server_actor, atom("quit"));

    send(printer, atom("quit"));
    await_all_others_done();
    shutdown();
    return 0;
}
