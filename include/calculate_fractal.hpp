#ifndef CALCULATE_FRACTAL_HPP
#define CALCULATE_FRACTAL_HPP

#include <vector>

#include <QColor>
#include <QImage>

inline void calculate_palette(std::vector<QColor>& storage, uint32_t iterations) {
    // generating new colors
    storage.clear();
    storage.reserve(iterations + 1);
    for (uint32_t i = 0; i < iterations; ++i) {
        QColor tmp;
        tmp.setHsv(((180.0 / iterations) * i) + 180.0, 255, 200);
        storage.push_back(tmp);
    }
    storage.push_back(QColor(qRgb(0,0,0)));
}

template<typename FloatType>
QImage calculate_palette(std::vector<QColor>& palette,
                         uint32_t width, uint32_t height, uint32_t iterations,
                         FloatType min_re, FloatType max_re,
                         FloatType min_im, FloatType max_im) {
    if (palette.size() != (iterations + 1)) calculate_palette(palette, iterations);
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
            image.setPixel(x,y,palette[iteration].rgb());
        }
    }
    return image;
}

#endif // CALCULATE_FRACTAL_HPP
