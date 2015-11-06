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
                                        uint32_t offset, uint32_t rows,
                                        uint16_t max_iterations,
                                        float min_re, float max_re,
                                        float min_im, float max_im);

/// calculates the index in the color palette from
/// cnt: number of iterations for the index
/// min: minimum number of iterations needed
/// max: maximum number of iterations needed
/// num_colors: number of available colors
/// P: periodicity, number of pallette repetitions
/// Q: phase shift
size_t map_count_to_color(int cnt, int min, int max, int num_colors,
                          double P = 5.0, double Q = 0.2);

/// Calculate a comulative distribution for counts of a mandelbrot set
std::vector<int> cumulative_distribution(const std::vector<uint16_t>& counts);

/// calculates the index in the color palette using a histogram mapping from
/// cnt: number of iterations for the index
/// num_colors: number of available colors
/// distribution: the comulative distribution of all counts in the frame
/// total_pixels: the nuber of pixels in the frame
/// P: periodicity, number of pallette repetitions
/// Q: phase shift
size_t map_count_to_color_historic(int cnt, int num_colors,
                                   const std::vector<int>& distribution,
                                   size_t total_pixles,
                                   double P = 3.0, double Q = 0.2);
#endif // CALCULATE_FRACTAL_HPP
