#include "buffer.hpp"

buffer::buffer(storage& dest, QObject* parent)
    : QIODevice(parent),
      wr_pos_(0),
      data_(dest) {
  // nop
}

bool buffer::open(OpenMode mode) {
  QIODevice::open(mode);
  return true;
}

void buffer::close() {
  // nop
}

qint64 buffer::readData(char* data, qint64 maxSize) {
  return maxSize;
}

qint64 buffer::writeData(const char* data, qint64 num_bytes) {
  auto last = data + static_cast<ptrdiff_t>(num_bytes);
  for (; data != last; ++data)
    data_.push_back(*data);
  return num_bytes;
}
