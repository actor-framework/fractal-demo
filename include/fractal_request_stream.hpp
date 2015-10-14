#ifndef FRACTAL_REQUEST_STREAM_HPP
#define FRACTAL_REQUEST_STREAM_HPP

#include <vector>
#include <cstdint>
#include <functional>

#include "fractal_request.hpp"

class fractal_request_stream {
public:
  using predicate = std::function<bool (const fractal_request_stream*,
                                        const fractal_request&)>;

  using operation = std::function<void (const fractal_request_stream*,
                                        fractal_request&)>;

  inline float min_re() const {
    return min_re_;
  }

  inline float max_re() const {
    return max_re_;
  }

  inline float min_im() const {
    return min_im_;
  }

  inline float max_im() const {
    return max_im_;
  }

  inline float zoom() const {
    return zoom_;
  }

  void resize(std::uint32_t width, std::uint32_t height);

  void init(std::uint32_t width, std::uint32_t height, float min_re,
            float max_re, float min_im, float max_im,
            float zoom);

  const fractal_request& next();

  bool at_end() const;

  void loop_stack_mandelbrot();

  void loop_stack_burning_ship();

 private:
  void add_start_move(float frox_, float froy_, float to_x,
                      float to_y, int max_zoom);

  void add_move_froto_(float frox_, float froy_, float to_x,
                        float to_y, int max_zoom);

  void add_end_move(float frox_, float froy_, float to_x,
                    float to_y);

  void add_chain(std::vector<std::pair<float, float>>& chain,
                 int zoom);

  std::uint32_t width_;
  std::uint32_t height_;
  float min_re_;
  float max_re_;
  float min_im_;
  float max_im_;
  float zoom_;
  fractal_request freq_;
  std::vector<std::pair<operation, predicate>> operations_;
};

#endif // FRACTAL_REQUEST_STREAM_HPP

