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

#include "client.hpp"
#include "config.hpp"
#include "fractal_request.hpp"
#include "q_byte_array_info.hpp"

#ifdef ENABLE_OPENCL
#include "cppa/opencl.hpp"
#endif // ENABLE_OPENCL

using namespace std;
using namespace cppa;

any_tuple response_from_image(QImage& image, uint32_t image_id) {
    QByteArray ba;
    QBuffer buf{&ba};
    buf.open(QIODevice::WriteOnly);
    image.save(&buf, image_format);
    buf.close();
    return make_any_tuple(atom("result"), image_id, std::move(ba));
}

void calculate_palette(vector<QColor>& storage, uint32_t iterations) {
    aout << "generating new colors" << endl;
    storage.clear();
    storage.reserve(iterations + 1);
    for (uint32_t i = 0; i < iterations; ++i) {
        QColor tmp;
        tmp.setHsv(((180.0 / iterations) * i) + 180.0, 255, 200);
        storage.push_back(tmp);
    }
    storage.push_back(QColor(qRgb(0,0,0)));
}

#ifdef ENABLE_OPENCL

namespace {

constexpr const char* kernel_source = R"__(
    __kernel void mandelbrot(__global float* config,
                             __global int* output)
    {
        uint32_t iterations = config[0];
        uint32_t width = config[1];
        uint32_t height = config[2];

        float min_re = config[3];
        float max_re = config[4];
        float min_im = config[5];
        float max_im = config[6];

        float re_factor = (max_re-min_re)/(width-1);
        float im_factor = (max_im-min_im)/(height-1);

        uint32_t x = get_global_id(0);
        uint32_t y = get_global_id(1);
        float z_re = min_re + x*re_factor;
        float z_im = max_im - y*im_factor;
        float const_re = z_re;
        float const_im = z_im;
        uint32_t cnt = 0;
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
)__";

} // namespace <anonymous>

using palette_ptr = shared_ptr<vector<QColor>>;

void clbroker(uint32_t clwidth, uint32_t clheight, uint32_t cliterations, actor_ptr clworker, palette_ptr palette) {
    become (
        on(atom("assert"), clwidth, clheight, cliterations, arg_match).when(gval(palette != nullptr) == true)
        >> [=](uint32_t image_id, float_type min_re, float_type max_re, float_type min_im, float_type max_im) {
            vector<float> cljob;
            cljob.reserve(7);
            cljob.push_back(cliterations);
            cljob.push_back(clwidth);
            cljob.push_back(clheight);
            cljob.push_back(min_re);
            cljob.push_back(max_re);
            cljob.push_back(min_im);
            cljob.push_back(max_im);
            auto hdl = self->make_response_handle();
            sync_send(clworker, std::move(cljob)).then (
                on_arg_match >> [&](const vector<int>& result) {
                    QImage image{static_cast<int>(clwidth),
                                 static_cast<int>(clheight),
                                 QImage::Format_RGB32};
                    for (uint32_t y = 0; y < clheight; ++y) {
                        for (uint32_t x = 0; x < clwidth; ++x) {
                            image.setPixel(x,y,(*palette)[result[x+y*clwidth]].rgb());
                        }
                    }
                    reply_tuple_to(hdl, response_from_image(image, image_id));
                }
            );
        },
        on(atom("assert"), arg_match)
        >> [=](uint32_t width, uint32_t height, uint32_t iterations, uint32_t, float_type, float_type, float_type, float_type) {
            palette_ptr ptr = palette;
            if (!ptr) ptr = make_shared<vector<QColor>>();
            calculate_palette(*ptr, iterations);
            auto w = spawn_cl<vector<int>(vector<float>)>(kernel_source, "mandelbrot",
                                                          {width, height});
            clbroker(width, height, iterations, w, ptr);
            // re-invoke message using the new behavior
            (self->bhvr_stack().back())(self->last_dequeued());
        }
    );
}

actor_ptr spawn_opencl_client() {
    return spawn(clbroker, 0, 0, 0, nullptr, nullptr);
}

#else

cppa::actor_ptr spawn_opencl_client() {
    throw std::logic_error("spawn_opencl_client: compiled wo/ OpenCL");
}

#endif // ENABLE_OPENCL

void client::init() {
    become (
        on(atom("quit")) >> [=] {
            quit();
        },
        on(atom("assign"), arg_match) >> [=](uint32_t width,
                                             uint32_t height,
                                             uint32_t iterations,
                                             uint32_t image_id,
                                             float_type min_re,
                                             float_type max_re,
                                             float_type min_im,
                                             float_type max_im) {
            if (m_palette.size() != (iterations + 1)) {
                aout << "generating new colors" << endl;
                m_palette.clear();
                m_palette.reserve(iterations + 1);
                for (uint32_t i = 0; i < iterations; ++i) {
                    QColor tmp;
                    tmp.setHsv(((180.0 / iterations) * i) + 180.0, 255, 200);
                    m_palette.push_back(tmp);
                }
                m_palette.push_back(QColor(qRgb(0,0,0)));
            }
            auto re_factor = (max_re - min_re) / (width - 1);
            auto im_factor = (max_im - min_im) / (height - 1);
            QImage image{static_cast<int>(width), static_cast<int>(height), QImage::Format_RGB32};
            for (uint32_t y = 0; y < height; ++y) {
                for (uint32_t x = 0; x < width; ++x) {
                    auto z_re = min_re + x*re_factor;
                    auto z_im = max_im - y*im_factor;
                    auto const_re = z_re;
                    auto const_im = z_im;
                    uint32_t iteration = 0;
                    float_type cond = 0;
                    do {
                        auto tmp_re = z_re;
                        auto tmp_im = z_im;
                        z_re = ( tmp_re*tmp_re - tmp_im*tmp_im ) + const_re;
                        z_im = ( 2 * tmp_re * tmp_im ) + const_im;
                        cond = z_re*z_re + z_im*z_im;
                        ++iteration;
                    } while (iteration < iterations && cond <= 4.0f);
                    image.setPixel(x,y,m_palette[iteration].rgb());
                }
            }
            reply_tuple(response_from_image(image, image_id));
        },
        others() >> [=]() {
            aout << "Unexpected message: '"
                 << to_string(last_dequeued()) << "'.\n";
        }
    );
}
