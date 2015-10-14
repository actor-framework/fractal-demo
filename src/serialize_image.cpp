#include "serialize_image.hpp"

#include "config.hpp"
#include "buffer.hpp"

std::vector<char> serialize_image(QImage image) {
  std::vector<char> result;
  buffer buf{result};
  buf.open(QIODevice::WriteOnly);
  image.save(&buf, image_format);
  buf.close();
  return result;
}
