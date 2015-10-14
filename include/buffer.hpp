#include <vector>

#include <QIODevice>

class buffer : public QIODevice {
public:
  using storage = std::vector<char>;

  buffer(storage& dest, QObject* parent = nullptr);

  bool open(OpenMode mode) override;

  void close() override;

protected:
  qint64 readData(char* data, qint64 maxSize) override;

  qint64 writeData(const char* data, qint64 num_bytes) override;

private:
 qint64 wr_pos_;
 std::vector<char>& data_;
};
