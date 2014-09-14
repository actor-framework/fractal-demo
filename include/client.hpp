#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <vector>
#include <cstdlib>

#include <QColor>

#include "caf/all.hpp"

caf::actor spawn_opencl_client(uint32_t);

class client : public caf::event_based_actor {
 public:
  caf::behavior make_behavior() override;

 private:
  caf::actor          m_current_server;
  caf::atom_value     m_current_fractal_type;
  std::vector<QColor> m_palette;
};

#endif // CLIENT_HPP

