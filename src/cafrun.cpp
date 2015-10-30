#include <unistd.h>

#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <iostream>

#include "caf/all.hpp"
#include "caf/io/all.hpp"

#include "utility.hpp"

using namespace caf;

using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::vector;

using csi = string::const_iterator; // constant string iterator

using namespace container_operators;

// compare [first, last) to cstr, ignoring leading '-'
bool opt_cmp(csi first, csi last, const char* cstr) {
  return std::equal(std::find_if(first, last, [](char c ) { return c != '-'; }),
                    last, cstr, std::equal_to<>{});
}

auto cli_opt(std::initializer_list<const char*> xs, string& storage) {
  auto f = [=](const std::string& str) -> optional<const std::string&> {
    for (auto x : xs)
      if (opt_cmp(str.begin(), str.end(), x))
        return str;
    return none;
  };
  return on(f, arg_match) >> [&](string& x) {
    storage = std::move(x);
  };
}

class host_desc {
public:
  std::string host;
  int cpu_slots;
  int gpu_slots;

  host_desc(host_desc&&) = default;
  host_desc(const host_desc&) = default;
  host_desc& operator=(host_desc&&) = default;
  host_desc& operator=(const host_desc&) = default;

  static optional<host_desc> from_string(const std::string& line) {
    auto xs = explode(line, is_any_of(" "), token_compress_on);
    if (xs.empty())
      return none;
    host_desc hd;
    hd.host = std::move(xs.front());
    hd.cpu_slots = static_cast<int>(std::thread::hardware_concurrency());
    hd.gpu_slots = 0;
    for (auto i = xs.begin() + 1; i != xs.end(); ++i) {
      message_builder{explode(*i, is_any_of("="))}.extract({
        on("slots", to_i32) >> [&](int x) {
          hd.cpu_slots = x;
        }
      });
    }
    return hd;
  }

private:
  host_desc() = default;
};

std::vector<host_desc> read_hostfile(const string& fname) {
  std::vector<host_desc> result;
  std::ifstream in{fname};
  std::string line;
  while (std::getline(in, line))
    append(result, host_desc::from_string(line));
  return result;
}

int run_ssh(const string& wdir, const string& cmd, const string& host) {
  // pack command before sending it to avoid any issue with shell escaping
  string full_cmd = "cd ";
  full_cmd += wdir;
  full_cmd += '\n';
  full_cmd += cmd;
  auto packed = encode_base64(full_cmd);
  std::ostringstream oss;
  oss << "ssh -o ServerAliveInterval=60 " << host
      << " echo " << packed << " | base64 --decode | /bin/sh";
  //return system(oss.str().c_str());
  string line;
  auto fp = popen(oss.str().c_str(), "r");
  if (! fp)
    return -1;
  char buf[512];
  auto eob = buf + sizeof(buf); // end-of-buf
  auto pred = [](char c) { return c == 0 || c == '\n'; };
  scoped_actor self;
  while (fgets(buf, sizeof(buf), fp)) {
    auto i = buf;
    auto e = std::find_if(i, eob, pred);
    line.insert(line.end(), i, e);
    while (e != eob && *e != 0) {
      aout(self) << line << std::endl;
      line.clear();
      i = e + 1;
      e = std::find_if(i, eob, pred);
      line.insert(line.end(), i, e);
    }
  }
  pclose(fp);
  if (! line.empty())
    aout(self) << line << std::endl;
  return 0;
}

void bootstrap(const string& wdir,
               const string& master,
               const vector<host_desc>& slaves,
               vector<string> args) {
  using io::network::interfaces;
  auto arg0 = args.front();
  args.erase(args.begin());
  scoped_actor self;
  // open a random port and generate a list of all
  // possible addresses slaves can use to connect to us
  auto port = io::publish(self, 0);
  auto add_port = [=](auto str) {
    str += "/";
    str += std::to_string(port);
    return str;
  };
  for (auto& slave : slaves) {
    // build SSH command and pack it to avoid any issue with shell escaping
    std::thread{[=](actor bootstrapper) {
      std::ostringstream oss;
      oss << arg0
          << " --caf-passive-mode"
          << " --caf-slave-name=" << slave.host
          << " --caf-bootstrap-node="
          << (interfaces::list_addresses(io::network::protocol::ipv4, false)
              + interfaces::list_addresses(io::network::protocol::ipv6, false)
              | map(add_port) | [](auto xs) { return join(xs, ","); })
          << " " << join(args, " ");
      if (run_ssh(wdir, oss.str(), slave.host) != 0)
        anon_send(bootstrapper, error_atom::value, slave.host);
    }, actor{self}}.detach();
  }
  std::string slaveslist;
  for (size_t i = 0; i < slaves.size(); ++i) {
    self->receive(
      [&](ok_atom, const string& host, uint16_t port) {
        if (! slaveslist.empty())
          slaveslist += ',';
        slaveslist += host;
        slaveslist += '/';
        slaveslist += std::to_string(port);
      },
      [](error_atom, const string& node) {
        cerr << "unable to launch process via SSH at node " << node << endl;
      }
    );
  }
  // run (and wait for) master
  std::ostringstream oss;
  oss << arg0 << " --caf-slave-nodes=" << slaveslist << " " << join(args, " ");
  run_ssh(wdir, oss.str(), master);
}

int main(int argc, char** argv) {
  string hostfile;
  std::unique_ptr<char, void (*)(void*)> pwd{getcwd(nullptr, 0), ::free};
  string wdir;
  vector<string> args;
  message_builder{argv + 1, argv + argc}.extract({
    cli_opt({"hostfile", "machinefile"}, hostfile),
    cli_opt({"wdir"}, wdir)
  }).extract([&](string& x) { args.emplace_back(std::move(x)); });
  if (args.empty())
    return cerr << "empty command line" << eom;
  auto get_wdir = [&]() -> const char* {
    return (wdir.empty()) ? pwd.get() : wdir.c_str();
  };
  if (hostfile.empty())
    return cerr << "no hostfile specified" << eom;
  auto hosts = read_hostfile(hostfile);
  if (hosts.empty())
    return cerr << "no valid entry in hostfile" << eom;
  auto master = hosts.front().host;
  hosts.erase(hosts.begin());
  bootstrap((wdir.empty()) ? pwd.get() : wdir.c_str(), master, hosts, args);
  shutdown();
}
