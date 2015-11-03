#ifndef CLIENT_ACTOR_HPP
#define CLIENT_ACTOR_HPP

#include <list>
#include <vector>
#include <chrono>

#include "caf/all.hpp"

#include "image_sink.hpp"
#include "fractal_request.hpp"
#include "fractal_request_stream.hpp"

using calc_weights_atom = caf::atom_constant<caf::atom("weights")>;
using tick_atom = caf::atom_constant<caf::atom("tick")>;

class client_actor : public caf::event_based_actor {
  //                             worker     weight
  using weight_map  = std::map<caf::actor, double>;
  //                  worker sets
  using worker_sets = std::vector<weight_map>;
  //
  using image_cache = std::vector<std::vector<uint16_t>>;
  //                      image id      image_cache id + offset + missing chunks
  using idx_map = std::map<uint32_t, std::tuple<uint32_t, uint32_t, uint32_t>>;
  //                         image id /  worker_set idx
  using job_goup_map = std::map<uint32_t, size_t>;

public:
  client_actor(image_sink& sink, uint32_t seconds_to_buffer,
               size_t image_height, size_t image_width)
    : worker_sets_()
    , image_height_(image_height)
    , image_width_(image_width)
    , image_size_(image_height_ * image_width)
    , sink_(sink)
    , cache_()
    , idx_map_()
    , seconds_to_buffer_(seconds_to_buffer)
    , buffer_min_size_(0)
    , current_read_idx_(0)
    , current_write_idx_(0) {
    stream_.init(default_width,
                 default_height,
                 default_min_real,
                 default_max_real,
                 default_min_imag,
                 default_max_imag,
                 default_zoom);
  }

private:
  void send_job(size_t worker_set_idx) {
    std::cout << "Send job to worker group: " << worker_set_idx << std::endl;
    auto fr = stream_.next();
    auto& worker_set = worker_sets_[worker_set_idx]; // send job to correct set
    uint32_t rows = image_width_;
    uint32_t shared_rows = 0;
    std::cout << "rows: " << rows << std::endl;
    size_t current_idx = 0;
    for (auto& e : worker_set) {
      auto& w = e.first;
      auto weight = e.second;
      uint32_t rows_to_share = 0;
      if (++current_idx < worker_set.size()) {
        rows_to_share = static_cast<uint32_t>(rows * weight);
        shared_rows += rows_to_share;
      } else {
        rows_to_share = rows - shared_rows;
        shared_rows += rows_to_share;
      }
      std::cout << "Weight: " << weight << ", rows: " << rows * weight
                << " of total " << rows << " rows. (rows_to_share: "
                << rows_to_share << " , shared_rows: " << shared_rows << ")."
                << std::endl;
      // TODO:
      // - Send chunks
      // - update job_group_map_
      // - update idx_map_ !! offset must be bound to the type size !!
    }
    std::cout << "Shared " << shared_rows << " of total " << rows << "."
              << std::endl;
  }

  void concat_data(const std::vector<uint16_t>& data, uint32_t id) {
    auto cache_idx    = std::get<0>(idx_map_[id]);
    auto cache_offset = std::get<1>(idx_map_[id]);
    auto missing_chunks = (std::get<2>(idx_map_[id]) -= 1);
    auto& cache_entry = cache_[cache_idx];
    *(cache_entry.data() + cache_offset) = *data.data();
    if (missing_chunks == 0) {
      // TODO: ready to send
      std::cout << "Image is complete (all chuncks recieved)." << std::endl;
    }
  }

  caf::behavior main_phase() {
    return {
      [=](const std::vector<uint16_t>& data, uint32_t id) {
        concat_data(data, id);
        send_job(job_group_map_[id]);
      },
      [=](tick_atom) {
        // TODO:
        // send(sink_, ...)
        // delayed_send(self, tick_atom::value, ...)
        // buffer_clear(...)
      },
      caf::others >> [=]{
        std::cout << "main_phase() " << to_string(current_message())
                  << std::endl;
      }
    };
  }

  caf::behavior init_buffer() {
    // Initial send tasks
    for (size_t i = 0; i < worker_sets_.size(); ++i)
      send_job(i);
    return {
      [=](const std::vector<uint16_t>& data, uint32_t id) {
        concat_data(data, id);
        if (cache_.size() < buffer_min_size_)
          send_job(job_group_map_[id]);
        else
          become(main_phase());
      },
      caf::others() >> [=] {
        std::cout << "init_buffer() " << to_string(current_message())
                  << std::endl;
      }
    };
  }

  caf::behavior make_behavior() override {
    auto start_map = std::make_shared<
                       std::map<caf::actor,
                                std::chrono::time_point<
                                  std::chrono::high_resolution_clock>
                                >
                       >();
    return {
      [=](calc_weights_atom, const std::vector<caf::actor>& workers,
          uint32_t workers_per_set) {
        // TODO: Split into multiple sets if more then `worker_per_set` workers
        // Initial fill map with workers
        for (size_t sets = 0; sets < 1; ++sets) {
          weight_map map;
          for (auto& w : workers)
            map.emplace(w, 0);
          worker_sets_.push_back(map);
        }
        using hrc = std::chrono::high_resolution_clock;
        auto req = stream_.next(); // TODO: Get a image with much black
        uint32_t req_width = width(req);
        uint32_t req_height = height(req);
        auto req_min_re = min_re(req);
        auto req_max_re = max_re(req);
        auto req_min_im = min_im(req);
        auto req_max_im = max_im(req);
        uint16_t iter = default_iterations;
        uint32_t offset = 0;
        uint32_t rows = 700;
        for (auto& maps : worker_sets_) {
          for (auto& e : maps) {
            auto& worker = e.first;
            start_map->emplace(worker, hrc::now());
            send(worker, uint32_t{0}, iter, req_width, req_height, offset, rows,
                                 req_min_re,
                                 req_max_re,
                                 req_min_im,
                                 req_max_im);
          }
        }
      },
      [=](const std::vector<uint16_t>&, uint32_t) {
        std::cout << "got result" << std::endl;
        auto sender = caf::actor_cast<caf::actor>(current_sender());
        auto t2 = std::chrono::high_resolution_clock::now();
        auto diff = std::chrono::duration_cast<std::chrono::microseconds>(t2 - (*start_map)[sender]).count();
        for (auto& map : worker_sets_) {
          auto iter = map.find(sender);
          if (iter != map.end()) {
            map[sender] = diff;
            start_map->erase(sender);
            break;
          }
        }
        if (start_map->empty()) {
          std::cout << "got all results" << std::endl;
          auto add = [](double lhs, const std::pair<caf::actor, double>& rhs) {
            return lhs + rhs.second;
          };
          double total_time_all = 0;
          for (auto& map : worker_sets_) {
            auto total_time = std::accumulate(map.begin(), map.end(), double{0},
                                              add);
            std::cout << "total_time: " << total_time << std::endl;
            for (auto& e : map) {
              double weight = e.second / total_time;
              e.second = weight;
              std::cout << "weight is: " << weight << std::endl;
            }
            total_time_all += total_time; // Total time with all worker sets
          }
          // TODO:
          // - Calculate buffer_min_size_
          // - preallocate memory in cache_
          std::cout << "bleh: " << 1/total_time_all << std::endl;
          auto fps = static_cast<uint16_t>((1/total_time_all) * 100000);
          std::cout << "Assumed FPS: " << fps << std::endl;
          buffer_min_size_ = (fps * seconds_to_buffer_) + 1; // +1 to be sure
          current_read_idx_  = 0;
          current_write_idx_ = 0;
          become(init_buffer());
        }
      },
      caf::others() >> [=]{
        std::cout << "make_behavior()" << to_string(current_message())
                  << std::endl;
      }
    };
  }
  //
  worker_sets worker_sets_;
  // image properties
  size_t image_height_;
  size_t image_width_;
  size_t image_size_;
  // fractal stream
  fractal_request_stream stream_;
  //
  image_sink& sink_;
  // buffer
  image_cache cache_;
  idx_map idx_map_;
  job_goup_map job_group_map_; // store which group worked on a image
  uint32_t seconds_to_buffer_;
  uint32_t buffer_min_size_; // minimum entries for 3 secs
                             // ^^ can be used as ringbuffer
  uint32_t current_read_idx_;  //
  uint32_t current_write_idx_; //
};

#endif // CLIENT_ACTOR_HPP
