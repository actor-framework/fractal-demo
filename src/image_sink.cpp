#include "image_sink.hpp"

#include <future>
#include <thread>
#include <memory>

#include <QMainWindow>
#include <QApplication>

#include "ui_main.h"
#include "config.hpp"
#include "mainwidget.hpp"

image_sink make_gui_sink(int argc, char** argv, uint32_t iterations) {
  auto promise = std::make_shared<std::promise<image_sink>>();
  auto f = promise->get_future();
  std::thread{[=] {
    int ac = argc;
    QApplication app{ac, argv};
    QMainWindow window;
    Ui::Main main;
    main.setupUi(&window);
    window.resize(default_width, default_height);
    main.mainWidget->image_label()->newPalette(iterations, default_color);
    promise->set_value(caf::actor_cast<image_sink>(main.mainWidget->as_actor()));
    window.show();
    app.quitOnLastWindowClosed();
    app.exec();
  }}.detach();
  return f.get();
}

image_sink make_file_sink(uint32_t init_iterations) {
  struct sink_state {
    uint32_t iterations;
    int image_nr = 0;
    std::vector<QColor> palette;
  };
  return caf::spawn([=](image_sink::stateful_pointer<sink_state> self)
                    -> image_sink::behavior_type {
    self->state.iterations = init_iterations;
    calculate_color_palette(self->state.palette, init_iterations,
                            default_color);
    return [=](uint32_t width, const std::vector<uint16_t>& frac) {
      auto image = image_from_fractal(width, frac, self->state.palette);
      image.save(QString("fractal-%1.png").arg(self->state.image_nr++));
    };
  });
}

QImage image_from_fractal(uint32_t width,
                          const std::vector<uint16_t>& frac,
                          const std::vector<QColor>& palette) {
  QImage image(static_cast<int>(width), static_cast<int>(frac.size() / width),
               QImage::Format_RGB32);
  size_t idx = 0;
  for (size_t y = 0; y < frac.size() / width; ++y)
    for (size_t x = 0; x < width; ++x)
      image.setPixel(x, y, palette[frac[idx++]].rgb());
  return image;
}

void calculate_color_palette(std::vector<QColor>& res, uint16_t iterations,
                             int color) {
  res.clear();
  res.reserve(iterations + 1);
  for (int i = 0; i < static_cast<int>(iterations); ++i)
    res.emplace_back(
          QColor::fromHsv(((static_cast<float>(color) / iterations) * i)
                          + static_cast<float>(color), 255, 200));
  res.emplace_back(QColor::fromRgb(0, 0, 0));
}
