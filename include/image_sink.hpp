#ifndef IMAGE_SINK_HPP
#define IMAGE_SINK_HPP

#include <vector>
#include <cstdint>

#include <QColor>
#include <QImage>

#include "caf/all.hpp"

/// An image sink receives fractal images in order and
/// either displays them to the user or stores them to disk.
using image_sink =
  caf::typed_actor<caf::reacts_to<uint32_t /* width */,
                                  std::vector<uint16_t> /* fractal */>>;

QImage image_from_fractal(uint32_t width,
                          const std::vector<uint16_t>& frac,
                          const std::vector<QColor>& palette);

void calculate_color_palette(std::vector<QColor>& storage, uint16_t iterations);

/// Creates a GUI frontend for the user for displaying fractals.
image_sink make_gui_sink(int argc, char** argv, uint32_t iterations);

/// Creates a dummy sink writing all received images to disk.
image_sink make_file_sink(uint32_t iterations);

#endif // IMAGE_SINK_HPP
