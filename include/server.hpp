#ifndef SERVER_HPP
#define SERVER_HPP

#include <set>
#include <map>
#include <vector>
#include <cstdint>

#include <QByteArray>

#include "caf/all.hpp"

#include "config_map.hpp"
#include "fractal_request_stream.hpp"

class server : public caf::event_based_actor {
 public:
  using job_id = uint32_t;

  struct worker_entry {
    caf::actor handle;
    caf::optional<job_id> assigned_job;
  };

  server(config_map& config, caf::atom_value fractal_type);

  caf::behavior make_behavior() override;

 private:
  // assign_job
  bool assign_job();
  // create and send a new job
  void send_next_job(const caf::actor& whom);
  // send the image from our cache to the sink
  void send_next_image();
  // keeps track of how many images we've drawn this second
  size_t m_drawn_images;
  // configured frames per second
  uint32_t m_fps;
  // maximum number of images we are allowed to cache
  size_t m_max_pending_images;
  // current position for drawing in the image stream
  job_id m_draw_pos;
  // ID for the next image to be computed
  job_id m_next_id;
  // number of iterations in the computation of our fractal
  uint32_t m_iterations;
  // determines what kind of fractal we are distributing
  caf::atom_value m_fractal_type;
  // some actor needs to draw our pretty images
  caf::actor m_image_sink;
  // sorted according to the number of jobs a worker has
  std::set<caf::actor> m_workers;
  // currently assigned jobs
  std::map<job_id, caf::actor> m_assigned_jobs;
  // orphaned jobs that no longer produce a result
  std::set<job_id> m_dropped_jobs;
  // generator object for our jobs
  fractal_request_stream m_stream;
  // caches incoming images until we actually draw them
  std::map<job_id, QByteArray> m_image_cache;
};

#endif
