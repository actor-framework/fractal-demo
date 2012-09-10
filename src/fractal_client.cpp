#include <map>
#include <chrono>
#include <time.h>
#include <vector>
#include <complex>
#include <cstdlib>
#include <sstream>
#include <iostream>

#include <QImage>
#include <QBuffer>
#include <QByteArray>

#include "cppa/cppa.hpp"
#include "cppa/match.hpp"

#include "hdr/fractal_cppa.hpp"
#include "hdr/q_byte_array_info.hpp"

using namespace cppa;
using namespace std;

class client : public event_based_actor {

    actor_ptr m_server;
    actor_ptr m_printer;
    bool m_connected;

    uint32_t m_width;
    uint32_t m_height;

    double m_min_re;
    double m_max_re;
    double m_min_im;
    double m_max_im;

    double m_re_factor;
    double m_im_factor;

    uint32_t m_iterations;

    vector<QRgb> m_palette;

    void init() {
        become (
            on(atom("ping")) >> [=] {
                send(m_printer, "pong");
            },
            on(atom("next")) >> [=] {
                if(m_connected) {
                    send(m_printer, "Asking server for work.");
                    send(m_server, atom("enqueue"));
                }
            },
            on(atom("assign"), arg_match) >> [=](uint32_t id) {
                send(m_printer, "Got assingment.");
                QImage image(m_width, m_height, QImage::Format_RGB32);
                for(int y = m_height-1; y >= 0; --y) {
                    for(uint32_t x = 0; x < m_width; ++x) {
                        complex_d z = complex_d(m_min_re + x * m_re_factor, m_max_im - y * m_im_factor);
                        const complex_d constant = z;
                        uint32_t cnt = 0;
                        for(bool done = false; !done;) {
                            z = pow(z, 2) + constant;
                            cnt++;
                            if(cnt >= m_iterations || abs(z) > 2 ) { //|| norm(z) > 2) {
                                done = true;
                            }
                        };
                        image.setPixel(x,y,m_palette[cnt]);
                    }
                }
                send(m_printer, "Done, generating jpeg ...");
                QByteArray ba;
                QBuffer buf(&ba);
                buf.open(QIODevice::WriteOnly);
                image.save(&buf,"BMP");
                buf.close();
                send(m_printer, "... sending image.");
                reply(atom("result"), id, ba);
            },
            on(atom("quit")) >> [=]() {
                quit();
            },
            on(atom("initialize"), arg_match) >> [=](uint32_t width, uint32_t height, double min_re,double max_re, double min_im, double max_im, uint32_t iterations) {
                m_width  = width;
                m_height = height;
                m_min_re = min_re;
                m_max_re = max_re;
                m_min_im = min_im;
                m_max_im = max_im;
                m_re_factor = (m_max_re-m_min_re)/(m_width-1);
                m_im_factor = (m_max_im-m_min_im)/(m_height-1);
                if(iterations != m_iterations) {
                    m_iterations = iterations;
                    m_palette.clear();
                    m_palette.reserve(m_iterations+1);
                    for(uint32_t i = 0; i <= m_iterations; ++i) {
                        if(i < m_iterations/2) {
                            // m_palette.push_back(qRgb((255.0/(m_iterations/2.0))*i,0,0)); // black to red
                            m_palette.push_back(qRgb(0,0,(255.0/(m_iterations/2.0))*i)); // black to blue
                        }
                        else {
                            // m_palette.push_back(qRgb(255.0,
                            //                         (255.0/(m_iterations/2.0))*(i-(m_iterations/2.0)),
                            //                         (255.0/(m_iterations/2.0))*(i-(m_iterations/2.0)))); // red to white
                            m_palette.push_back(qRgb((255.0/(m_iterations/2.0))*(i-(m_iterations/2.0)),
                                                     (255.0/(m_iterations/2.0))*(i-(m_iterations/2.0)),
                                                     255.0)); // blue to white
                        }
                    }
                }
            },
            on(atom("init"), atom("assign"), arg_match) >> [=](uint32_t width, uint32_t height, double min_re,double max_re, double min_im, double max_im, uint32_t iterations, uint32_t id) {
                stringstream strstr;
                strstr << "Received init & assign. (id: " << id << ")";
                send(m_printer, strstr.str());
                m_width  = width;
                m_height = height;
                m_min_re = min_re;
                m_max_re = max_re;
                m_min_im = min_im;
                m_max_im = max_im;
                m_re_factor = (m_max_re-m_min_re)/(m_width-1);
                m_im_factor = (m_max_im-m_min_im)/(m_height-1);
                if(iterations != m_iterations) {
                    m_iterations = iterations;
                    //send(self, atom("config"));
                    m_palette.clear();
                    m_palette.reserve(m_iterations+1);
                    for(uint32_t i = 0; i < m_iterations; ++i) {
                        if(i < m_iterations/2) {
                            // m_palette.push_back(qRgb((255.0/(m_iterations/2.0))*i,0,0)); // black to red
                            m_palette.push_back(qRgb(0,0,(255.0/(m_iterations/2.0))*i)); // black to blue
                        }
                        else {
                            // m_palette.push_back(qRgb(255.0,
                            //                         (255.0/(m_iterations/2.0))*(i-(m_iterations/2.0)),
                            //                         (255.0/(m_iterations/2.0))*(i-(m_iterations/2.0)))); // red to white
//                            m_palette.push_back(qRgb((255.0/(m_iterations/2.0))*(i-(m_iterations/2.0)),
//                                                     (255.0/(m_iterations/2.0))*(i-(m_iterations/2.0)),
//                                                     255.0)); // blue to white
                            m_palette.push_back(qRgb((255.0/(m_iterations/2.0))*(i-(m_iterations/2.0)),
                                                     0,
                                                     255.0)); // blue to purple
                        }
                    }
                    m_palette.push_back(qRgb(0,0,0));
                }
                QImage image(m_width, m_height, QImage::Format_RGB32);
                for(int y = m_height-1; y >= 0; --y) {
                    for(uint32_t x = 0; x < m_width; ++x) {
                        complex_d z = complex_d(m_min_re + x * m_re_factor, m_max_im - y * m_im_factor);
                        const complex_d constant = z;
                        uint32_t cnt = 0;
                        for(bool done = false; !done;) {
                            z = pow(z, 2) + constant;
                            cnt++;
                            if(cnt >= m_iterations || abs(z) > 2 ) { //|| norm(z) > 2) {
                                done = true;
                            }
                        };
                        image.setPixel(x,y,m_palette[cnt]);
                    }
                }
                send(m_printer, "Generated fractal.");
                QByteArray ba;
                QBuffer buf(&ba);
                buf.open(QIODevice::WriteOnly);
                image.save(&buf,"BMP");
                buf.close();
                send(m_printer, "Sending.");
                reply(atom("result"), id, ba);
                send(this, atom("next"));
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
                       << "starting at (" << m_min_re + 0 * m_re_factor << "/" << m_max_im - (m_height-1) * m_im_factor << ")\n"
                       << "ending   at (" << m_min_re + (m_width-1) * m_re_factor << "/" << m_max_im - 0 * m_im_factor << ")\n"
                       << "-------\n"
                       << "iterations for clients: " << m_iterations << "\n"
                       << "·················································";
                send(m_printer, strstr.str());
            },
            on(atom("connect"), arg_match) >> [=](const string& host, uint16_t port) {
                if(m_connected) {
                    send(m_printer, "You can only be connected to one server at a time");
                }
                else {
                    try {
                        stringstream strstr;
                        strstr << "Connecting to a server at '" << host << ":" << port << "'.";
                        send(m_printer, strstr.str());
                        auto new_server = remote_actor(host, port);
                        m_server = new_server;
                        m_connected = true;
                        send(m_printer, "Connection established.");
                        send(m_server, atom("connect"));
                        trap_exit(true);
                    }
                    catch (network_error exc) {
                        send(m_printer, string("[!!!] Could not connect: ") + exc.what());
                    }
                }
                // send(self, atom("next"));
                // send(m_server, atom("enqueue"));
                // send(m_printer, "Waiting for work.");
            },
            on(atom("EXIT"), arg_match) >> [=](std::uint32_t err) {
                send(m_printer, "Server shut down!");
                m_connected = false;
            },
            others() >> [=]() {
                send(m_printer, string("[!!!] unexpected message: '") + to_string(last_dequeued()));
            }
        );
    }

 public:

    client(actor_ptr printer) : m_printer(printer), m_connected(false) { }

};

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

    send(printer, "Starting client.");

    auto client_actor = spawn<client>(printer);

    if(has_host && has_port) {
        stringstream strstr;
        strstr << "Connecting to '" << host << "' on port '" << port << "'.";
        send(printer, strstr.str());
        send(client_actor, atom("connect"), host, port);
        send(client_actor, atom("next"));
    }

    auto get_command = [&](){
        string input;
        bool done = false;
        getline(cin, input);
        if(input.size() > 0) {
            input.erase(input.begin());
            vector<string> values = split(input, ' ');
            match (values) (
                on("connect", val<string>, conv<uint16_t>) >> [&](const string& host, uint16_t port) {
                    send(client_actor, atom("connect"), host, port);
                },
                on("quit") >> [&] {
                    done = true;
                },
                on("ping") >> [&] {
                    send(client_actor, atom("ping"));
                },
                on("next") >> [&] {
                    send(client_actor, atom("next"));
                },
                others() >> [&] {
                    send(printer, "available commands:\n /connect HOST PORT\n /quit");
                }
            );
        }
        return !done;
    };
    auto running = get_command();
    while(running) {
        running = get_command();
    }
    send(client_actor, atom("quit"));


    send(printer, atom("quit"));
    await_all_others_done();
    shutdown();
    return 0;
}
