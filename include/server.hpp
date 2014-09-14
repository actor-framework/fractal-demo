#ifndef SERVER_HPP
#define SERVER_HPP

#include <set>
#include <map>
#include <vector>
#include <functional>

#include <QLabel>
#include <QObject>
#include <QPicture>
#include <QByteArray>

#include "caf/all.hpp"

#include "imagelabel.h"
#include "mainwidget.hpp"
#include "config_map.hpp"
#include "fractal_request.hpp"
#include "fractal_request_stream.hpp"

class server : public caf::event_based_actor {
 public:
  server(config_map& config, caf::actor counter,
         caf::atom_value fractal_type_atom);

  caf::behavior make_behavior() override;

 private:
  std::uint32_t m_interval; // in msecs
  std::uint32_t m_queuesize;
  std::uint32_t m_next_id;
  // std::uint32_t m_assign_id;
  std::uint32_t m_iterations;

  caf::atom_value m_fractal_type_atom;
  caf::actor m_counter;

  std::set<caf::actor> m_workers;
  std::map<caf::actor, std::uint32_t> m_jobs;

  fractal_request_stream m_stream;

  void send_next_job(const caf::actor& worker);
};

#endif
