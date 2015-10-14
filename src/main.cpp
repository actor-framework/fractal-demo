#include <unistd.h>

#include <QByteArray>
#include <QMainWindow>
#include <QApplication>

#include "caf/all.hpp"
#include "caf/io/all.hpp"
#include "caf/riac/all.hpp"

#include "caf/experimental/announce_actor_type.hpp"

#include "atoms.hpp"
#include "ui_main.h"
#include "server.hpp"
#include "controller.hpp"
#include "mainwidget.hpp"
#include "projection.hpp"
#include "calculate_fractal.hpp"

#include "ui_controller.h"

using namespace std;
using namespace caf;
using namespace caf::experimental;

using ivec = vector<int>;
using fvec = vector<float>;
using actor_set = set<actor>;
using str_atom_map = map<string, atom_value>;

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

int passive_mode() {
  std::cout << "type 'quit' to shut process down" << endl;
  std::string line;
  for (;;) {
    std::getline(std::cin, line);
    if (line == "quit")
      return 0;
  }
}

std::pair<node_id, actor_addr> connect_node(const std::string& node) {
  std::pair<node_id, actor_addr> result;
  scoped_actor self;
  auto mm = io::get_middleman_actor();
  std::vector<std::string> parts;
  split(parts, node, ':');
  if (parts.size() != 2)
    throw std::invalid_argument("invalid format, expected <host>:<port>");
  auto port = static_cast<uint16_t>(std::stoul(parts[1]));
  self->sync_send(mm, connect_atom::value, std::move(parts[0]), port).await(
    [&](ok_atom, const node_id& nid, actor_addr aid, std::set<std::string>&) {
      if (nid == invalid_node_id)
        throw std::runtime_error("no valid node_id received");
      result = std::make_pair(nid, aid);
    },
    [&](error_atom, std::string& msg) {
      throw runtime_error(std::move(msg));
    }
  );
  return result;
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
  app.exec();
  return 0;
}

int client(const std::vector<node_id>& nodes) {
  return 0;
}

std::vector<node_id> connect_nodes(const std::string& nodes_list) {
  std::vector<node_id> result;
  std::vector<std::string> nl;
  split(nl, nodes_list, ',', token_compress_on);
  std::cout << "try to connect to " << nl.size() << " worker nodes" << endl;
  std::transform(nl.begin(), nl.end(),
                 std::back_inserter(result),
                 [](const std::string& node) -> node_id {
                   node_id result;
                   try {
                     result = connect_node(node).first;
                   } catch (std::exception& e) {
                     std::cerr << "cannot connect to '" << node << "': "
                               << e.what() << std::endl;
                   }
                   return result;
                 });
  result.erase(std::remove(result.begin(), result.end(), invalid_node_id),
               result.end());
  return result;
}

int err(const char* str) {
  std::cerr << str << std::endl;
  return -1;
}

int main(int argc, char** argv) {
  riac::announce_message_types();
  announce<std::vector<uint16_t>>("uint16_vec");
  announce_actor_type("CPU worker", cpu_worker);
  announce_actor_factory("GPU worker", make_gpu_worker);
  scoped_actor self;
  // parse command line options
  string cn;
  uint16_t port = 20283;
  std::string nodes_list;
  auto sg = detail::make_scope_guard([] {
    shutdown();
  });
  const map<string, atom_value> fractal_map = {
    {"mandelbrot", mandelbrot_atom::value},
    {"tricorn", tricorn_atom::value},
    {"burnship", burnship_atom::value},
    {"julia", julia_atom::value},
  };
  std::string fractal = "mandelbrot";
  auto res = message_builder(argv+1, argv + argc).extract_opts({
    // general options
    {"caf-passive-node", "run in passive mode"},
    {"port,p", "set port (default: 20283)", port},
    {"help,h", "print this help text"},
    // client options
    {"fractal,f", "mandelbrot (default) | tricorn | burnship | julia", fractal},
    {"node,n", "add given node (host:port notation) as workers", nodes_list},
    {"no-gui", "save images to local directory"},
    // controller
    {"client-node", "denotes which client to control (host:port notation)", cn},
    {"controller,c", "start controller UI"}
  });
  if (fractal_map.count(fractal) == 0) {
    std::cout << "unrecognized fractal type" << std::endl;
    return -1;
  }
  if (res.opts.count("caf-passive-mode") > 0)
    return passive_mode();
  if (res.opts.count("controller") > 0)
    return controller(argc, argv, cn);
  auto nodes = connect_nodes(nodes_list);
  if (nodes.empty())
    return err("no worker node available");
  return client(nodes);
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
