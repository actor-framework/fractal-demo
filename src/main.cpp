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
using namespace vector_operators;

using std::endl;

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
      [&](error_atom, const std::string&) {
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
                    whom.address(), std::set<std::string>{}, "", false);
}

int passive_mode(uint16_t min_port, uint16_t max_port) {
  auto cs = whereis(config_server_atom::value);
  anon_send(cs, put_atom::value, "fractal-demo.hardware-concurrency",
            make_message(static_cast<int64_t>(std::thread::hardware_concurrency())));
  if (! open_port_in_range(min_port, max_port)) {
    std::cerr << "unable to open a port in range "
              << min_port << "-" << max_port << endl;
    return -1;
  }
  daemon(1, 0);
  // make sure main() never returns
  for (;;) {
    using rep = std::chrono::seconds::rep;
    std::chrono::seconds t{std::numeric_limits<rep>::max()};
    std::this_thread::sleep_for(t);
  }
  return 0;
}

int passive_mode(optional<std::pair<uint16_t, uint16_t>> port_range) {
  if (port_range)
    return passive_mode(port_range->first, port_range->second);
  std::cerr << "no valid port range found" << std::endl;
  return -1;
}

std::pair<node_id, actor_addr>
connect_node(const std::string& node,
             const std::pair<uint16_t, uint16_t>& range) {
  std::pair<node_id, actor_addr> result;
  scoped_actor self;
  auto mm = io::get_middleman_actor();
  for (uint16_t port = range.first;
       port < range.second && is_invalid(result.first); ++port) {
    self->sync_send(mm, connect_atom::value, node, port).await(
      [&](ok_atom, const node_id& nid, actor_addr aid, std::set<std::string>&) {
        if (! is_invalid(nid))
          result = std::make_pair(nid, aid);
      },
      [&](error_atom, const std::string&) {
        // nop
      });
  }
  return result;
}

std::pair<node_id, actor_addr>
connect_node(const std::string& node,
             const optional<std::pair<uint16_t, uint16_t>>& range) {
  if (range)
    return connect_node(node, *range);
  return {};
}

std::pair<node_id, actor_addr> connect_node(const std::string& node) {
  return connect_node(node,
                      explode(default_port_range, is_any_of("-"))
                      | map(to_u16) | flatten | to_pair);
}

int controller(int argc, char** argv, const std::string& client_node) {
  auto ctrl = actor_cast<actor>(connect_node(client_node).second);
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
    [&](ok_atom, std::string&, message value) {
      value.apply([&](int64_t res) { result = res; });
    },
    others >> [] {
      // ignore
    }
  );
  return result;
}

template <class... Ts>
maybe<actor> remote_spawn(node_id target, std::string name, Ts&&... xs) {
  maybe<actor> result = actor{invalid_actor};
  scoped_actor self;
  auto mm = io::get_middleman_actor();
  self->sync_send(mm, spawn_atom::value, target, name,
                  make_message(std::forward<Ts>(xs)...)).await(
    [&](ok_atom, const actor_addr& addr, const std::set<std::string>& ifs) {
      if (ifs.empty())
        result = actor_cast<actor>(addr);
      else
        result = std::make_error_condition(cec::expected_dynamically_typed_remote_actor);
    },
    [&](error_atom, const std::string& errmsg) {
      if (errmsg == "no connection to requested node")
        result = std::make_error_condition(cec::remote_node_unreachable);
      else
        result = std::make_error_condition(cec::unknown_error);
    }
  );
  return result;
}

int client(const std::vector<node_id>& nodes) {
  if (nodes.empty()) {
    std::cerr << "no slave nodes available" << endl;
    return -1;
  }
  std::vector<actor> workers;
  for (auto& node : nodes) {
    auto cs = whereis(config_server_atom::value, node);
    repeat(ask_config(cs, "fractal-demo.hardware-concurrency"),
           [&]() { append(workers, remote_spawn(node, "CPU worker")); });
    repeat(ask_config(cs, "fractal-demo.gpu-devices"),
           [&]() { append(workers, remote_spawn(node, "GPU worker")); });
  }
  return 0;
}

std::vector<node_id> connect_nodes(const std::string& nodes_list,
                                   const std::pair<uint16_t, uint16_t>& range) {
  auto to_node_id = [&](const std::string& node) -> node_id {
    node_id result;
    try {
      result = connect_node(node, range).first;
    } catch (std::exception& e) {
      std::cerr << "cannot connect to '" << node << "': "
                << e.what() << std::endl;
    }
    return result;
  };
  return explode(nodes_list, ',', token_compress_on)
         | map(to_node_id) | filter_not(is_invalid);
}

std::vector<node_id>
connect_nodes(const std::string& nodes_list,
              const optional<std::pair<uint16_t, uint16_t>>& port_range) {
  if (port_range)
    return connect_nodes(nodes_list, *port_range);
  std::cerr << "no valid port range found" << std::endl;
  return {};
}

int err(const char* str) {
  std::cerr << str << std::endl;
  return -1;
}

int main(int argc, char** argv) {
  riac::announce_message_types();
  announce_actor_type("CPU worker", cpu_worker);
  announce_actor_factory("GPU worker", make_gpu_worker);
  scoped_actor self;
  // parse command line options
  std::string cn;
  std::string range = default_port_range;
  std::string nodes_list;
  auto sg = detail::make_scope_guard([] {
    shutdown();
  });
  const std::map<std::string, atom_value> fractal_map = {
    {"mandelbrot", mandelbrot_atom::value},
    {"tricorn", tricorn_atom::value},
    {"burnship", burnship_atom::value},
    {"julia", julia_atom::value},
  };
  std::string fractal = "mandelbrot";
  auto res = message_builder(argv+1, argv + argc).extract_opts({
    // general options
    {"help,h", "print this help text"},
    // worker options
    {"caf-passive-mode", "run in passive mode"},
    {"caf-port-range", "daemon port range (default: '63000-63050')", range},
    // client options
    {"caf-slave-nodes", "use given nodes (host:port notation)", nodes_list},
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
  auto u16range = [&] {
    return explode(range, is_any_of("-")) | map(to_u16) | flatten | to_pair;
  };
  if (res.opts.count("caf-passive-mode") > 0)
    return passive_mode(u16range());
  if (res.opts.count("controller") > 0)
    return controller(argc, argv, cn);
  return client(connect_nodes(nodes_list, u16range()));
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
    std::vector<actor> workers;
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
