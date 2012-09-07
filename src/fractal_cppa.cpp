#include <vector>
#include <iostream>
#include <sstream>
#include <time.h>
#include <cstdlib>
#include <chrono>
#include <complex>
#include <map>

#include "cppa/cppa.hpp"
#include "cppa/match.hpp"


using namespace cppa;
using namespace std;

typedef complex<double> complex_d;

class server : public event_based_actor {

    vector<actor_ptr> m_available_workers;
    actor_ptr m_printer;

    bool m_done;

    complex_d m_power;
    complex_d m_constant;

    const int m_width;
    const int m_height;

    const double m_min_re;
    const double m_max_re;
    const double m_min_im;
    const double m_max_im;

    const double m_re_factor;
    const double m_im_factor;

    int m_x;
    int m_y;

//    map<pair<int, int>, char> m_image;

    int counter;

    void init() {
        become (
            on(atom("enqueue")) >> [=] {
                if(!m_done) {
                    complex_d tmp(m_min_re + m_x * m_re_factor, m_max_im - m_y * m_im_factor);
                    reply(atom("assign"),m_x, m_y, tmp, m_power, tmp /*m_constant*/);
                    ++m_x;
                    if(m_x >= m_width) {
                        m_x = 0;
                        --m_y;
                        if(m_y < 0) {
                            m_done = true;
                        }
//                        std::array<10, std::array<10>> m_1010[;
                    }
                }
                else {
                    send(m_printer, "Nothing to do!");
                    m_available_workers.push_back(last_sender());
                }
            },
            on(atom("result"), arg_match) >> [=](int x, int y, double res) {
                if(counter >= m_width) {
                    cout << endl;
                    counter = 0;
                }
                    cout << (res < 2.0 ? " " : "· ") << flush;
                counter++;
            },
            on(atom("quit")) >> [=]() {
                quit();
            },
            on(atom("connect")) >> [=] {
                monitor(last_sender());
            },
            on(atom("DOWN"), exit_reason::normal) >> [=]() {
                send(m_printer, "Worker disconnected.");
            },
            on(atom("DOWN"), arg_match) >> [=](std::uint32_t err) {
                stringstream strstr;
                strstr << "[!!!] Worker disconnectd: " << err << endl;
                send(m_printer, strstr.str());
            },
            others() >> [=]() {
                cout << "[!!!] unexpected message: '" << to_string(last_dequeued()) << "'." << endl;
            }
//            ,
//            after(std::chrono::milliseconds(50)) >> [=]() {
//                check if work and workers available ...
//            }

        );
    }

 public:

    server(actor_ptr printer, int width, int height, double min_real, double max_real, double min_imag)
        : m_printer(printer),
          m_done(false),
          m_width(width),
          m_height(height),
          m_power(2,0),
          m_constant(2,0),
          m_min_re(min_real),
          m_max_re(max_real),
          m_min_im(min_imag),
          m_max_im(min_imag+(max_real-min_real)*m_height/m_width),
          m_re_factor((m_max_re-m_min_re)/(m_width-1)),
          m_im_factor((m_max_im-m_min_im)/(m_height-1)),
          m_x(0),
          m_y(m_height-1),
          counter(0)
    {
        stringstream strstr;
        strstr << "_________________________________________________\n"
               << "stuff i calculated:\n"
               << "_________________________________________________\n"
               << "top left  (" << m_min_re << "/" << m_max_im << ")\n"
               << "top right (" << m_max_re << "/" << m_max_im << ")\n"
               << "bot left  (" << m_min_re << "/" << m_min_im << ")\n"
               << "bot right (" << m_max_re << "/" << m_min_im << ")\n"
               << "_________________________________________________\n"
               << "re_factor: " << m_re_factor << "\n"
               << "im_factor: " << m_im_factor << "\n"
               << "_________________________________________________\n"
               << "starting at (" << m_min_re + m_x * m_re_factor << "/" << m_max_im - m_y * m_im_factor << ")\n"
               << "ending   at (" << m_min_re + (m_width-1) * m_re_factor << "/" << m_max_im - 0 * m_im_factor << ")\n"
               << "_________________________________________________";
        send(m_printer, strstr.str());
    }

};

class client : public event_based_actor {

    actor_ptr m_server;
    actor_ptr m_printer;
    bool m_connected;

    int m_iterations;
    complex_d m_relax;

    void init() {
        become (
            on(atom("ping")) >> [=] {
                send(m_printer, "pong");
            },
            on(atom("assign"), arg_match) >> [=](int x, int y, complex_d base, complex_d power, complex_d constant) {
                stringstream strstr;
                strstr << "calculating pixel (" << x << "/" << y << "): base (" << base.real() << "/" << base.imag() << "), power (" << power.real() << "/" << power.imag() << "), constant (" << constant.real() << "/" << constant.imag() << ").";
                send(m_printer, strstr.str());
                complex_d z = base;
                int i = 0;
                for(bool done = false; !done;) {
                    z = pow(z, power) + constant;
                    i++;
                    if(i >= m_iterations || norm(z) > 2) {
                        done = true;
                    }
                };
                reply(atom("result"),x, y, norm(z));
                reply(atom("enqueue"));
            },
            on(atom("quit")) >> [=]() {
                quit();
            },
            on(atom("connect"), arg_match) >> [=](const string& host, uint16_t port) {
                if(m_connected) {
                    send(m_printer, "You cann only be connected to one server at a time");
                }
                else {
                    try {
                        stringstream strstr;
                        strstr << "Connecting to a server at '" << host << ":" << port << "'.";
                        send(m_printer, strstr.str());
                        auto new_server = remote_actor(host, port);
                        m_server = new_server;
                        monitor(new_server);
                        m_connected = true;
                        send(m_printer, "Connection established.");
                        send(m_server, atom("connect"));
                        send(m_printer, "Waiting for work.");
                        send(m_server, atom("enqueue"));
                    }
                    catch (network_error exc) {
                        send(m_printer, string("[!!!] Could not connect: ") + exc.what());
                    }
                }
            },
            on(atom("DOWN"), arg_match) >> [=](std::uint32_t err) {
                send(m_printer, "Server shut down!");
                m_connected = false;
            },
            others() >> [=]() {
                send(m_printer, string("[!!!] unexpected message: '") + to_string(last_dequeued()));
            }
        );
    }

 public:

            client(actor_ptr printer) : m_printer(printer), m_connected(false), m_iterations(50), m_relax(2.5,0) { }

};

class print_actor : public event_based_actor {
    void init() {
        become (
            on(atom("quit")) >> [] {
                self->quit();
            },
            on_arg_match >> [](const string& str) {
                cout << (str + "\n");
            }
        );
    }
};

inline vector<std::string> split(const string& str, char delim) {
    vector<std::string> result;
    stringstream strs{str};
    string tmp;
    while (std::getline(strs, tmp, delim)) result.push_back(tmp);
    return result;
}

template<typename T>
auto conv(const string& str) -> option<T> {
    T result;
    if (istringstream(str) >> result) return result;
    return {};
}

void print_usage(actor_ptr printer) {
    send(printer, "blub");
}

std::function<option<string> (const string&)> get_extractor(const string& identifier) {
    auto tmp = [&](const string& kvp) -> option<string> {
        auto vec = split(kvp, '=');
        if (vec.size() == 2) {
            if (vec.front() == "--"+identifier) {
                return vec.back();
            }
        }
        return {};
    };
    return tmp;
}


typedef double (complex_d::*complex_getter)() const;
typedef void   (complex_d::*complex_setter)(double);


auto main(int argc, char* argv[]) -> int {

    announce<complex_d>(make_pair(static_cast<complex_getter>(&complex_d::real),
                                  static_cast<complex_setter>(&complex_d::real)),
                        make_pair(static_cast<complex_getter>(&complex_d::imag),
                                  static_cast<complex_setter>(&complex_d::imag)));

    vector<string> args(argv + 1, argv + argc);

    cout.unsetf(std::ios_base::unitbuf);

    auto printer = spawn<print_actor>();

    bool has_type = false;
    bool has_host = false;
    bool has_port = false;

    string type;
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
        (on("-t", arg_match) || on(get_extractor("type"))) >> [&](const string& input) -> bool {
            if(input == "c" || input == "client") {
                type = "c";
                type = input;
                has_type = true;
                return true;
            }
            else if(input == "s" || input == "server") {
                type = "s";
                type = input;
                has_type = true;
                return true;
            }
            else {
                send(printer, "not a valid type");
                return false;
            }
        },
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

    if(!success || !has_type) {
        print_usage(printer);
        send(printer, atom("quit"));
        await_all_others_done();
        shutdown();
        return 0;
    }

    if(type == "s") { // server
        if(!has_port) {
            srand(time(NULL));
            send(printer, "No port was given, randomness 4tw!");
            port = (rand()%(static_cast<int>(numeric_limits<uint16_t>::max()) - 1024))+1024;
        }

        int    width      =  130;
        int    height     =   90;
        double min_real   =  -2.0;
        double max_real   =   1.0;
        double min_imag   =  -1.2;

        auto server_actor = spawn<server>(printer, width, height, min_real, max_real, min_imag);
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
        for(bool done = false; !done; ) {
            string input;
            getline(cin, input);
            if(input.size() > 0) {
                input.erase(input.begin());
                vector<string> values = split(input, ' ');
                match (values) (
                    on("quit") >> [&] {
                        done = true;
                    },
                    others() >> [&] {
                        send(printer, "available commands:\n /quit\n");
                    }
                );
            }
        }
        send(server_actor, atom("quit"));
    } // server

    else { // is_client <=> !is_server
        send(printer, "Starting client.");

        auto client_actor = spawn<client>(printer);

        if(has_host && has_port) {
            stringstream strstr;
            strstr << "Connecting to '" << host << "' on port '" << port << "'.";
            send(printer, strstr.str());
            send(client_actor, atom("connect"), host, port);
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
    } // client

    send(printer, atom("quit"));
    await_all_others_done();
    shutdown();
    return 0;
}
