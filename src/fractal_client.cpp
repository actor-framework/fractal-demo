#include <map>
#include <chrono>
#include <time.h>
#include <vector>
#include <cstdlib>
#include <iostream>

#include <QImage>
#include <QColor>
#include <QBuffer>
#include <QByteArray>

#include "cppa/opt.hpp"
#include "cppa/cppa.hpp"
#include "cppa/opencl.hpp"

#include "fractal_cppa.hpp"
#include "fractal_client.hpp"
#include "q_byte_array_info.hpp"

using namespace std;
using namespace cppa;

#define STRINGIFY(A) #A
namespace { constexpr const char* kernel_source = R"__(
__kernel void mandelbrot(__global float* config,
                         __global int* output)
{
    unsigned iterations = config[0];
    unsigned width = config[1];
    unsigned height = config[2];

    float min_re = config[3];
    float max_re = config[4];
    float min_im = config[5];
    float max_im = config[6];

    float re_factor = (max_re-min_re)/(width-1);
    float im_factor = (max_im-min_im)/(height-1);

    unsigned x = get_global_id(0);
    unsigned y = get_global_id(1);
    float z_re = min_re + x*re_factor;
    float z_im = max_im - y*im_factor;
    float const_re = z_re;
    float const_im = z_im;
    unsigned cnt = 0;
    float cond = 0;
    do {
        float tmp_re = z_re;
        float tmp_im = z_im;
        z_re = ( tmp_re*tmp_re - tmp_im*tmp_im ) + const_re;
        z_im = ( 2 * tmp_re * tmp_im ) + const_im;
        cond = z_re*z_re + z_im*z_im;
        cnt ++;
    } while (cnt < iterations && cond <= 4.0f);
    output[x+y*width] = cnt;
}
)__"; }

void send_result_to(actor_ptr server, QImage &image, unsigned id) {
    QByteArray ba;
    QBuffer buf{&ba};
    buf.open(QIODevice::WriteOnly);
    image.save(&buf,"BMP");
    buf.close();
    send(server, atom("result"), id, ba);
}

void client::init() {
    become (
        on(atom("next")) >> [=] {
            sync_send(m_server, atom("enqueue")).then(
                on(atom("ack"), atom("enqueue")) >> [=] {
                    aout << m_prefix << "Enqueued for work.\n";
                },
                after(chrono::minutes(5)) >> [=] {
                    aout << m_prefix
                         << "Failed to equeue for new work. Goodbye.\n";
                    quit();
                }
            );
        },
        on(atom("quit")) >> [=]() {
            quit();
        },
        on(atom("assign"), arg_match) >> [=](uint32_t width,
                                             uint32_t height,
                                             long double min_re,
                                             long double max_re,
                                             long double min_im,
                                             long double max_im,
                                             uint32_t iterations,
                                             uint32_t id) {
            aout << m_prefix
                 << "Received assignment with id '"<< id << "'.\n";
            if (iterations != m_iterations) {
                aout << m_prefix << "Generating new colors.\n";
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
            if(m_with_opencl) {
                if (   m_current_width != width
                    || m_current_height != height
                    || !m_fractal) {
                    m_fractal = spawn_cl<vector<int>(vector<float>)>(m_program,
                                                                     "mandelbrot",
                                                                     {width, height});
                    m_current_width = width;
                    m_current_height = height;
                }
                m_current_id = id;
                vector<float> config;
                config.reserve(7);
                config.push_back(iterations);
                config.push_back(width);
                config.push_back(height);
                config.push_back(min_re);
                config.push_back(max_re);
                config.push_back(min_im);
                config.push_back(max_im);
                send(m_fractal, std::move(config));
                aout << m_prefix << "Sent work to opencl kernel.\n";
            }
            else {
                long double re_factor{(max_re-min_re)/(width-1)};
                long double im_factor{(max_im-min_im)/(height-1)};
                QImage image{static_cast<int>(width),
                             static_cast<int>(height),
                             QImage::Format_RGB32};
                for (unsigned y{0}; y < height; ++y) {
                    for (unsigned x{0}; x < width; ++x) {
                        float z_re = min_re + x*re_factor;
                        float z_im = max_im - y*im_factor;
                        float const_re = z_re;
                        float const_im = z_im;
                        unsigned cnt = 0;
                        float cond = 0;
                        do {
                            float tmp_re = z_re;
                            float tmp_im = z_im;
                            z_re = ( tmp_re*tmp_re - tmp_im*tmp_im ) + const_re;
                            z_im = ( 2 * tmp_re * tmp_im ) + const_im;
                            cond = z_re*z_re + z_im*z_im;
                            cnt ++;
                        } while (cnt < iterations && cond <= 4.0f);
                        image.setPixel(x,y,m_palette[cnt].rgb());
                    }
                }
                send_result_to(m_server, image, id);
                aout << m_prefix << "Sent image with id '"
                     << id << "' to server.\n";
                send(this, atom("next"));
            }
        },
        on_arg_match >> [&](const vector<int>& result) {
            aout << m_prefix << "Received result from gpu\n";
            QImage image{static_cast<int>(m_current_width),
                         static_cast<int>(m_current_height),
                         QImage::Format_RGB32};
            for (unsigned y{0}; y < m_current_height; ++y) {
                for (unsigned x{0}; x < m_current_width; ++x) {
                    image.setPixel(x,y,m_palette[result[x+y*m_current_width]].rgb());
                }
            }
            send_result_to(m_server, image, m_current_id);
            aout << m_prefix << "Sent image with id '"
                 << m_current_id << "' to server.\n";
            send(this, atom("next"));
        },
        others() >> [=]() {
            aout << m_prefix << "Unexpected message: '"
                 << to_string(last_dequeued()) << "'.\n";
        }
    );
}

client::client(actor_ptr server,
               uint32_t client_id,
               bool with_opencl,
               opencl::program& prog)
    : m_server{server}
    , m_client_id{client_id}
    , m_with_opencl{with_opencl}
    , m_program{prog}
    , m_current_id{0}
    , m_current_width{0}
    , m_current_height{0}
{
    stringstream strstr;
    strstr << "[" << m_client_id << "] ";
    m_prefix = move(strstr.str());
    send(m_server, atom("link"));
}


auto main(int argc, char* argv[]) -> int {
    announce(typeid(QByteArray), new q_byte_array_info);
    /*
    announce<complex_d>(make_pair(static_cast<complex_getter>(&complex_d::real),
                                  static_cast<complex_setter>(&complex_d::real)),
                        make_pair(static_cast<complex_getter>(&complex_d::imag),
                                  static_cast<complex_setter>(&complex_d::imag)));
    */
    announce<vector<float>>();
    announce<vector<int>>();

    cout.unsetf(std::ios_base::unitbuf);

    string host;
    uint16_t port{20283};
    uint32_t number_of_actors{1};
    bool with_opencl{false};

    options_description desc;
    bool args_valid = match_stream<string>(argv + 1, argv + argc) (
        on_opt1('H', "host", &desc,
                "set server host")
                >> rd_arg(host),
        on_opt1('p', "port", &desc,
                "set port (default: 20283)")
                >> rd_arg(port),
        on_opt1('a', "actors", &desc,
                "set number of actors started by this client")
                >> rd_arg(number_of_actors),
        on_opt0('o', "opencl", &desc,
                "run with opencl")
                >> [&] { with_opencl = true; },
        on_opt0('h', "help", &desc,
                "print help")
                >> print_desc_and_exit(&desc)
    );
    if (!args_valid || host.empty() || number_of_actors < 1) {
        print_desc_and_exit(&desc)();
    }

    aout << "Starting client(s). Connecting to '" << host
         << "' on port '" << port << "'.\n";

    if (with_opencl) {
        aout << "Using OpenCL kernel for calculations.\n";
    }
    auto server = remote_actor(host, port);
    auto prog = opencl::program::create(kernel_source);
    vector<actor_ptr> running_actors;
    for (uint32_t i{0}; i < number_of_actors; ++i) {
        running_actors.push_back(spawn<client>(server, i, with_opencl, prog));
    }

    aout << running_actors.size() << " worker(s) running.\n";
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
                    aout << "available commands:\n - /quit\n";
                }
            );
        }
    };
    for (auto& a : running_actors) {
        send(a, atom("quit"));
    }
    await_all_others_done();
    shutdown();
    return 0;
}
