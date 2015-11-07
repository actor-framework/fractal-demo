#include <unistd.h>

#include "caf/all.hpp"
#include "caf/io/all.hpp"
#include "caf/riac/all.hpp"

// fractal-demo extensions to CAF
#include "caf/cec.hpp"
#include "caf/maybe.hpp"
#include "caf/policy/work_sharing.hpp"

//#include "caf/opencl/spawn_cl.hpp"

#include "caf/experimental/whereis.hpp"
#include "caf/experimental/announce_actor_type.hpp"


#include "atoms.hpp"

#include "server.hpp"
#include "utility.hpp"
#include "controller.hpp"
#include "image_sink.hpp"

#include "projection.hpp"
#include "client_actor.hpp"
#include "calculate_fractal.hpp"

#include "ui_controller.h"

using namespace caf;
using namespace caf::experimental;

// from utility, allows "explode(...) | map(...) | ..."
using namespace container_operators;

using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::vector;

/*spawn_result make_gpu_worker(message args) {
  spawn_result result;
  // pre-pocessesing must be adjusted to the message sent to the actor
  auto pre_process = [](message& msg) -> maybe<message> {
    return msg.apply(
      [](uint16_t iterations, uint32_t width, uint32_t height,
         uint32_t offset, uint32_t rows,
         float min_re, float max_re, float min_im, float max_im) {
        std::vector<float> config{
          static_cast<float>(iterations), // 0
          static_cast<float>(width),      // 1
          static_cast<float>(height),     // 2
          static_cast<float>(min_re),     // 3
          static_cast<float>(max_re),     // 4
          static_cast<float>(min_im),     // 5
          static_cast<float>(max_im),     // 6
          static_cast<float>(offset),     // 7
          static_cast<float>(rows)        // 8
        };
        return make_message(std::move(config));
      }
    );
  };
  // post-processing ensures the format (slice, offset)
  auto post_process = [] (std::vector<float>&    config,
                          std::vector<uint16_t>& mandelbrot) -> message {
    return make_message(move(mandelbrot), static_cast<uint32_t>(config[7]));
  };
  // size is calculated to fit the number of rows specified in the config
  auto get_size = [](const std::vector<float>& config) {
    auto columns = config[1];
    auto rows    = config[8];
    return static_cast<size_t>(columns * rows);
  };
  args.apply({
    [&](atom_value fractal, uint32_t device_id) {
      if (! valid_fractal_type(fractal))
        return;
      auto w = spawn_cl(opencl::program::create(calculate_fractal_kernel(fractal), nullptr, device_id),
                        "calculate_fractal",
                        opencl::spawn_config(opencl::dim_vec{default_width,
                                                             default_height}),
                        pre_process, post_process,
                        opencl::in_out<float*>{}, opencl::out<uint16_t*>{get_size});
      result.first = w.address();
    }
  });
  return result;
}*/

struct cpu_worker_state {
  const char* name = "CPU worker";
};

behavior cpu_worker(stateful_actor<cpu_worker_state>* self,
                    atom_value fractal) {
  aout(self) << "spawned new CPU worker" << endl;
  if (! valid_fractal_type(fractal))
    return {};
  return {
    [=](uint16_t iterations,
        uint32_t width, uint32_t height, uint32_t offset, uint32_t rows,
        float min_re, float max_re, float min_im, float max_im) -> fractal_result {
      return std::make_tuple(calculate_fractal(fractal, width, height,
                                               offset, rows, iterations,
                                               min_re, max_re, min_im, max_im),
                             offset);
    }
  };
}

template <class F, class Atom, class... Ts>
maybe<uint16_t> x_in_range(uint16_t min_port, uint16_t max_port,
                F fun, Atom op, const Ts&... xs) {
  scoped_actor self;
  auto mm = io::get_middleman_actor();
  auto success = false;
  for (uint16_t port = min_port; port < max_port && ! success; ++port) {
    success = true;
    self->sync_send(mm, op, port, xs...).await(
      fun,
      [&](error_atom, const string&) {
        success = false;
      }
    );
    if (success)
      return port;
  }
  return none;
}

maybe<uint16_t> open_port_in_range(uint16_t min_port, uint16_t max_port) {
  auto f = [](ok_atom, uint16_t actual_port) {
    std::cout << "opened port " << actual_port << endl;
  };
  return x_in_range(min_port, max_port, f, open_atom::value, "", false);
}

maybe<uint16_t> publish_in_range(uint16_t min_port, uint16_t max_port, actor whom) {
  auto f = [](ok_atom, uint16_t actual_port) {
    std::cout << "published actor on port " << actual_port << endl;
  };
  return x_in_range(min_port, max_port, f, publish_atom::value,
                    whom.address(), std::set<string>{}, "", false);
}

std::pair<node_id, actor_addr> connect_node(const string& node, uint16_t port) {
  std::pair<node_id, actor_addr> result;
  scoped_actor self;
  auto mm = io::get_middleman_actor();
  self->sync_send(mm, connect_atom::value, node, port).await(
    [&](ok_atom, node_id& nid, actor_addr& aid, std::set<string>&) {
      if (! is_invalid(nid))
        result = std::make_pair(std::move(nid), std::move(aid));
    },
    [&](error_atom, const string& errstr) {
      cerr << "error: " << errstr << endl;
    });
  if (is_invalid(result.first))
    cerr << "cannot connect to " << node << " on port " << port << endl;
  return result;
}

std::pair<node_id, actor_addr>
connect_node(const maybe<std::pair<string, string>>& x) {
  if (! x)
    return {};
  auto port = to_u16(x->second);
  if (! port)
    return {};
  return connect_node(x->first, *port);
}

int controller(int argc, char** argv, const string& client_node,
               uint16_t client_port) {
  auto ctrl = actor_cast<actor>(connect_node(client_node, client_port).second);
  QApplication app{argc, argv};
  QMainWindow window;
  Ui::Controller ctrl_ui;
  ctrl_ui.setupUi(&window);
  auto ctrl_widget = ctrl_ui.controllerWidget;
  ctrl_widget->set_controller(ctrl);
  ctrl_widget->initialize();
  send_as(ctrl_widget->as_actor(), ctrl, atom("widget"),
          ctrl_widget->as_actor());
  window.show();
  app.quitOnLastWindowClosed();
  return app.exec();
}

template <class... Ts>
maybe<actor> remote_spawn(node_id target, string name, Ts&&... xs) {
  maybe<actor> result = actor{invalid_actor};
  scoped_actor self;
  auto mm = io::get_middleman_actor();
  self->sync_send(mm, spawn_atom::value, target, name,
                  make_message(std::forward<Ts>(xs)...)).await(
    [&](ok_atom, const actor_addr& addr, const std::set<string>& ifs) {
      if (ifs.empty())
        result = actor_cast<actor>(addr);
      else
        result = std::make_error_condition(cec::expected_dynamically_typed_remote_actor);
    },
    [&](error_atom, const string& errmsg) {
      if (errmsg == "no connection to requested node")
        result = std::make_error_condition(cec::remote_node_unreachable);
      else
        result = std::make_error_condition(cec::unknown_error);
    }
  );
  if (result)
    cout << "got worker: " << to_string(*result) << " via "
         << to_string(self.address()) << endl;
  return result;
}

int client(int, char**, const vector<node_id>& nodes,
           atom_value fractal_type, image_sink sink) {
  if (nodes.empty()) {
    std::cerr << "no slave nodes available" << endl;
    return -1;
  }
  cout << "found " << nodes.size() << " slave nodes, spawn workers ..." << endl;
  int64_t total_cpu_workers = 0;
  int64_t total_gpu_workers = 0;
  vector<actor> workers;
  for (auto& node : nodes) {
    auto cs = whereis(config_server_atom::value, node);
    auto hc = ask_config(cs, "scheduler.max-threads");
    total_cpu_workers += hc;
    repeat(hc, [&] { append(workers,
                            remote_spawn(node, "CPU worker", fractal_type)); });
    for (auto device_id : explode(ask_str_config(cs, "opencl.device-ids"), ",") | map(to_u32) | flatten) {
      ++total_gpu_workers;
      append(workers,
             remote_spawn(node, "GPU worker", fractal_type, device_id));
    }
    /*
    total_gpu_workers += gd;
    repeat(gd, [&] { append(workers,
                            remote_spawn(node, "GPU worker", fractal_type)); });
    */
  }
  if (workers.empty()) {
    cerr << "could not spawn any workers" << endl;
    return -1;
  }
  cout << "^run demo on " << nodes.size() << " slave nodes with "
       << total_cpu_workers << " CPU workers and "
       << total_gpu_workers << " GPU workers" << endl;

  // Get some test values
  auto clt = spawn<client_actor>(sink,
                                 3, /* seconds to buffer */
                                 default_height,
                                 default_width);
  io::publish(clt, 1337, nullptr, true);
  anon_send(clt, init_atom::value, workers);
  await_all_actors_done();
  // Lauch gui
  //anon_send(clt, atom("SetSink"), actor_cast<actor>(*main.mainWidget));
  // TODO: Setup alrogithm based on this values
  // TODO: Get Resolution
  // TODO: Setup Tile sizes
  //size_t number_of_workers = total_cpu_workers + total_gpu_workers;
  return 0;
}

vector<node_id> connect_nodes(const string& nodes_list) {
  auto to_node_id = [&](const string& node) -> node_id {
    node_id result;
    try {
      result = connect_node(explode(node, '/', token_compress_on) | to_pair).first;
    } catch (std::exception& e) {
      std::cerr << "cannot connect to '" << node << "': "
                << e.what() << std::endl;
    }
    return result;
  };
  return explode(nodes_list, ',', token_compress_on)
         | map(to_node_id) | filter_not(is_invalid);
}

class actor_system;

int run(actor_system&, vector<node_id> nodes, int argc, char** argv) {
  cout << "nodes:" << endl;
  for (auto& n : nodes) {
    cout << to_string(n) << endl;
  }
  std::map<string, atom_value> fractal_map = {
    {"mandelbrot", mandelbrot_atom::value},
    {"tricorn", tricorn_atom::value},
    {"burnship", burnship_atom::value},
    {"julia", julia_atom::value},
  };
  string fractal = "mandelbrot";
  string controllee;
  auto res = message_builder(argv + 1, argv + argc).extract_opts({
    // client options
    {"fractal,f", "mandelbrot (default) | tricorn | burnship | julia", fractal},
    {"no-gui", "save images to local directory"},
    // controller
    {"controllee,c", "control given client ('host/port' notation)", controllee}
  });
  if (fractal_map.count(fractal) == 0)
    std::cerr << "unrecognized fractal type" << eom;
  if (res.opts.count("controllee")) {
    auto f = lift(io::remote_actor);
    auto x = explode(controllee, '/') | to_pair;
    auto c = f(mget<0>(x), to_u16(mget<1>(x)));
  }
  image_sink sink;
  //if (res.opts.count("no-gui"))
    sink = make_file_sink(default_iterations);
  //else
  //  sink = make_gui_sink(argc, argv, default_iterations);
  return client(argc, argv, nodes, fractal_map[fractal], sink);
}

template <class T>
bool check(T&& x, const char* error) {
  if (x)
    return true;
  std::cerr << error << std::endl;
  return false;
}

enum sched_policy_t {
  work_stealing_policy,
  work_sharing_policy
};

caf::maybe<sched_policy_t> sched_policy_from_string(const std::string& str) {
  if (str == "work-stealing")
    return work_stealing_policy;
  if (str == "work-sharing")
    return work_sharing_policy;
  return caf::none;
}

std::string to_string(sched_policy_t x) {
  if (x == work_sharing_policy)
    return "work-sharing";
  return "work-stealing";
}

class actor_system {
public:
  actor_system(int argc, char** argv) : argc_(argc), argv_(argv) {
    // nop
  }

  using f0 = std::function<int (actor_system&, std::vector<std::string>)>;
  using f1 = std::function<int (actor_system&, std::vector<node_id>, std::vector<std::string>)>;
  using f2 = std::function<int (actor_system&, int, char**)>;
  using f3 = std::function<int (actor_system&, std::vector<node_id>, int, char**)>;

  int run(f0 fun) {
    auto f = [fun](actor_system& sys,
                   std::vector<node_id>,
                   std::vector<std::string> args) {
      return fun(sys, std::move(args));
    };
    return run(f);
  }

  int run(f1 fun) {
    auto sg = detail::make_scope_guard([] {
      shutdown();
    });
    auto sched_policy = work_stealing_policy;
    auto set_sched_policy = [&](const std::string& str) -> bool {
      auto x = sched_policy_from_string(str);
      if (x) {
        sched_policy = *x;
        return true;
      }
      return false;
    };
    vector<string> args{argv_[0]};
    string range = default_port_range;
    string bnode;
    string nodes_list;
    string name;
    size_t nthreads = std::thread::hardware_concurrency();
    size_t throughput = std::numeric_limits<size_t>::max();
    string cl_devices;
    auto res = message_builder(argv_ + 1, argv_ + argc_).extract_opts({
      // general CAF options
      {"caf-scheduler-policy", "scheduler policy", set_sched_policy},
      {"caf-scheduler-max-throughput", "max scheduler throughput", throughput},
      {"caf-scheduler-max-threads", "number of scheduler threads", nthreads},
      {"caf-opencl-devices", "comma-separated OpenCL devices", cl_devices},
      // cafrun general options
      {"caf-bootstrap-node", "bootstrap node in 'host/port' notation", bnode},
      // cafrun slave options
      {"caf-slave-mode", "run in slave mode"},
      {"caf-slave-name", "name when running in slave mode", name},
      {"caf-slave-mode-port-range", "slave port range [63000-63050]", range},
      // cafrun master options
      {"caf-slave-nodes", "slave nodes in 'host/port' notation", nodes_list}
    }, nullptr, true);
    if (! res.error.empty())
      return std::cerr << res.error << eom;
    if (sched_policy == work_sharing_policy)
      set_scheduler<policy::work_sharing>(nthreads, throughput);
    else
      set_scheduler<policy::work_stealing>(nthreads, throughput);
    // FIXME: move application-specifc announce code back to main
    //        once actor_system_conf is implemented
    announce_actor_type("CPU worker", cpu_worker);
    //announce_actor_factory("GPU worker", make_gpu_worker);
    announce<std::vector<uint16_t>>("frac_result");
    set_config("scheduler.policy", to_string(sched_policy));
    set_config("scheduler.max-throughput", static_cast<int64_t>(throughput));
    set_config("scheduler.max-threads", static_cast<int64_t>(nthreads));
    set_config("opencl.device-ids", cl_devices);
    if (res.opts.count("caf-slave-mode")) {
      auto pr = explode(range, "-") | map(to_u16) | flatten | to_pair;
      auto bslist = explode(bnode, ',');
      maybe<actor> bootstrapper;
      auto f = lift(io::remote_actor);
      for (auto i = bslist.begin(); i != bslist.end() && ! bootstrapper; ++i) {
        auto x = explode(*i, '/') | to_pair;
        bootstrapper = f(mget<0>(x), to_u16(mget<1>(x)));
      }
      return check(pr, "no valid port range specified")
             && check(res.opts.count("caf-bootstrap-node"), "no bootstrap node")
             && check(bootstrapper, "invalid bootstrap host or port")
             ? slave_mode(pr->first, pr->second, name, *bootstrapper)
             : -1;
    }
    res.remainder.extract([&](string& x) { args.emplace_back(std::move(x)); });
    // generate argc/argv pair from remaining arguments
    return fun(*this, connect_nodes(nodes_list), std::move(args));
  }

  int run(f2 fun) {
    auto f = [fun](actor_system& sys,
                   std::vector<node_id>,
                   std::vector<std::string> args) -> int {
      auto argc = static_cast<int>(args.size());
      std::vector<char*> argv;
      for (auto& arg : args)
        argv.push_back(const_cast<char*>(arg.c_str()));
      return fun(sys, argc, argv.data());
    };
    return run(f);
  }

  int run(f3 fun) {
    auto f = [fun](actor_system& sys,
                   std::vector<node_id> slaves,
                   std::vector<std::string> args) -> int {
      auto argc = static_cast<int>(args.size());
      std::vector<char*> argv;
      for (auto& arg : args)
        argv.push_back(const_cast<char*>(arg.c_str()));
      return fun(sys, std::move(slaves), argc, argv.data());
    };
    return run(f);
  }

  /// Returns the *unmodified* number of
  /// arguments used to initialize this system.
  int argc() const {
    return argc_;
  }

  /// Returns the *unmodified* arguments used to initialize this system.
  char** argv() const {
    return argv_;
  }

private:
  int slave_mode(uint16_t min_port, uint16_t max_port,
                   const string& name, actor bootstrapper) {
    std::cout << "slave_mode: " << name << std::endl;
    std::cout << "my addresses: " << std::endl;
    for (auto addr : io::network::interfaces::list_addresses(io::network::protocol::ipv4))
      std::cout << addr << std::endl;
    auto port = open_port_in_range(min_port, max_port);
    if (! port) {
      std::cerr << "unable to open a port in range "
                << min_port << "-" << max_port << endl;
      return -1;
    }
    anon_send(bootstrapper, ok_atom::value, name, *port);
    scoped_actor self;
    self->monitor(bootstrapper);
    uint32_t rsn = 0;
    self->receive(
      [&](const down_msg& dm) {
        rsn = dm.reason;
      }
    );
    return -static_cast<int>(rsn);
  }

  int argc_;
  char** argv_;
};

int main(int argc, char** argv) {
  actor_system sys{argc, argv};
  return sys.run(run);
}
