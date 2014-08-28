#ifndef FRACTAL_REQUEST_STREAM_HPP
#define FRACTAL_REQUEST_STREAM_HPP

#include <vector>
#include <cstdint>
#include <functional>

#include "fractal_request.hpp"

class fractal_request_stream {

    using predicate = std::function<bool (const fractal_request_stream*,
                                          const fractal_request&)>;

    using operation = std::function<void (const fractal_request_stream*,
                                          fractal_request&)>;

 public:

    inline float_type min_re() const { return m_min_re; }
    inline float_type max_re() const { return m_max_re; }
    inline float_type min_im() const { return m_min_im; }
    inline float_type max_im() const { return m_max_im; }
    inline float_type zoom()   const { return m_zoom;   }

    inline const fractal_request& request() const { return m_freq; }

    void resize(std::uint32_t width, std::uint32_t height);

    void init(std::uint32_t width,
              std::uint32_t height,
              float_type    min_re,
              float_type    max_re,
              float_type    min_im,
              float_type    max_im,
              float_type    zoom);

    // false if stream is done
    bool next();

    inline bool at_end() const { return m_operations.empty(); }

    void loop_stack_mandelbrot();
    void loop_stack_burning_ship();

 private:

    std::uint32_t   m_width;
    std::uint32_t   m_height;
    float_type      m_min_re;
    float_type      m_max_re;
    float_type      m_min_im;
    float_type      m_max_im;
    float_type      m_zoom;
    fractal_request m_freq;

    std::vector<std::pair<operation,predicate>> m_operations;

    void add_start_move(float_type from_x, float_type from_y, float_type to_x, float_type to_y, int max_zoom);
    void add_move_from_to(float_type from_x, float_type from_y, float_type to_x, float_type to_y, int max_zoom);
    void add_end_move(float_type from_x, float_type from_y, float_type to_x, float_type to_y);
    void add_chain(std::vector<std::pair<float_type, float_type>>& chain, int zoom);

};

#endif // FRACTAL_REQUEST_STREAM_HPP

