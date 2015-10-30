#include <unistd.h>

#include <QByteArray>
#include <QMainWindow>
#include <QApplication>

#include "caf/all.hpp"
#include "caf/io/all.hpp"
#include "caf/riac/all.hpp"

// fractal-demo extensions to CAF
#include "caf/cec.hpp"
#include "caf/maybe.hpp"

#include "caf/experimental/whereis.hpp"
#include "caf/experimental/announce_actor_type.hpp"

#include "atoms.hpp"
#include "ui_main.h"
#include "server.hpp"
#include "utility.hpp"
#include "controller.hpp"
#include "mainwidget.hpp"
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

spawn_result make_gpu_worker(message args) {
  spawn_result result;
  args.apply({
    [&](atom_value fractal) {
      if (! valid_fractal_type(fractal))
        return;
      // TODO: implement me
    }
  });
  return result;
}

struct cpu_worker_state {
  const char* name = "CPU worker";
};

behavior cpu_worker(stateful_actor<cpu_worker_state>* self,
                    atom_value fractal) {
  aout(self) << "spawned new CPU worker" << endl;
  if (! valid_fractal_type(fractal))
    return {};
  return {
    [=](uint32_t image_id, uint16_t iterations,
        uint32_t width, uint32_t height, float min_re, float max_re,
        float min_im, float max_im) -> fractal_result {
      auto t1 = std::chrono::high_resolution_clock::now();
      auto result = calculate_fractal(fractal, width, height,
                                      iterations,
                                      min_re, max_re,
                                      min_im, max_im);
      auto t2 = std::chrono::high_resolution_clock::now();
      auto diff = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1);
      return std::make_tuple(result, image_id, diff.count());
    }
  };
}

template <class F, class Atom, class... Ts>
optional<uint16_t> x_in_range(uint16_t min_port, uint16_t max_port,
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

optional<uint16_t> open_port_in_range(uint16_t min_port, uint16_t max_port) {
  auto f = [](ok_atom, uint16_t actual_port) {
    std::cout << "running in passive mode on port " << actual_port << endl;
  };
  return x_in_range(min_port, max_port, f, open_atom::value, "", false);
}

optional<uint16_t> publish_in_range(uint16_t min_port, uint16_t max_port, actor whom) {
  auto f = [](ok_atom, uint16_t actual_port) {
    std::cout << "running in passive mode on port " << actual_port << endl;
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
    [&](error_atom, const string&) {
      // nop
    });
  if (is_invalid(result.first))
    cerr << "cannot connect to " << node << " onn port " << port << endl;
  return result;
}

std::pair<node_id, actor_addr>
connect_node(const optional<std::pair<string, string>>& x) {
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

int64_t ask_config(actor config_serv, const char* key) {
  scoped_actor self;
  int64_t result = 0;
  self->sync_send(config_serv, get_atom::value, key).await(
    [&](ok_atom, string&, message value) {
      value.apply([&](int64_t res) { result = res; });
    },
    others >> [] {
      // ignore
    }
  );
  return result;
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

int client(int argc, char** argv,
           const vector<node_id>& nodes, atom_value fractal_type) {
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
    auto hc = ask_config(cs, "fractal-demo.hardware-concurrency");
    total_cpu_workers += hc;
    repeat(hc, [&] { append(workers,
                            remote_spawn(node, "CPU worker", fractal_type)); });
    auto gd = ask_config(cs, "fractal-demo.gpu-devices");
    total_gpu_workers += gd;
    repeat(gd, [&] { append(workers,
                            remote_spawn(node, "GPU worker", fractal_type)); });
  }
  if (workers.empty()) {
    cerr << "could not spawn any workers" << endl;
    return -1;
  }
  cout << "run demo on " << nodes.size() << " slave nodes with "
       << total_cpu_workers << " CPU workers and "
       << total_gpu_workers << " GPU workers" << endl;
  // Get some test values
  auto clt = spawn<client_actor>(workers);
  {
    scoped_actor self;
    self->send(clt, test_atom::value, self);
    self->receive([](size_t best_case, double avarage_case, size_t worst_case) {
      std::cout << "Best case: "    << best_case    << std::endl
                << "Avarage case: " << avarage_case << std::endl
                << "Worst case: "   << worst_case   << std::endl;
    });
  }
  // Lauch gui
  QApplication app{argc, argv};
  QMainWindow window;
  Ui::Main main;
  main.setupUi(&window);
  window.resize(default_width, default_height);
  //anon_send(clt, atom("SetSink"), actor_cast<actor>(*main.mainWidget));
  anon_send(clt, atom("SetSink"), main.mainWidget->as_actor());
  window.show();
  app.quitOnLastWindowClosed();
  app.exec();
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
  if (! res.opts.count("controllee"))
    return client(argc, argv, nodes, fractal_map[fractal]);
  auto f = lift(io::remote_actor);
  auto x = explode(controllee, '/') | to_pair;
  auto c = f(oget<0>(x), to_u16(oget<1>(x)));
  return 0;
}

template <class T>
bool check(T&& x, const char* error) {
  if (x)
    return true;
  std::cerr << error << std::endl;
  return false;
}

class actor_system {
public:
  actor_system(int argc, char** argv) : argc_(argc), argv_(argv) {
    // nop
  }

  using f0 = std::function<int (actor_system&, int, char**)>;
  using f1 = std::function<int (actor_system&, std::vector<node_id>, int, char**)>;

  int run(f0 fun) {
    return fun(*this, argc_, argv_);
  }

  int run(f1 fun) {
    auto sg = detail::make_scope_guard([] {
      shutdown();
    });
    vector<string> args;
    string range = default_port_range;
    string bnode;
    string nodes_list;
    string name;
    auto res = message_builder(argv_ + 1, argv_ + argc_).extract_opts({
      // worker options
      {"caf-passive-mode", "run in passive mode"},
      {"caf-slave-name", "set name when running in passive mode", name},
      {"caf-passive-mode-port-range", "daemon port range [63000-63050]", range},
      {"caf-bootstrap-node", "set bootstrap node (host/port notation)", bnode},
      {"caf-slave-nodes", "set slave nodes ('host/port'' notation)", nodes_list}
    }, nullptr, true);
    if (res.opts.count("caf-passive-mode") > 0) {
      auto pr = explode(range, "-") | map(to_u16) | flatten | to_pair;
      auto bslist = explode(bnode, ',');
      optional<actor> bootstrapper;
      auto f = lift(io::remote_actor);
      for (auto i = bslist.begin(); i != bslist.end() && ! bootstrapper; ++i) {
        auto x = explode(*i, '/') | to_pair;
        bootstrapper = f(oget<0>(x), to_u16(oget<1>(x)));
      }
      return check(pr, "no valid port range specified")
             && check(res.opts.count("caf-bootstrap-node"), "no bootstrap node")
             && check(bootstrapper, "invalid bootstrap host or port")
             ? passive_mode(pr->first, pr->second, name, *bootstrapper)
             : -1;
    }
    res.remainder.extract([&](string& x) { args.emplace_back(std::move(x)); });
    // generate argc/argv pair from remaining arguments
    auto argc = static_cast<int>(args.size()) + 1;
    std::vector<char*> argv;
    argv.push_back(argv_[0]);
    for (auto& arg : args)
      argv.push_back(const_cast<char*>(arg.c_str()));
    return fun(*this, connect_nodes(nodes_list), argc, argv.data());
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
  int passive_mode(uint16_t min_port, uint16_t max_port,
                   const string& name, actor bootstrapper) {
    auto cs = whereis(config_server_atom::value);
    anon_send(cs, put_atom::value, "fractal-demo.hardware-concurrency",
              make_message(static_cast<int64_t>(std::thread::hardware_concurrency())));
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
  riac::announce_message_types();
  announce_actor_type("CPU worker", cpu_worker);
  announce_actor_factory("GPU worker", make_gpu_worker);
  actor_system sys{argc, argv};
  return sys.run(run);
}
