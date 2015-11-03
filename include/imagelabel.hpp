#ifndef IMAGE_LABEL_HPP
#define IMAGE_LABEL_HPP

#include <vector>

#include <QImage>
#include <QLabel>
#include <QWidget>
#include <QResizeEvent>

class ImageLabel : public QLabel {
private:
  Q_OBJECT

public slots:
  /// Displays given fractal image.
  void setPixmap(uint32_t width, const std::vector<uint16_t>& fractal);

  /// Calculates the color palette for rendering fractal images.
  void newPalette(uint32_t iterations);

public:
  ImageLabel(QWidget* parent = nullptr, Qt::WindowFlags f = 0);

private:
  std::vector<QColor> palette_;
};

#endif
