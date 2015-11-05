#include "config.hpp"

const char* image_format = "PNG";
const char* image_file_ending = ".png";
const char* default_port_range = "63000-63050";

const std::uint32_t default_width = 1920;
const std::uint32_t default_height = 1080;
const std::uint16_t default_iterations = 1000;

const float default_min_real = -1.9; // must be <= 0.0
const float default_max_real = 1.0;  // must be >= 0.0
const float default_min_imag = -0.9; // must be <= 0.0
const float default_max_imag = default_min_imag
                                  + (default_max_real - default_min_real)
                                  * default_height
                                  / default_width;
const float default_zoom     = 0.9; // must be >= 0.0

const int default_color = 180; // range from 0 to 359 (hsv)
