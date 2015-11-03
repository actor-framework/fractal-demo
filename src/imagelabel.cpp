#include <iostream>

#include <QImage>
#include <QBuffer>
#include <QPixmap>

#include "config.hpp"
#include "imagelabel.hpp"
#include "image_sink.hpp"

using storage = std::vector<uint16_t>;
using palette = std::vector<QColor>;

void ImageLabel::setPixmap(uint32_t width, const std::vector<uint16_t>& frac) {
  QLabel::setPixmap(QPixmap::fromImage(image_from_fractal(width, frac,
                                                          palette_)));
}

void ImageLabel::newPalette(uint32_t iterations) {
  calculate_color_palette(palette_, iterations);
}

ImageLabel::ImageLabel(QWidget* parent, Qt::WindowFlags f) : QLabel(parent, f) {
  // nop
}
