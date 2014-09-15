#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <cstdint>

using float_type = float;

extern const char* image_format;
extern const char* image_file_ending;

extern const std::uint32_t default_width;
extern const std::uint32_t default_height;
extern const std::uint32_t default_iterations;

extern const float_type default_min_real;
extern const float_type default_max_real;
extern const float_type default_min_imag;
extern const float_type default_max_imag;
extern const float_type default_zoom;

#endif // CONFIG_HPP
