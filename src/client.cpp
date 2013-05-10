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

using namespace std;
using namespace cppa;

namespace {

constexpr const char* kernel_source = R"__(
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
)__";

} // namespace <anonymous>

actor_ptr spawn_opencl_client() {
    // TODO
    return nullptr;
}

#endif // ENABLE_OPENCL

using namespace std;
using namespace cppa;

void reply_image(QImage& image, uint32_t image_id) {
    QByteArray ba;
    QBuffer buf{&ba};
    buf.open(QIODevice::WriteOnly);
    image.save(&buf, image_format);
//    image.save(&buf, "JPEG");
    buf.close();
    auto tup = make_any_tuple(atom("result"), image_id, std::move(ba));
    reply_tuple(tup);
}

void client::init() {
    become (
        on(atom("quit")) >> [=] {
            quit();
        },
        on(atom("assign"), arg_match) >> [=](uint32_t image_id,
                                             uint32_t width,
                                             uint32_t height,
                                             uint32_t iterations,
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
            reply_image(image, image_id);
        },
        others() >> [=]() {
            aout << "Unexpected message: '"
                 << to_string(last_dequeued()) << "'.\n";
        }
    );
}
