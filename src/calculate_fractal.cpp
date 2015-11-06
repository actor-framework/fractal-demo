#include "calculate_fractal.hpp"

#include <set>
#include <cmath>
#include <cassert>
#include <numeric>
#include <iostream>
#include <stdexcept>
#include <functional>

void calculate_palette(caf::atom_value fractal,
                       std::vector<QColor>& storage,
                       uint16_t iterations) {
  storage.clear();
  storage.reserve(iterations + 1);
  switch (static_cast<uint64_t>(fractal)) {
    case burnship_atom::uint_value():
      for (uint16_t i = 0; i < iterations; ++i) {
        QColor tmp;
        int color = ((iterations + 3000) / ((i + 1) * (i + 1)));
        tmp.setHsv(color, 255, 200);
        storage.push_back(tmp);
      }
      storage.push_back(QColor(qRgb(0,0,0)));
      break;
    default:
      for (uint16_t i = 0; i < iterations; ++i) {
        QColor tmp;
        tmp.setHsv(((180.0 / iterations) * i) + 180.0, 255, 200);
        storage.push_back(tmp);
      }
      storage.push_back(QColor(qRgb(0,0,0)));
  }
}

/// Substracts the int part from the number
double frac(double value) {
  return value - static_cast<unsigned>(value);
}

size_t map_count_to_color(int cnt, int min, int max, int num_colors,
                          double P, double Q) {
  auto cnt_d = static_cast<double>(cnt);
  auto min_d = static_cast<double>(min);
  auto max_d = static_cast<double>(max);
//  auto u = (cnt_d - min_d) / (max_d - min_d);                         // linear
//  auto u = (sqrt(cnt_d) - sqrt(min_d)) / (sqrt(max_d) - sqrt(min_d)); // sqrt
  auto u = (cbrt(cnt_d) - cbrt(min_d)) / (cbrt(max_d) - cbrt(min_d)); // cbrt
//  auto u = (log(cnt_d) - log(min_d)) / (log(max_d) - log(min_d));     // log
//  auto u = (log(cnt_d / min_d)) / (log(max_d / min_d));               // log
  u = frac(u * P + Q);
  return static_cast<size_t>(u * static_cast<double>(num_colors));
}

std::vector<int> cumulative_distribution(const std::vector<uint16_t>& counts) {
  std::vector<int> histogram(counts.size(), 0);
  for (size_t i = 0; i < counts.size(); ++i) {
    histogram[counts[i]] = histogram[counts[i]] + 1;
  }
  std::vector<int> distribution(counts.size());
  distribution[0] = histogram[0];
  for (size_t i = 1; i < histogram.size(); ++i) {
    distribution[i] = distribution[i-1] + histogram[i];
  }
  return distribution;
}

size_t map_count_to_color_historic(int cnt, int num_colors,
                                   const std::vector<int>& distribution,
                                   size_t total_pixles, double P, double Q) {
  auto u = static_cast<double>(distribution[cnt]) /
             static_cast<double>(total_pixles);
  u = frac(u * P + Q); // FRAC substracts the int part from the number
  return static_cast<size_t>(u * static_cast<double>(num_colors));
}

/// The config passed to kernels contains the following (as floats)
/// [0]: iterations -> mamyimum iterations to test for escape
/// [1]: width      -> totoal columns
/// [2]: height     -> total rows
/// [3]: min_re     -> minimum on real axis
/// [4]: max_re     -> maxmimum on real axis
/// [5]: min_im     -> minimum on imagnary axis
/// [6]: max_im     -> maxmimum on imagnary axis
/// [7]: offset     -> offset for calculation
/// [8]: rows       -> rows after offset
constexpr const char* mandelbrot_kernel = R"__(
  __kernel void calculate_fractal(__global float*    config,
                                  __global unsigned short* output) {
    unsigned offset = (unsigned) config[7];
    unsigned rows   = (unsigned) config[8];
    unsigned x = get_global_id(0);
    unsigned y = get_global_id(1);
    if (y >= offset && y < (offset + rows)) {
      unsigned iterations = (unsigned) config[0];
      unsigned width      = (unsigned) config[1];
      unsigned height     = (unsigned) config[2];
      float min_re = config[3];
      float max_re = config[4];
      float min_im = config[5];
      float max_im = config[6];
      float re_factor = (max_re - min_re) / (width - 1);
      float im_factor = (max_im - min_im) / (height - 1);
      float z_re = min_re + x * re_factor;
      float z_im = max_im - y * im_factor;
      float const_re = z_re;
      float const_im = z_im;
      unsigned short cnt = 0;
      float cond = 0;
      do {
        float tmp_re = z_re;
        float tmp_im = z_im;
        z_re = ( tmp_re * tmp_re - tmp_im * tmp_im ) + const_re;
        z_im = ( 2 * tmp_re * tmp_im ) + const_im;
        cond = z_re * z_re + z_im * z_im;
        ++cnt;
      } while (cnt < iterations && cond <= 4.0f);
      output[x + (y - offset) * width] = cnt;
    }
  }
)__";

// TODO: fix types and arguments
constexpr const char* tricorn_kernel = R"__(
  __kernel void calculate_fractal(__global float*    config,
                                  __global unsigned short* output) {
    unsigned offset = (unsigned) config[7];
    unsigned rows   = (unsigned) config[8];
    unsigned x = get_global_id(0);
    unsigned y = get_global_id(1);
    if (y >= offset && y < (offset + rows)) {
      unsigned iterations = config[0];
      unsigned width = config[1];
      unsigned height = config[2];
      float min_re = config[3];
      float max_re = config[4];
      float min_im = config[5];
      float max_im = config[6];
      float re_factor = (max_re - min_re) / (width - 1);
      float im_factor = (max_im - min_im) / (height - 1);
      float z_re = min_re + x * re_factor;
      float z_im = max_im - y * im_factor;
      float const_re = z_re;
      float const_im = z_im;
      unsigned cnt = 0;
      float cond = 0;
      do {
        float tmp_re = z_re;
        float tmp_im = z_im;
        z_re = ( tmp_re * tmp_re - tmp_im * tmp_im ) + const_re;
        z_im = -1 * ( 2 * tmp_re * tmp_im ) + const_im;
        cond = z_re * z_re + z_im * z_im;
        ++cnt;
      } while (cnt < iterations && cond <= 4.0f);
      output[x + (y - offset) * width] = cnt;
    }
  }
)__";

// TODO: fix types and arguments
constexpr const char* burnship_kernel = R"__(
  __kernel void calculate_fractal(__global float*    config,
                                  __global unsigned short* output) {
    unsigned offset = (unsigned) config[7];
    unsigned rows   = (unsigned) config[8];
    unsigned x = get_global_id(0);
    unsigned y = get_global_id(1);
    if (y >= offset && y < (offset + rows)) {
      unsigned iterations = config[0];
      unsigned width = config[1];
      unsigned height = config[2];
      float min_re = config[3];
      float max_re = config[4];
      float min_im = config[5];
      float max_im = config[6];
      float re_factor = (max_re - min_re) / (width - 1);
      float im_factor = (max_im - min_im) / (height - 1);
      unsigned x = get_global_id(0);
      unsigned y = get_global_id(1);
      float z_re = min_re + x * re_factor;
      float z_im = max_im - y * im_factor;
      float const_re = z_re;
      float const_im = z_im;
      unsigned cnt = 0;
      float cond = 0;
      do {
        auto tmp_re = z_re;
        auto tmp_im = z_im;
        z_re = ( tmp_re * tmp_re - tmp_im * tmp_im ) - const_re;
        z_im = ( 2 * abs(tmp_re * tmp_im) ) - const_im;
        cond = (abs(tmp_re) + abs(tmp_im)) * (abs(tmp_re) + abs(tmp_im));
        ++cnt;
      } while (cnt < iterations && cond <= 4.0f);
      output[x + (y - offset) * width] = cnt;
    }
  }
)__";

// TODO: fix types and arguments
constexpr const char* julia_kernel = R"__(
  __kernel void calculate_fractal(__global float*    config,
                                  __global unsigned short* output) {
    unsigned offset = (unsigned) config[7];
    unsigned rows   = (unsigned) config[8];
    unsigned x = get_global_id(0);
    unsigned y = get_global_id(1);
    if (y >= offset && y < (offset + rows)) {
      unsigned iterations = config[0];
      unsigned width  = config[1];
      unsigned height = config[2];
      float min_re = config[3];
      float max_re = config[4];
      float min_im = config[5];
      float max_im = config[6];
      float re_factor = (max_re - min_re) / (width  - 1);
      float im_factor = (max_im - min_im) / (height - 1);
      float z_re = min_re + x*re_factor;
      float z_im = max_im - y*im_factor;
      float const_re = z_re;
      float const_im = z_im;
      unsigned cnt = 0;
      float cond = 0;
      do {
        auto tmp_re = z_re;
        auto tmp_im = z_im;
        z_re = ( tmp_re * tmp_re - tmp_im*tmp_im ) - const_re;
        z_im = ( 2 * abs(tmp_re * tmp_im) ) - const_im;
        cond = (abs(tmp_re) + abs(tmp_im)) * (abs(tmp_re) + abs(tmp_im));
        ++cnt;
      } while (cnt < iterations && cond <= 4.0f);
      output[x + (y - offset) * width] = cnt;
    }
  }
)__";

const char* calculate_fractal_kernel(caf::atom_value fractal) {
  switch (static_cast<uint64_t>(fractal)) {
    case mandelbrot_atom::uint_value():
      return mandelbrot_kernel;
    case burnship_atom::uint_value():
      return burnship_kernel;
    case tricorn_atom::uint_value():
      return tricorn_kernel;
    case julia_atom::uint_value():
      return julia_kernel;
    default:
      throw std::invalid_argument("fractal is an unrecognized type");
  }
}

std::vector<uint16_t> calculate_fractal(caf::atom_value fractal,
                                        uint32_t width, uint32_t height,
                                        uint32_t offset, uint32_t rows,
                                        uint16_t max_iterations,
                                        float min_re, float max_re,
                                        float min_im, float max_im) {
  const auto min_y = offset;
  const auto max_y = min_y + rows;
  std::vector<uint16_t> result(width * rows);
  auto re_factor = (max_re - min_re) / (width - 1);
  auto ifactor_ = (max_im - min_im) / (height - 1);
  switch (static_cast<uint64_t>(fractal)) {
    case mandelbrot_atom::uint_value():
      for (uint32_t y = min_y; y < max_y; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
          auto z_re = min_re + x * re_factor;
          auto z_im = max_im - y * ifactor_;
          auto const_re = z_re;
          auto const_im = z_im;
          uint16_t iteration = 0;
          float cond = 0;
          do {
            auto tmp_re = z_re;
            auto tmp_im = z_im;
            z_re = (tmp_re * tmp_re - tmp_im * tmp_im) + const_re;
            z_im = (2 * tmp_re * tmp_im) + const_im;
            cond = z_re * z_re + z_im * z_im;
            ++iteration;
          } while (iteration < max_iterations && cond <= 4.0f);
          result[x + ((y - min_y) * width)] = iteration;
        }
      }
      break;
    case burnship_atom::uint_value():
      for (uint32_t y = min_y; y < max_y; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
          auto z_re = min_re + x * re_factor;
          auto z_im = max_im - y * ifactor_;
          auto const_re = z_re;
          auto const_im = z_im;
          uint16_t iteration = 0;
          float cond = 0;
          do {
            auto tmp_re = z_re;
            auto tmp_im = z_im;
            z_re = (tmp_re * tmp_re - tmp_im * tmp_im) - const_re;
            z_im = (2 * fabs(tmp_re * tmp_im)) - const_im;
            cond = (fabs(tmp_re) + fabs(tmp_im)) * (fabs(tmp_re) + fabs(tmp_im));
            ++iteration;
          } while (iteration < max_iterations && cond <= 4.0f);
          result[x + ((y - min_y) * width)] = iteration;
        }
      }
      break;
    case tricorn_atom::uint_value():
      for (uint32_t y = min_y; y < max_y; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
          auto z_re = min_re + x * re_factor;
          auto z_im = max_im - y * ifactor_;
          auto const_re = z_re;
          auto const_im = z_im;
          uint16_t iteration = 0;
          float cond = 0;
          do {
            auto tmp_re = z_re;
            auto tmp_im = z_im * -1;
            z_re = (tmp_re * tmp_re - tmp_im * tmp_im) + const_re;
            z_im = (2 * tmp_re * tmp_im + const_im);
            cond = z_re * z_re + z_im * z_im;
            ++iteration;
          } while (iteration < max_iterations && cond <= 4.0f);
          result[x + ((y - min_y) * width)] = iteration;
        }
      }
      break;
    case julia_atom::uint_value():
      for (uint32_t y = min_y; y < max_y; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
          auto z_re = min_re + x * re_factor;
          auto z_im = max_im - y * ifactor_;
          auto const_re = z_re;
          auto const_im = z_im;
          uint16_t iteration = 0;
          float cond = 0;
          do {
            auto tmp_re = z_re;
            auto tmp_im = z_im;
            z_re = (tmp_re * tmp_re - tmp_im * tmp_im) - const_re;
            z_im = (2 * fabs(tmp_re * tmp_im)) - const_im;
            cond = (fabs(tmp_re) + fabs(tmp_im)) * (fabs(tmp_re) + fabs(tmp_im));
            ++iteration;
          } while (iteration < max_iterations && cond <= 4.0f);
          result[x + ((y - min_y) * width)] = iteration;
        }
      }
      break;
    default:
      throw std::invalid_argument("fractal is an unrecognized type");
  }
  return result;
}
