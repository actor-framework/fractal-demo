#ifndef CALCULATE_FRACTAL_HPP
#define CALCULATE_FRACTAL_HPP

#include <vector>

#include <QColor>

#include "config.hpp"
#include "atoms.hpp"

void calculate_palette(caf::atom_value fractal,
                       std::vector<QColor>& storage,
                       uint16_t iterations);

const char* calculate_fractal_kernel(caf::atom_value fractal);

std::vector<uint16_t> calculate_fractal(caf::atom_value fractal,
                                        uint32_t width, uint32_t height,
                                        uint16_t max_iterations,
                                        float min_re, float max_re,
                                        float min_im, float max_im);

#endif // CALCULATE_FRACTAL_HPP
