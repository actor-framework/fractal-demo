#include <set>
#include <iterator>
#include <algorithm>

#include "include/controller.hpp"

using namespace std;
using namespace caf;

controller::controller(actor server)
    : server_(server),
      use_normal_(0),
      use_opencl_(0) {
  // nop
}

void controller::send_worker_config() {
  if (use_normal_ > normal_.size() || use_opencl_ > opencl_.size()) {
    aout(this) << "[!!!] only "  << normal_.size()
               << " normal and " << opencl_.size()
               << " workers known" << endl;
    return;
  }
  set<actor> workers;
  copy_n(normal_.begin(), use_normal_, inserter(workers, workers.end()));
  copy_n(opencl_.begin(), use_opencl_, inserter(workers, workers.end()));
  send(server_, atom("SetWorkers"), move(workers));
}

behavior controller::make_behavior() {
  trap_exit(true);
  return {
    on(atom("widget"), arg_match) >> [=](const actor& widget){
      widget_ = widget;
      send(widget_, atom("max_cpu"), normal_.size());
      send(widget_, atom("max_gpu"), opencl_.size());
    },
    on(atom("add")) >> [=]{
      return atom("identity");
    },
    on(atom("normal"), arg_match) >> [=](const actor& worker){
      aout(this) << "add CPU-based worker" << endl;
      link_to(worker);
      normal_.insert(worker);
      send(widget_, atom("max_cpu"), normal_.size());
    },
    on(atom("opencl"), arg_match) >> [=](const actor& worker){
      aout(this) << "add GPU-based worker" << endl;
      link_to(worker);
      opencl_.insert(worker);
      send(widget_, atom("max_gpu"), opencl_.size());
    },
    on(atom("resize"), arg_match) >> [=](uint32_t, uint32_t) {
      forward_to(server_);
    },
    on(atom("limit"), atom("normal"), arg_match) >> [=](uint32_t limit) {
      use_normal_ = limit;
      send_worker_config();
    },
    on(atom("limit"), atom("opencl"), arg_match) >> [=](uint32_t limit) {
      use_opencl_ = limit;
      send_worker_config();
    },
    on(atom("fps"), arg_match) >> [=](uint32_t) {
       forward_to(widget_);
    },
    on(atom("changefrac"), arg_match) >> [=](atom_value frac_option) {
      send(server_, atom("changefrac"), frac_option);
    },
    [=](const exit_msg& msg) {
      if (msg.source == widget_) {
        aout(this) << "[!!!] GUI died" << endl;
      } else if (msg.source == server_) {
        aout(this) << "[!!!] server died" << endl;
      }
      else {
        auto source = actor_cast<actor>(msg.source);
        if (normal_.erase(source) > 0) {
          aout(this) << "A CPU worker died" << endl;
          send(widget_, atom("max_cpu"), normal_.size());
          send_worker_config();
        } else if (opencl_.erase(source) > 0) {
          aout(this) << "A GPU worker died" << endl;
          send(widget_, atom("max_gpu"), opencl_.size());
          send_worker_config();
        } else {
          // unknown link died, fall back to default behavior
          quit(msg.reason);
          return;
        }
      }
    },
    others >> [=]{
      aout(this) << "[!!!] controller received unexpected message: '"
           << to_string(current_message())
           << "'." << endl;
    }
  };
}
