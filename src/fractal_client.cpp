#include <map>
#include <chrono>
#include <time.h>
#include <vector>
#include <complex>
#include <cstdlib>
#include <sstream>
#include <iostream>

#include <QImage>
#include <QColor>
#include <QBuffer>
#include <QByteArray>

#include "cppa/cppa.hpp"
#include "cppa/match.hpp"

#include "client.hpp"
#include "fractal_cppa.hpp"
#include "q_byte_array_info.hpp"

using namespace cppa;
using namespace std;

void client::init() {
    trap_exit(true);
    become (
        on(atom("ping")) >> [=] {
            stringstream strstr;
            strstr << "[" << m_client_id << "] pong!";
            send(m_printer, strstr.str());
        },
        on(atom("next")) >> [=] {
            stringstream strstr;
            strstr << "[" << m_client_id << "] ";
            if(m_connected) {
                strstr << "Applying for work.";
                send(m_server, atom("enqueue"));
            }
            else {
                strstr << "Not connected to server.";
            }
            send(m_printer, strstr.str());
        },
        on(atom("quit")) >> [=]() {
            quit();
        },
        on(atom("init"), atom("assign"), arg_match) >> [=](uint32_t width, uint32_t height, long double min_re, long double max_re, long double min_im, long double max_im, uint32_t iterations, uint32_t id) {
            stringstream strstr;
            strstr << "[" << m_client_id << "] Received assignment, id: " << id <<".";
            auto re_factor = (max_re-min_re)/(width-1);
            auto im_factor = (max_im-min_im)/(height-1);
            if(iterations != m_iterations) {
                strstr << " Generating new colors.";
                m_iterations = iterations;
                m_palette.clear();
                m_palette.reserve(m_iterations+1);
                for(uint32_t i = 0; i < iterations; ++i) {
                    QColor tmp;
                    tmp.setHsv(((180.0/iterations)*i)+180.0, 255, 200);
                    m_palette.push_back(tmp);
                }
                m_palette.push_back(QColor(qRgb(0,0,0)));
            }
            send(m_printer, strstr.str());
            QImage image(width, height, QImage::Format_RGB32);
            for(int y = height-1; y >= 0; --y) {
                for(uint32_t x = 0; x < width; ++x) {
                    complex_d z = complex_d(min_re + x * re_factor, max_im - y * im_factor);
                    const complex_d constant = z;
                    uint32_t cnt = 0;
                    for(bool done = false; !done;) {
                        z = pow(z, 2) + constant;
                        cnt++;
                        if(cnt >= iterations || abs(z) > 2 ) { //|| norm(z) > 2) {
                            done = true;
                        }
                    };
                    image.setPixel(x,y,m_palette[cnt].rgb());
                }
            }
            QByteArray ba;
            QBuffer buf(&ba);
            buf.open(QIODevice::WriteOnly);
            image.save(&buf,"BMP");
            buf.close();
            reply(atom("result"), id, ba);
            send(this, atom("next"));
        },
        on(atom("connect"), arg_match) >> [=](const string& host, uint16_t port) {
            stringstream strstr;
            strstr << "[" << m_client_id << "] ";
            if(m_connected) {
                strstr << "You can only be connected to one server at a time!";
            }
            else {
                try {
                    strstr << "Connecting to a server at '" << host << ":" << port << "'.";
                    auto new_server = remote_actor(host, port);
                    m_server = new_server;
                    m_connected = true;
                    strstr << " Connection established.";
                    send(m_server, atom("connect"));
                }
                catch (network_error exc) {
                    strstr << "Could not connect: " << exc.what();
                }
            }
            send(m_printer, strstr.str());
        },
        on(atom("EXIT"), arg_match) >> [=](std::uint32_t err) {
            stringstream strstr;
            strstr << "[" << m_client_id << "] Server shut down: 0x" << hex << err << ".";
            send(m_printer, strstr.str());
            m_connected = false;
        },
        others() >> [=]() {
            stringstream strstr;
            strstr << "[" << m_client_id << "] Unexpected message: '" << to_string(last_dequeued()) << "'";
            send(m_printer, strstr.str());
        }
    );
}

client::client(actor_ptr printer, uint32_t client_id) : m_printer(printer), m_connected(false), m_client_id(client_id) { }

void print_usage(actor_ptr printer) {
    send(printer, "Usage: fractal_client\n -h, --host=\t\tserver host\n -p, --port=\t\tserver port\n -a, --actors=\t\tnumber of actors\n");
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
    bool has_actors = false;

    string host("localhost");
    uint16_t port(20283);
    uint32_t number_of_actors(1);

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
        },
        (on("-a", arg_match) || on(get_extractor("actors"))) >> [&](const string& input) -> bool {
            auto tmp = toint(input);
            if(tmp) {
                number_of_actors = *tmp;
                if(number_of_actors > 0) {
                    has_actors = true;
                    return true;
                }
                else {
                    send(printer, "Please choose a number greater than 0.");
                    return false;
                }
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
    vector<actor_ptr> running_actors;
    for(uint32_t i = 0; i < number_of_actors; ++i) {
        running_actors.push_back(spawn<client>(printer, i));
    }

    if(has_host && has_port) {
        stringstream strstr;
        strstr << "Connecting to '" << host << "' on port '" << port << "'.";
        send(printer, strstr.str());
        for(auto& a : running_actors) {
            send(a, atom("connect"), host, port);
            send(a, atom("next"));
        }
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
                    for(auto& a : running_actors) {
                        send(a, atom("connect"), host, port);
                    }
                },
                on("quit") >> [&] {
                    done = true;
                },
                on("ping") >> [&] {
                    for(auto& a : running_actors) {
                        send(a, atom("ping"));
                    }
                },
                on("next") >> [&] {
                    for(auto& a : running_actors) {
                        send(a, atom("next"));
                    }
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
    for(auto& a : running_actors) {
        send(a, atom("quit"));
    }
    send(printer, atom("quit"));
    await_all_others_done();
    shutdown();
    return 0;
}
