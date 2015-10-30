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

behavior cpu_worker(stateful_actor<cpu_worker_state>*,
                    atom_value fractal) {
cout << "spawned new CPU worker" << endl;
  if (! valid_fractal_type(fractal))
    return {};
  return {
    [=](uint32_t image_id, uint16_t iterations,
        uint32_t width, uint32_t height, float min_re, float max_re,
        float min_im, float max_im) -> fractal_result {
      return std::make_tuple(calculate_fractal(fractal, width, height,
                                               iterations,
                                               min_re, max_re,
                                               min_im, max_im),
                             image_id);
    }
  };
}

template <class F, class Atom, class... Ts>
bool x_in_range(uint16_t min_port, uint16_t max_port,
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
  }
  return success;
}

bool open_port_in_range(uint16_t min_port, uint16_t max_port) {
  auto f = [](ok_atom, uint16_t actual_port) {
    std::cout << "running in passive mode on port " << actual_port << endl;
  };
  return x_in_range(min_port, max_port, f, open_atom::value, "", false);
}

bool publish_in_range(uint16_t min_port, uint16_t max_port, actor whom) {
  auto f = [](ok_atom, uint16_t actual_port) {
    std::cout << "running in passive mode on port " << actual_port << endl;
  };
  return x_in_range(min_port, max_port, f, publish_atom::value,
                    whom.address(), std::set<string>{}, "", false);
}

std::pair<node_id, actor_addr>
connect_node(const string& node, uint16_t port) {
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

int controller(int argc, char** argv, const string& client_node, uint16_t client_port) {
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

int client(const vector<node_id>& nodes, atom_value fractal_type) {
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
    return client(nodes, fractal_map[fractal]);
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
    auto res = message_builder(argv_ + 1, argv_ + argc_).extract_opts({
      // worker options
      {"caf-passive-mode", "run in passive mode"},
      {"caf-daemonize-passive-process", "daemonize passive process"},
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
             ? passive_mode(pr->first, pr->second,
                            *bootstrapper,
                            res.opts.count("caf-daemonize-passive-process"))
             : -1;
    }
    res.remainder.extract([&](string& x) { args.emplace_back(std::move(x)); });
    // generate argc/argv pair from remaining arguments
    auto argc = static_cast<int>(args.size()) + 1;
    std::vector<char*> argv;
    argv.push_back(argv[0]);
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
                   actor bootstrapper, bool daemonize) {
    auto cs = whereis(config_server_atom::value);
    anon_send(cs, put_atom::value, "fractal-demo.hardware-concurrency",
              make_message(static_cast<int64_t>(std::thread::hardware_concurrency())));
    if (! open_port_in_range(min_port, max_port)) {
      std::cerr << "unable to open a port in range "
                << min_port << "-" << max_port << endl;
      return -1;
    }
    auto guardian = spawn([](event_based_actor* self) -> behavior {
      return {
        others >> [=] {
          cerr << "guardian received a message: "
               << to_string(self->current_message()) << endl;
        }
      };
    });
    if (daemonize)
      daemon(1, 0);
    scoped_actor self;
    self->monitor(bootstrapper);
    uint32_t rsn = 0;
    self->receive(
      [&](const down_msg& dm) {
        rsn = dm.reason;
      }
    );
    return static_cast<int>(rsn) * -1;
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
  /*
  scoped_actor self;
  // parse command line options
  string cn;
  string range = default_port_range;
  string bnode;
  string nodes_list;
  std::map<string, atom_value> fractal_map = {
    {"mandelbrot", mandelbrot_atom::value},
    {"tricorn", tricorn_atom::value},
    {"burnship", burnship_atom::value},
    {"julia", julia_atom::value},
  };
  string fractal = "mandelbrot";
  auto res = message_builder(argv+1, argv + argc).extract_opts({
    // general options
    {"help,h", "print this help text"},
    // worker options
    {"caf-passive-mode", "run in passive mode"},
    {"caf-daemonize-passive-process", "daemonize process when in passive mode"},
    {"caf-port-range", "daemon port range (default: '63000-63050')", range},
    {"caf-bootstrap-node", "set bootstrap node (host/port notation)", bnode},
    // client options
    {"caf-slave-nodes", "use given nodes (host/port notation)", nodes_list},
    {"fractal,f", "mandelbrot (default) | tricorn | burnship | julia", fractal},
    {"no-gui", "save images to local directory"},
    // controller
    {"controller,c", "start controller UI"},
    {"client-node", "denotes which client to control (host:port notation)", cn},
  });
  if (fractal_map.count(fractal) == 0) {
    std::cout << "unrecognized fractal type" << std::endl;
    return -1;
  }
  if (res.opts.count("caf-passive-mode") > 0)
    return passive_mode(explode(range, "-") | map(to_u16) | flatten | to_pair,
                        res.opts.count("caf-daemonize-passive-process") > 0);
  */
/*
  is_server   = res.opts.count("server") > 0;
  with_opencl = res.opts.count("opencl") > 0;
  publish_workers = res.opts.count("publish") > 0;
  no_gui = res.opts.count("no-gui") > 0;
  is_controller = res.opts.count("controller") > 0;
  if ( res.opts.count("help") > 0 || !res.error.empty() ||
      !res.remainder.empty()) {
    print_desc_and_exit(res);
  }
  if (nexus_port && !nexus_host.empty()) {
    riac::init_probe(nexus_host, *nexus_port);
  }
  if (!is_server && !is_controller) {
    if (nuworkers_ == 0) {
      nuworkers_ = 1;
    }
    vector<actor> workers;
    // spawn at most one GPU worker
    if (with_opencl) {
      cout << "add an OpenCL worker" << endl;
      workers.push_back(spawn_opencl_client(opencl_device_id));
      if (nuworkers_ > 0) {
        --nuworkers_;
      }
    }
    for (size_t i = 0; i < nuworkers_; ++i) {
      cout << "add a CPU worker" << endl;
      workers.push_back(spawn<client>());
    }
    auto emergency_shutdown = [&] {
      for (auto w : workers) {
        self->send_exit(w, exit_reason::remote_link_unreachable);
      }
      await_all_actors_done();
      shutdown();
      exit(-1);
    };
    auto send_workers = [&](const actor& ctrl) {
      for (auto w : workers) {
        send_as(w, ctrl, atom("add"));
      }
    };
    if (publish_workers) {
      try {
        io::publish(self, port);
      } catch (std::exception& e) {
        cerr << "unable to publish at port " << port << ": " << e.what()
             << endl;
        emergency_shutdown();
      }
      self->receive_loop(
        on(atom("getWorkers"), arg_match) >> [&](const actor& last_sender) {
          send_workers(last_sender);
        },
        others >> [&] {
          cerr << "unexpected: " << to_string(self->current_message()) << endl;
        }
      );
    } else {
      try {
        auto controller = io::remote_actor(host, port);
        send_workers(controller);
      } catch (std::exception& e) {
        cerr << "unable to connect to server: " << e.what() << endl;
        emergency_shutdown();
      }
    }
    await_all_actors_done();
    shutdown();
    return 0;
  } else if (is_server && !is_controller) {
    // check fractaltype in arg is valid
    auto fractal_is_vaild = any_of(
      valid_fractal.begin(), valid_fractal.end(),
      [fractal](const map<string, atom_value>::value_type& fractal_type) {
        return fractal_type.first == fractal;
      });
    if (!fractal_is_vaild) {
      cerr << "fractal is not valid" << endl << "choose between: ";
      for (auto& valid_fractal_pair : valid_fractal) {
        cerr << valid_fractal_pair.first << " ";
      }
      cerr << endl;
      print_desc_and_exit(res);
    }

    auto fractal_type_pair = find_if(
      valid_fractal.begin(), valid_fractal.end(),
      [fractal](const map<string, atom_value>::value_type& fractal_type) {
        return fractal == fractal_type.first;
      });

    // else: server mode
    // read config
    auto srvr = spawn<server>(fractal_type_pair->second);
    auto ctrl = spawn<controller>(srvr);
    // TODO: this vector is completely useless to the application,
    //      BUT: libcppa will close the network connection, because the app.
    //      calls ptr = remote_actor(..), sends a message and then ptr goes
    //      out of scope, i.e., no local reference to the remote actor
    //      remains ... however, the remote node will eventually answer to
    //      the message (which will fail, because the network connection
    //      was closed *sigh*); long story short: fix it by improve how
    //      'unused' network connections are detected
    vector<actor> remotes;
    if (not nodes_list.empty()) {
      vector<string> nl;
      split(nl, nodes_list, ',');
      aout(self) << "try to connect to " << nl.size() << " worker nodes" << endl;
      for (auto& n : nl) {
        vector<string> parts;
        split(parts, n, ':');
        message_builder{parts.begin(), parts.end()}.apply(
          on(val<string>, projection<uint16_t>) >>
          [&](const string& host, std::uint16_t p) {
            try {
              aout(self) << "get workers from " << host << ":" << p << endl;
              auto ptr = io::remote_actor(host, p);
              remotes.push_back(ptr);
              send_as(ctrl, ptr, atom("getWorkers"), ctrl);
            } catch (std::exception& e) {
              cerr << "unable to connect to " << host << " on port " << p << endl;
            }
          }
        );
      }
    }
    try {
      io::publish(srvr, port);
    } catch (std::exception& e) {
      cerr << "unable to publish actor: " << e.what() << endl;
      return -1;
    }
    if (nuworkers_ > 0) {
#   ifdef ENABLE_OPENCL
        // spawn at most one GPU worker
        if (with_opencl) {
          cout << "add an OpenCL worker" << endl;
          send_as(spawn_opencl_client(opencl_device_id), ctrl, atom("add"));
          if (nuworkers_ > 0)
            --nuworkers_;
        }
#   endif // ENABLE_OPENCL
      for (size_t i = 0; i < nuworkers_; ++i) {
        cout << "add a CPU worker" << endl;
        send_as(spawn<client>(), ctrl, atom("add"));
      }
    }
    if (no_gui) {
      self->send(srvr, atom("SetSink"), self);
      uint32_t received_images = 0;
      uint32_t total_images = 0xFFFFFFFF; // set properly in 'done' handler
      self->receive_while([&] { return received_images < total_images; }) (
        on(atom("image"), arg_match) >> [&](const QByteArray& ba) {
           auto img = QImage::fromData(ba, image_format);
           std::ostringstream fname;
           fname.width(4);
           fname.fill('0');
           fname.setf(ios_base::right);
           fname << image_file_ending;
           QFile f{fname.str().c_str()};
           if (!f.open(QIODevice::WriteOnly)) {
             cerr << "could not open file: " << fname.str() << endl;
           } else
             img.save(&f, image_format);
           ++received_images;
         },
        others >> [&] {
          cerr << "main:unexpected: "
               << to_string(self->current_message()) << endl;
        }
      );
    } else {
      // launch gui
      QApplication app{argc, argv};
      QMainWindow window;
      Ui::Main main;
      main.setupUi(&window);
      //            main.mainWidget->set_server(master);
      // todo tell widget about other actors?
      //window.resize(ini.get_or_else("fractals", "width", default_width),
      //              ini.get_or_else("fractals", "height", default_height));
      window.resize(default_width, default_height);
      self->send(srvr, atom("SetSink"), main.mainWidget->as_actor());
      window.show();
      app.quitOnLastWindowClosed();
      app.exec();
    }
    self->send_exit(ctrl, exit_reason::user_shutdown);
    // await_all_actors_done();
    shutdown();
  } else if (is_controller && !is_server) { // is controller ui
    cout << "starting controller ui" << endl;
    auto ctrl = io::remote_actor(host, port);
    // launch gui
    QApplication app{argc, argv};
    QMainWindow window;
    Ui::Controller ctrl_ui;
    ctrl_ui.setupUi(&window);
    auto ctrl_widget = ctrl_ui.controllerWidget;
    ctrl_widget->set_controller(ctrl);
    ctrl_widget->initialize();
    send_as(ctrl_widget->as_actor(), ctrl, atom("widget"),
            ctrl_widget->as_actor());
    self->send(ctrl_widget->as_actor(), atom("fraclist"), valid_fractal);
    window.show();
    app.quitOnLastWindowClosed();
    app.exec();
  } else {
    print_desc_and_exit(res);
  }
*/
}
