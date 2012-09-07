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
            on(atom("enqueue")) >> [=] {
                // reply(atom("initialize"), m_width, m_height, m_min_re, m_max_re, m_min_im, m_max_im, m_iterations);
                // reply(atom("assign"), ++m_id);
                stringstream strstr;
                strstr << "Worker enqueued, sending assingment with id: " << (m_id+1) << ".";
                send(m_printer, strstr.str());
                reply(atom("init"), atom("assign"), m_width, m_height, m_min_re, m_max_re, m_min_im, m_max_im, m_iterations, ++m_id);
                m_min_re *= 0.90;
                m_max_re *= 0.90;
                m_min_im *= 0.90;
                m_max_im = m_min_im+(m_max_re-m_min_re)*m_height/m_width;
                m_re_factor = (m_max_re-m_min_re)/(m_width-1);
                m_im_factor = (m_max_im-m_min_im)/(m_height-1);
                //send(self, atom("config"), atom("small"));
            },
            on(atom("result"), arg_match) >> [=](uint32_t id, QByteArray ba) {
                if(ba.size() > 0) {
                    stringstream strstr;
                    strstr << "Received image with id: " << id << ".";
                    send(m_printer, strstr.str());
                    emit(setPixmapWithByteArray(ba));
                }
                else {
                    send(m_printer, "Received empty image.");
                }
                send(m_printer, "Set image");
            },
            on(atom("quit")) >> [=]() {
                quit();
            },
            on(atom("connect")) >> [=] {
                link_to(last_sender());
            },
            on(atom("DOWN"), exit_reason::normal) >> [=]() {
                send(m_printer, "Worker disconnected.");
            },
            on(atom("DOWN"), arg_match) >> [=](std::uint32_t err) {
                stringstream strstr;
                strstr << "[!!!] Worker disconnectd: " << err << endl;
                send(m_printer, strstr.str());
            },
            on(atom("EXIT"), exit_reason::normal) >> [=]() {
                send(m_printer, "Worker disconnected.");
            },
            on(atom("EXIT"), arg_match) >> [=](std::uint32_t err) {
                stringstream strstr;
                strstr << "[!!!] Worker disconnectd: " << err << endl;
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
            }
        );
}

server::server(actor_ptr printer, int width, int height, double min_real, double max_real, double min_imag, int iterations, ImageLabel* lbl)
        : m_printer(printer),
          m_id(0),
          m_done(false),
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
          m_iterations(iterations)
    {
        connect(this, SIGNAL(setPixmapWithByteArray(QByteArray)),
                lbl, SLOT(setPixmapFromByteArray(QByteArray)),
                Qt::QueuedConnection);
        send(self, atom("config"));
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

    int    width      =  1920;
    int    height     =  1200;
    double min_real   =  -2.0; //  2.0
    double max_real   =   1.0; //  1.0
    double min_imag   =  -1.0; // -1.2
    int iterations    =    50;

    QApplication app(argc, argv);
    QWidget window;
    Ui::Main main;
    main.setupUi(&window);

    auto server_actor = spawn<server>(printer, width, height, min_real, max_real, min_imag, iterations, main.imgLabel);

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
