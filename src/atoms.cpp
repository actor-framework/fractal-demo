#include "atoms.hpp"

bool valid_fractal_type(caf::atom_value value) {
  switch (static_cast<uint64_t>(value)) {
    case julia_atom::uint_value():
    case tricorn_atom::uint_value():
    case burnship_atom::uint_value():
    case mandelbrot_atom::uint_value():
      return true;
    default:
      return false;
  }
}
