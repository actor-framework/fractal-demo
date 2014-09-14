#ifndef COUNTER_HPP
#define COUNTER_HPP

#include <set>
#include <vector>
#include <chrono>

#include <QByteArray>

#include "caf/all.hpp"

class counter : public caf::event_based_actor {
 public:
  counter();
  caf::behavior make_behavior() override;

 private:
  bool probe();
  void init(caf::actor widget, caf::actor ctrl);

  std::uint32_t m_next;
  std::uint32_t m_delay; // in msec
  std::set<std::uint32_t> m_dropped;
  std::map<std::uint32_t, QByteArray> m_buffer;
  std::uint32_t m_buffer_limit;
  std::uint32_t m_idx;
  std::chrono::steady_clock::time_point m_last;
  std::vector<std::chrono::milliseconds> m_intervals;
};

#endif // COUNTER_HPP

