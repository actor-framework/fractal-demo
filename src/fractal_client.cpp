#include <map>
#include <chrono>
#include <time.h>
#include <vector>
#include <complex>
#include <cstdlib>
#include <iostream>

#include <QImage>
#include <QColor>
#include <QBuffer>
#include <QByteArray>

#include "cppa/opt.hpp"
#include "cppa/cppa.hpp"

#include "fractal_cppa.hpp"
#include "fractal_client.hpp"
#include "q_byte_array_info.hpp"

using namespace cppa;
using namespace std;

void print_(ostream&) { }

template<typename Arg0, typename... Args>
void print_(ostream& out, Arg0&& arg1, Args&&... args) {
    print_(out << forward<Arg0>(arg1), forward<Args>(args)...);
}

template<typename... Args>
void print(const actor_ptr printer, Args&&... args) {
    stringstream out;
    print_(out, forward<Args>(args)...);
    send(printer, out.str());
}

void client::init() {
    become (
        on(atom("next")) >> [=] {
            send(m_printer, m_prefix+"Let's ask for some work.");
            sync_send(m_server, atom("enqueue")).then(
                on(atom("ack"), atom("enqueue")) >> [=] {
                    send(m_printer, m_prefix + "Enqueued for work.");
                },
                after(chrono::minutes(5)) >> [=] {
                    send(m_printer, m_prefix + "Failed to equeue for new work. Goodbye.");
                    quit();
                }
            );
        },
        on(atom("quit")) >> [=]() {
            quit();
        },
        on(atom("assign"), arg_match) >> [=](uint32_t width, uint32_t height, long double min_re, long double max_re, long double min_im, long double max_im, uint32_t iterations, uint32_t id) {
            print(m_printer, m_prefix, "Received assignment, id: ", id, ".");
//            auto re_factor = (max_re-min_re)/(width-1);
//            auto im_factor = (max_im-min_im)/(height-1);
            long double re_factor{(max_re-min_re)/(width-1)};
            long double im_factor{(max_im-min_im)/(height-1)};
            if (iterations != m_iterations) {
                send(m_printer, m_prefix + "Generating new colors.");
                m_iterations = iterations;
                m_palette.clear();
                m_palette.reserve(m_iterations+1);
                for (uint32_t i{0}; i < iterations; ++i) {
                    QColor tmp;
                    tmp.setHsv(((180.0/iterations)*i)+180.0, 255, 200);
                    m_palette.push_back(tmp);
                }
                m_palette.push_back(QColor(qRgb(0,0,0)));
            }
            QImage image{static_cast<int>(width), static_cast<int>(height), QImage::Format_RGB32};
            for (int y{static_cast<int>(height)-1}; y >= 0; --y) {
                for (uint32_t x{0}; x < width; ++x) {
                    complex_d z{min_re + x * re_factor, max_im - y * im_factor};
                    const complex_d constant{z};
                    uint32_t cnt{0};
                    for (bool done{false}; !done;) {
                        z = pow(z, 2) + constant;
                        cnt++;
                        if (cnt >= iterations || abs(z) > 2 ) {
                            done = true;
                        }
                    };
                    image.setPixel(x,y,m_palette[cnt].rgb());
                }
            }
            QByteArray ba;
            QBuffer buf{&ba};
            buf.open(QIODevice::WriteOnly);
            image.save(&buf,"BMP");
            buf.close();
            print(m_printer, m_prefix, "Sending image, id: ", id, ".");
            send(m_server, atom("result"), id, ba);
            send(this, atom("next"));
        },
        others() >> [=]() {
            print(m_printer, m_prefix, "Unexpected message: '",  to_string(last_dequeued()), "'.");
        }
    );
}

client::client(actor_ptr printer, actor_ptr server, uint32_t client_id) : m_server{server}, m_printer{printer}, m_client_id{client_id} {
    stringstream strstr;
    strstr << "[" << m_client_id << "] ";
    m_prefix = move(strstr.str());
    send(m_server, atom("link"));
}

void print_usage(actor_ptr printer) {
    send(printer, "Usage: fractal_client\n -h, --host=\t\tserver host\n -p, --port=\t\tserver port\n -a, --actors=\t\tnumber of actors\n");
}


auto main(int argc, char* argv[]) -> int {
    announce(typeid(QByteArray), new q_byte_array_info);
    announce<complex_d>(make_pair(static_cast<complex_getter>(&complex_d::real),
                                  static_cast<complex_setter>(&complex_d::real)),
                        make_pair(static_cast<complex_getter>(&complex_d::imag),
                                  static_cast<complex_setter>(&complex_d::imag)));

    cout.unsetf(std::ios_base::unitbuf);

    auto printer = spawn_printer();

    string host;
    uint16_t port{20283};
    uint32_t number_of_actors{1};

    options_description desc;
    bool args_valid = match_stream<string>(argv + 1, argv + argc) (
        on_opt1('H', "host", &desc, "set server host") >> rd_arg(host),
        on_opt1('p', "port", &desc, "set port (default: 20283)") >> rd_arg(port),
        on_opt1('a', "actors", &desc, "set number of actors started by this client") >> rd_arg(number_of_actors),
        on_opt0('h', "help", &desc, "print help") >> print_desc_and_exit(&desc)
    );
    if (!args_valid || host.empty()) print_desc_and_exit(&desc)();

    print(printer, "Starting client(s).\nConnecting to '", host, "' on port '", port, "'.");
    auto server = remote_actor(host, port);
    vector<actor_ptr> running_actors;
    for (uint32_t i{0}; i < number_of_actors; ++i) {
        running_actors.push_back(spawn<client>(printer, server, i));
    }

    print(printer, "Now I have ", running_actors.size(), " worker(s) running!\nTime to start working.");
    for (actor_ptr worker : running_actors) {
        send(worker, atom("next"));
    }

    for (bool done{false}; !done;){
        string input;
        getline(cin, input);
        if (input.size() > 0) {
            input.erase(input.begin());
            vector<string> values{split(input, ' ')};
            match (values) (
                on("quit") >> [&] {
                    done = true;
                },
                others() >> [&] {
                    send(printer, "available commands:\n - /quit");
                }
            );
        }
    };
    for (auto& a : running_actors) {
        send(a, atom("quit"));
    }
    send(printer, atom("quit"));
    await_all_others_done();
    shutdown();
    return 0;
}
