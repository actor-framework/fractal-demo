#include <unistd.h>

#include <QByteArray>
#include <QMainWindow>
#include <QApplication>

#include "caf/all.hpp"
#include "caf/io/all.hpp"
#include "caf/riac/all.hpp"

#include "ui_main.h"
#include "server.hpp"
#include "client.hpp"
#include "counter.hpp"
#include "controller.hpp"
#include "mainwidget.hpp"
#include "ui_controller.h"
#include "q_byte_array_info.hpp"

using namespace std;
using namespace caf;

std::vector<std::string> split(const std::string& str, char delim,
                               bool keep_empties = false) {
  using namespace std;
  vector<string> result;
  stringstream strs{str};
  string tmp;
  while (getline(strs, tmp, delim)) {
    if (!tmp.empty() || keep_empties)
      result.push_back(std::move(tmp));
  }
  return result;
}

int main(int argc, char** argv) {
  // announce some messaging types
  scoped_actor self;
  riac::announce_message_types();
  announce<vector<int>>();
  announce<vector<float>>();
  announce<set<actor>>();
  announce<map<string, atom_value>>();
  announce(typeid(QByteArray), create_unique<q_byte_array_info>());
  // sent from server to client
  announce_tuple<atom_value, uint32_t, uint32_t, uint32_t, uint32_t, float_type,
                 float_type, float_type, float_type>();
  // sent from client to -server
  announce_tuple<atom_value, uint32_t, QByteArray>();
  // parse command line options
  string host;
  uint16_t port = 20283;
  size_t num_workers = 0;
  bool no_gui = false;
  bool is_server = false;
  bool with_opencl = false;
  bool is_controller = false;
  bool publish_workers = false;
  bool use_riac        = false;
  uint32_t opencl_device_id = 0;
  std::string nodes_list;
  std::string nexus_ip;
  std::uint16_t nexus_port;
  const map<string, atom_value> valid_fractal
    = {{"mandelbrot", atom("mandel")},
       {"tricorn", atom("tricorn")},
       {"burnship", atom("burnship")}};
  std::string fractal = "mandelbrot";
  options_description desc;
  bool args_valid = match_stream<string>(argv + 1, argv + argc)(
    // general options
    on_opt0('s', "server", &desc, "run in server mode", "general")         >> set_flag(is_server),
    on_opt1('p', "port", &desc, "set port (default: 20283)", "general")    >> rd_arg(port),
    on_opt0('h', "help", &desc, "print this text", "general")              >> print_desc_and_exit(&desc),
    on_opt1('d', "device", &desc, "set OpenCL device", "general")          >> rd_arg(opencl_device_id),
    on_opt0('r', "riac", &desc, "use CAFs debugging tools", "general")     >> set_flag(use_riac),
    on_opt1('n', "nexus", &desc, "set nexus IP for debugging", "general")  >> rd_arg(nexus_ip),
    on_opt1('d', "probe", &desc, "set nexus port", "general")              >> rd_arg(nexus_port),
    // client options
    on_opt1('H', "host", &desc, "set server host", "client")               >> rd_arg(host),
    on_opt1('w', "worker", &desc, "number workers (default: 1)", "client") >> rd_arg(num_workers),
    on_opt0('o', "opencl", &desc, "enable opencl", "client")               >> set_flag(with_opencl),
    on_opt0('u', "publish", &desc, "don't connect to server; only publish worker(s) at given port", "client") >> set_flag(publish_workers),
    // server options
    on_opt1('f', "fractal", &desc, "choose fractaltype (default: mandelbrot)", "server") >> rd_arg(fractal),
    on_opt1('n', "nodes", &desc, "use given list (host:port notation) as workes", "server") >> rd_arg(nodes_list),
    on_opt0('g', "no-gui", &desc, "save images to local directory", "server") >> set_flag(no_gui),
    // controller
    on_opt0('c', "controller", &desc, "start a controller ui", "controller") >> set_flag(is_controller));
  if (!args_valid)
    print_desc_and_exit(&desc)();
  if (use_riac)
    riac::init_probe(nexus_ip, nexus_port);
  if (!is_server && !is_controller) {
    if (num_workers == 0)
      num_workers = 1;
    std::vector<actor> workers;
#ifdef ENABLE_OPENCL
    // spawn at most one GPU worker
    if (with_opencl) {
      cout << "add an OpenCL worker" << endl;
      workers.push_back(spawn_opencl_client(opencl_device_id));
      if (num_workers > 0)
        --num_workers;
    }
#endif // ENABLE_OPENCL
    for (size_t i = 0; i < num_workers; ++i) {
      cout << "add a CPU worker" << endl;
      workers.push_back(spawn<client>());
    }
    auto emergency_shutdown = [&] {
      for (auto w : workers) {
        self->send(w, atom("EXIT"), exit_reason::remote_link_unreachable);
      }
      await_all_actors_done();
      shutdown();
      exit(-1);
    };
    auto send_workers = [&](const actor& controller) {
      for (auto w : workers) {
        send_as(w, controller, atom("add"));
      }
    };
    if (publish_workers) {
      try {
        publish(self, port);
      } catch (std::exception& e) {
        cerr << "unable to publish at port " << port << ": " << e.what()
             << endl;
        emergency_shutdown();
      }
      self->receive_loop(
        on(atom("getWorkers"), arg_match) >> [&](const actor& last_sender) {
                                               auto controller = last_sender;
                                               send_workers(controller);
                                             },
        others() >> [&] {
                      cerr << "unexpected: " << to_string(self->last_dequeued())
                           << endl;
                    });
    } else {
      try {
        auto controller = remote_actor(host, port);
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
      print_desc_and_exit(&desc)();
    }

    auto fractal_type_pair = find_if(
      valid_fractal.begin(), valid_fractal.end(),
      [fractal](const map<string, atom_value>::value_type& fractal_type) {
        return fractal == fractal_type.first;
      });

    // else: server mode
    // read config
    config_map ini;
    try {
      ini.read_ini("fractal_server.ini");
    } catch (exception&) { /* no config file found (use defaults)" */
    }
    // counter requires init message with gui widget
    auto cntr = spawn<counter>();
    auto srvr = spawn<server>(ini, cntr, fractal_type_pair->second);
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
      auto nl = split(nodes_list, ',');
      for (auto& n : nl) {
        match(split(n, ':'))(on(val<string>, projection<uint16_t>) >>
                             [&](const string& host, std::uint16_t p) {
          try {
            auto ptr = remote_actor(host, p);
            remotes.push_back(ptr);
            send_as(ctrl, ptr, atom("getWorkers"), ctrl);
          } catch (std::exception& e) {
            cerr << "unable to connect to " << host << " on port " << p << endl;
          }
        });
      }
    }
    //        else {
    try {
      publish(ctrl, port);
    } catch (std::exception& e) {
      cerr << "unable to publish actor: " << e.what() << endl;
      return -1;
    }
    //        }
    if (num_workers > 0) {
#ifdef ENABLE_OPENCL
      // spawn at most one GPU worker
      if (with_opencl) {
        cout << "add an OpenCL worker" << endl;
        send_as(spawn_opencl_client(opencl_device_id), ctrl, atom("add"));
        if (num_workers > 0)
          --num_workers;
      }
#endif // ENABLE_OPENCL
      for (size_t i = 0; i < num_workers; ++i) {
        cout << "add a CPU worker" << endl;
        send_as(spawn<client>(), ctrl, atom("add"));
      }
    }
    if (no_gui) {
      self->send(cntr, atom("init"), self);
      uint32_t received_images = 0;
      uint32_t total_images = 0xFFFFFFFF; // set properly in 'done' handler
      self->receive_while(gref(received_images) < gref(total_images))(
        on(atom("image"), arg_match)
        >> [&](uint32_t img_id, const QByteArray& ba) {
             auto img = QImage::fromData(ba, image_format);
             std::ostringstream fname;
             fname.width(4);
             fname.fill('0');
             fname.setf(ios_base::right);
             fname << img_id << image_file_ending;
             QFile f{fname.str().c_str()};
             if (!f.open(QIODevice::WriteOnly)) {
               cerr << "could not open file: " << fname.str() << endl;
             } else
               img.save(&f, image_format);
             ++received_images;
           },
        on(atom("done"), arg_match)
        >> [&](uint32_t num_images) { total_images = num_images; },
        others() >> [&] {
                      cerr << "main:unexpected: "
                           << to_string(self->last_dequeued()) << endl;
                    });
    } else {
      // launch gui
      QApplication app{argc, argv};
      QMainWindow window;
      Ui::Main main;
      main.setupUi(&window);
      //            main.mainWidget->set_server(master);
      // todo tell widget about other actors?
      window.resize(ini.get_as<int>("fractals", "width"),
                    ini.get_as<int>("fractals", "height"));
      self->send(cntr, atom("init"), main.mainWidget->as_actor(), ctrl);
      window.show();
      app.quitOnLastWindowClosed();
      app.exec();
    }
    self->send(ctrl, atom("quit"));
    // await_all_actors_done();
    shutdown();
  } else if (is_controller && !is_server) { // is controller ui
    cout << "starting controller ui" << endl;
    auto ctrl = remote_actor(host, port);
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
    print_desc_and_exit(&desc)();
  }

  return 0;
}
