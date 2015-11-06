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

#ifndef FRACTAL_DEMO_ATOMS_HPP
#define FRACTAL_DEMO_ATOMS_HPP

#include "caf/atom.hpp"

using julia_atom = caf::atom_constant<caf::atom("julia")>;
using tricorn_atom = caf::atom_constant<caf::atom("tricorn")>;
using burnship_atom = caf::atom_constant<caf::atom("burnship")>;
using mandelbrot_atom = caf::atom_constant<caf::atom("mandelbrot")>;

bool valid_fractal_type(caf::atom_value value);

using config_server_atom = caf::atom_constant<caf::atom("ConfigServ")>;

using iterations_atom = caf::atom_constant<caf::atom("iterations")>;
using calc_weights_atom = caf::atom_constant<caf::atom("weights")>;
using tick_atom = caf::atom_constant<caf::atom("tick")>;
using init_atom = caf::atom_constant<caf::atom("init")>;

using resize_atom = caf::atom_constant<caf::atom("resize")>;
using max_cpu_atom = caf::atom_constant<caf::atom("max_cpu")>;
using max_gpu_atom = caf::atom_constant<caf::atom("max_gpu")>;
using fps_atom = caf::atom_constant<caf::atom("fps_atom")>;
using limit_atom = caf::atom_constant<caf::atom("limit")>;
using normal_atom = caf::atom_constant<caf::atom("normal")>;
using opencl_atom = caf::atom_constant<caf::atom("opencl")>;

#endif // FRACTAL_DEMO_ATOMS_HPP
