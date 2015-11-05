/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <tuple>
#include <vector>
#include <cstdint>

extern const char* image_format;
extern const char* image_file_ending;
extern const char* default_port_range;

extern const std::uint32_t default_width;
extern const std::uint32_t default_height;
extern const std::uint16_t default_iterations;

extern const float default_min_real;
extern const float default_max_real;
extern const float default_min_imag;
extern const float default_max_imag;
extern const float default_zoom;

extern const int default_color;

using fractal_result = std::tuple<std::vector<uint16_t>, // fractal result
                                  uint32_t>;             // image ID

#endif // CONFIG_HPP
