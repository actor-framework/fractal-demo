#include <unistd.h>

#include <QByteArray>
#include <QMainWindow>
#include <QApplication>

#include "cppa/opt.hpp"
#include "cppa/cppa.hpp"

#include "ui_main.h"
#include "server.hpp"
#include "client.hpp"
#include "mainwidget.hpp"
#include "q_byte_array_info.hpp"

using namespace std;
using namespace cppa;

std::vector<std::string> split(const std::string& str, char delim, bool keep_empties = false) {
    using namespace std;
    vector<string> result;
    stringstream strs{str};
    string tmp;
    while (getline(strs, tmp, delim)) {
        if (!tmp.empty() || keep_empties) result.push_back(std::move(tmp));
    }
    return result;
}

int main(int argc, char** argv) {
    // announce some messaging types
    announce<vector<int>>();
    announce<vector<float>>();
    announce(typeid(QByteArray), new q_byte_array_info);
    // parse command line options
    string host;
    uint16_t port = 20283;
    size_t num_workers = 0;
    bool no_gui = false;
    bool is_server = false;
    bool with_opencl = false;
    bool publish_workers = false;
    std::string nodes_list;
    options_description desc;
    bool args_valid = match_stream<string>(argv + 1, argv + argc) (
        // general options
        on_opt0('s', "server",  &desc, "run in server mode",          "general") >> set_flag(is_server),
        on_opt1('p', "port",    &desc, "set port (default: 20283)",   "general") >> rd_arg(port),
        on_opt0('h', "help",    &desc, "print this text",             "general") >> print_desc_and_exit(&desc),
        // client options
        on_opt1('H', "host",    &desc, "set server host",             "client") >> rd_arg(host),
        on_opt1('w', "worker",  &desc, "number workers (default: 1)", "client") >> rd_arg(num_workers),
        on_opt0('o', "opencl",  &desc, "enable opencl",               "client") >> set_flag(with_opencl),
        on_opt0('u', "publish", &desc, "don't connect to server; only publish worker(s) at given port", "client") >> set_flag(publish_workers),
        // server options
        on_opt1('n', "nodes",   &desc, "use given list (host:port notation) as workes", "server") >> rd_arg(nodes_list),
        on_opt0('g', "no-gui",  &desc, "save images to local directory", "server") >> set_flag(no_gui)
    );
    if (!args_valid) print_desc_and_exit(&desc)();
    if (!is_server) {
        if (num_workers == 0) num_workers = 1;
        std::vector<actor_ptr> workers;
#       ifdef ENABLE_OPENCL
        // spawn at most one GPU worker
        if (with_opencl) {
            cout << "add an OpenCL worker" << endl;
            workers.push_back(spawn_opencl_client());
            if (num_workers > 0) --num_workers;
        }
#       endif // ENABLE_OPENCL
        for (size_t i = 0; i < num_workers; ++i) {
            cout << "add a CPU worker" << endl;
            workers.push_back(spawn<client>());
        }
        auto emergency_shutdown = [&] {
            for (auto w : workers) {
                send(w, atom("EXIT"), exit_reason::remote_link_unreachable);
            }
            await_all_others_done();
            shutdown();
            exit(-1);
        };
        auto send_workers = [&](const actor_ptr& master) {
            for (auto w : workers) {
                send_as(w, master, atom("newWorker"));
            }
        };
        if (publish_workers) {
            try { publish(self, port); }
            catch (std::exception& e) {
                cerr << "unable to publish at port " << port << ": "
                     << e.what() << endl;
                emergency_shutdown();
            }
            receive_loop (
                on(atom("getWorkers")) >> [&] {
                    auto master = self->last_sender();
                    send_workers(master);
                },
                others() >> [] {
                    cerr << "unexpected: "
                         << to_string(self->last_dequeued()) << endl;
                }
            );
        }
        else {
            try {
                auto master = remote_actor(host, port);
                send_workers(master);
            }
            catch (std::exception& e) {
                cerr << "unable to connect to server: " << e.what() << endl;
                emergency_shutdown();
            }
        }
        await_all_others_done();
        shutdown();
        return 0;
    }
    // else: server mode
    // read config
    config_map ini;
    try { ini.read_ini("fractal_server.ini"); }
    catch (exception&) { /* no config file found (use defaults)" */ }
    // launch and publish master (waits for 'init' message)
    auto master = spawn<server>(ini);
    //TODO: this vector is completely useless to the application,
    //      BUT: libcppa will close the network connection, because the app.
    //      calls ptr = remote_actor(..), sends a message and then ptr goes
    //      out of scope, i.e., no local reference to the remote actor
    //      remains ... however, the remote node will eventually answer to
    //      the message (which will fail, because the network connection
    //      was closed *sigh*); long story short: fix it by improve how
    //      'unused' network connections are detected
    vector<actor_ptr> remotes;
    if (not nodes_list.empty()) {
        cout << "connect to nodes..." << endl;
        auto nl = split(nodes_list, ',');
        for (auto& n : nl) {
            match(split(n, ':')) (
                on(val<string>, projection<uint16_t>) >> [&](const string& host, std::uint16_t p) {
                    try {
                        auto ptr = remote_actor(host, p);
                        remotes.push_back(ptr);
                        send_as(master, ptr, atom("getWorkers"));
                    }
                    catch (std::exception& e) {
                        cerr << "unable to connect to " << host
                             << " on port " << p << endl;
                    }
                }
            );
        }
    }
    else {
        try { publish(master, port); }
        catch (std::exception& e) {
            cerr << "unable to publish actor: " << e.what() << endl;
            return -1;
        }
    }
    if (num_workers > 0) {
#       ifdef ENABLE_OPENCL
        // spawn at most one GPU worker
        if (with_opencl) {
            cout << "add an OpenCL worker" << endl;
            send_as(spawn_opencl_client(), master, atom("newWorker"));
            if (num_workers > 0) --num_workers;
        }
#       endif // ENABLE_OPENCL
        for (size_t i = 0; i < num_workers; ++i) {
            cout << "add a CPU worker" << endl;
            send_as(spawn<client>(), master, atom("newWorker"));
        }
    }
    if (no_gui) {
        send(master, atom("init"), self);
        uint32_t received_images = 0;
        uint32_t total_images = 0xFFFFFFFF; // set properly in 'done' handler
        receive_while(gref(received_images) < gref(total_images)) (
            on(atom("result"), arg_match) >> [&](uint32_t img_id, const QByteArray& ba) {
                auto img = QImage::fromData(ba, image_format);
                std::ostringstream fname;
                fname.width(4);
                fname.fill('0');
                fname.setf(ios_base::right);
                fname << img_id << image_file_ending;
                QFile f{fname.str().c_str()};
                if (!f.open(QIODevice::WriteOnly)) {
                    cerr << "could not open file: " << fname.str() << endl;
                }
                else img.save(&f, image_format);
                ++received_images;
            },
            on(atom("done"), arg_match) >> [&](uint32_t num_images) {
                total_images = num_images;
            },
            others() >> [] {
                cerr << "main:unexpected: "
                     << to_string(self->last_dequeued()) << endl;
            }
        );
    }
    else {
        // launch gui
        QApplication app{argc, argv};
        QMainWindow window;
        Ui::Main main;
        main.setupUi(&window);
        main.mainWidget->set_server(master);
        window.resize(ini.get_as<int>("fractals", "width"),
                      ini.get_as<int>("fractals", "height"));
        send_as(nullptr, master, atom("init"), main.mainWidget->as_actor());
        //window.resize(DEFAULT_WIDTH, DEFAULT_HEIGHT);
        window.show();
        app.quitOnLastWindowClosed();
        app.exec();
    }
    send_as(nullptr, master, atom("quit"));
    await_all_others_done();
    shutdown();
    return 0;
}
