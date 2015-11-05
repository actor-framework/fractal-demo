#ifndef SERVER_HPP
#define SERVER_HPP

#include <set>
#include <map>
#include <vector>
#include <cstdint>

#include <QByteArray>

#include "caf/all.hpp"

#include "fractal_request_stream.hpp"

class server : public caf::event_based_actor {
 public:
  using job_id = uint32_t;

  struct worker_entry {
    caf::actor handle;
    caf::maybe<job_id> assigned_job;
  };

  server(caf::atom_value fractal_type);

  caf::behavior make_behavior() override;

 private:
  using job_map = std::map<job_id, caf::actor>;
  // assign_job
  bool assign_job();
  // create and send a new job
  void send_next_job(const caf::actor& whom);
  // send the image from our cache to the sink
  void send_next_image();
  // keeps track of how many images we've drawn this second
  size_t drawn_images_;
  // configured frames per second
  uint32_t fps_;
  // maximum number of images we are allowed to cache
  size_t max_pending_images_;
  // current position for drawing in the image stream
  job_id draw_pos_;
  // ID for the next image to be computed
  job_id next_id_;
  // number of iterations in the computation of our fractal
  uint32_t iterations_;
  // determines what kind of fractal we are distributing
  caf::atom_value fractal_type_;
  // some actor needs to draw our pretty images
  caf::actor image_sink_;
  // sorted according to the number of jobs a worker has
  std::set<caf::actor> workers_;
  // currently assigned jobs
  job_map assigned_jobs_;
  // orphaned jobs that no longer produce a result
  std::set<job_id> dropped_jobs_;
  // generator object for our jobs
  fractal_request_stream stream_;
  // caches incoming images until we actually draw them
  std::map<job_id, QByteArray> image_cache_;
};

#endif
