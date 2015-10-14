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

  caf::actor server_;
  caf::actor widget_;
  std::uint32_t use_normal_;
  std::uint32_t use_opencl_;
  std::set<caf::actor> opencl_;
  std::set<caf::actor> normal_;
};

#endif // CONTROLLER_HPP

