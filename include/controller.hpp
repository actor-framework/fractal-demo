#ifndef CONTROLLER_HPP
#define CONTROLLER_HPP

#include <set>

#include "caf/all.hpp"

class controller : public caf::event_based_actor {
 public:
  controller(caf::actor server);
  caf::behavior make_behavior() override;

 private:
  void send_worker_config();

  caf::actor m_server;
  caf::actor m_widget;
  std::uint32_t m_use_normal;
  std::uint32_t m_use_opencl;
  std::set<caf::actor> m_opencl;
  std::set<caf::actor> m_normal;
};

#endif // CONTROLLER_HPP

