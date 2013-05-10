#include "config.hpp"

extern const char* image_format = "PNG";
extern const char* image_file_ending = ".png";

const std::uint32_t default_width = 1024;
const std::uint32_t default_height = 768;
const std::uint32_t default_interval = 2000;
const std::uint32_t default_iterations = 500;
const std::uint32_t default_queuesize = 15;

const float_type default_min_real = -1.9; // must be <= 0.0
const float_type default_max_real = 1.0;  // must be >= 0.0
const float_type default_min_imag = -0.9; // must be <= 0.0
const float_type default_max_imag = default_min_imag
                                  + (default_max_real - default_min_real)
                                  * default_height
                                  / default_width;
const float_type default_zoom     = 0.9; // must be >= 0.0

