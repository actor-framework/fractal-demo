
#include <set>
#include <iterator>
#include <algorithm>

#include "include/controller.hpp"

using namespace std;
using namespace caf;

controller::controller(actor server)
: m_server(server)
, m_use_normal(0)
, m_use_opencl(0) { }

void controller::send_worker_config() {
  if (m_use_normal > m_normal.size() || m_use_opencl > m_opencl.size()) {
    aout(this) << "[!!!] only "  << m_normal.size()
               << " normal and " << m_opencl.size()
               << " workers known" << endl;
    return;
  }
  set<actor> workers;
  copy_n(m_normal.begin(), m_use_normal, inserter(workers, workers.end()));
  copy_n(m_opencl.begin(), m_use_opencl, inserter(workers, workers.end()));
  send(m_server, atom("workers"), move(workers));
}

behavior controller::make_behavior() {
  trap_exit(true);
  return {
    on(atom("widget"), arg_match) >> [=](const actor& widget){
      m_widget = widget;
      send(m_widget, atom("max_cpu"), m_normal.size());
      send(m_widget, atom("max_gpu"), m_opencl.size());
    },
    on(atom("add")) >> [=]{
      return atom("identity");
    },
    on(atom("normal"), arg_match) >> [=](const actor& worker){
      aout(this) << "add CPU-based worker" << endl;
      link_to(worker);
      m_normal.insert(worker);
      send(m_widget, atom("max_cpu"), m_normal.size());
    },
    on(atom("opencl"), arg_match) >> [=](const actor& worker){
      aout(this) << "add GPU-based worker" << endl;
      link_to(worker);
      m_opencl.insert(worker);
      send(m_widget, atom("max_gpu"), m_opencl.size());
    },
    on(atom("resize"), arg_match) >> [=](uint32_t, uint32_t) {
      forward_to(m_server);
    },
    on(atom("limit"), atom("normal"), arg_match) >> [=](uint32_t limit) {
      m_use_normal = limit;
      send_worker_config();
    },
    on(atom("limit"), atom("opencl"), arg_match) >> [=](uint32_t limit) {
      m_use_opencl = limit;
      send_worker_config();
    },
    on(atom("fps"), arg_match) >> [=](uint32_t) {
       forward_to(m_widget);
    },
    on(atom("changefrac"), arg_match) >> [=](atom_value frac_option) {
      send(m_server, atom("changefrac"), frac_option);
    },
    on(atom("EXIT"), arg_match) >> [=](const exit_msg& msg) {
      if (msg.source == m_widget) {
        aout(this) << "[!!!] GUI died" << endl;
      } else if (msg.source == m_server) {
        aout(this) << "[!!!] server died" << endl;
      }
      else {
        auto source = actor_cast<actor>(msg.source);
        if (m_normal.erase(source) > 0) {
          send(m_widget, atom("max_cpu"), m_normal.size());
        } else if (m_opencl.erase(source) > 0) {
          send(m_widget, atom("max_gpu"), m_opencl.size());
        } else {
          // unknown actor exited, ignore this message
          return;
        }
        send_worker_config();
      }
    },
    on(atom("quit")) >> [=]{
      quit();
    },
    others() >> [=]{
      aout(this) << "[!!!] controller received unexpected message: '"
           << to_string(last_dequeued())
           << "'." << endl;
    }
  };
}
