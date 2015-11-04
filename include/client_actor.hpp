#ifndef CLIENT_ACTOR_HPP
#define CLIENT_ACTOR_HPP

#include <list>
#include <vector>
#include <chrono>

#include "caf/all.hpp"

#include "atoms.hpp"
#include "image_sink.hpp"
#include "fractal_request.hpp"
#include "fractal_request_stream.hpp"

class client_actor : public caf::event_based_actor {
  //                             worker     weight
  using weight_map  = std::map<caf::actor, double>;
  //                  worker sets
  using worker_sets = std::vector<weight_map>;
  //                            img_id     buffer
  using image_cache = std::map<uint32_t, std::vector<uint16_t>>;
  //                       img id,            cache id + offset
  using id_map = std::map<uint32_t, std::pair<uint32_t, uint32_t>>;
  //                            img id     missing
  using missing_list = std::map<uint32_t, uint32_t>;
  //                            img_id     worker_set_id
  using job_group_map = std::map<uint32_t, uint32_t>;

public:
  client_actor(image_sink& sink, uint32_t seconds_to_buffer,
               size_t image_height, size_t image_width)
    : worker_sets_(),
      image_height_(image_height),
      image_width_(image_width),
      image_size_(image_height_ * image_width),
      img_id_(0),
      chunk_id_(0),
      sink_(sink),
      cache_(),
      id_map_(),
      missing_list_(),
      job_group_map_(),
      seconds_to_buffer_(seconds_to_buffer),
      buffer_min_size_(0),
      current_read_idx_(0),
      buffer_fill_state_(0) {
    stream_.init(default_width,
                 default_height,
                 default_min_real,
                 default_max_real,
                 default_min_imag,
                 default_max_imag,
                 default_zoom);
  }

private:

  void allocate_cache(uint32_t idx) {
    // allocate cache for image data
    std::vector<uint16_t> buf(image_size_);
    cache_.emplace(idx, std::move(buf));
  }

  void send_job(size_t worker_set_idx) {
    auto& worker_set = worker_sets_[worker_set_idx]; // send job to correct set
    auto img_id = img_id_++;
    allocate_cache(img_id);
    // add missing list entry
    missing_list_.emplace(img_id, worker_set.size());
    // update job_group mapping
    job_group_map_.emplace(img_id, worker_set_idx);
    // get next from stream
    if (stream_.at_end())
      stream_.loop_stack_mandelbrot(); // TODO: Replace with current fractal
    auto fr = stream_.next();
    uint32_t fr_width  = width(fr);
    uint32_t fr_height = height(fr);
    auto fr_min_re = min_re(fr);
    auto fr_max_re = max_re(fr);
    auto fr_min_im = min_im(fr);
    auto fr_max_im = max_im(fr);
    // share data
    uint32_t rows = image_width_;
    uint32_t shared_rows = 0;
    uint32_t current_row = 0;
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
      //std::cout << "Weight: " << weight << ", rows: " << rows * weight
      //          << " of total " << rows << " rows. (rows_to_share: "
      //          << rows_to_share << " , shared_rows: " << shared_rows << ")."
      //          << std::endl;
      auto chunk_id = chunk_id_++;
      id_map_.emplace(chunk_id, std::make_pair(img_id, current_row));
      send(w, chunk_id_, default_iterations, fr_width, fr_height,
           current_row, rows_to_share, fr_min_re, fr_max_re, fr_min_im, fr_max_im);
      current_row += rows_to_share;
    }
  }

  void concat_data(const std::vector<uint16_t>& data, uint32_t chunk_id) {
    auto& id_map_entry = id_map_[chunk_id];
    auto  cache_idx    = id_map_entry.first;
    auto  cache_offset = id_map_entry.second;
    auto& cache_entry  = cache_[cache_idx];
    for (size_t idx = 0; idx < data.size(); ++idx)
      cache_entry[idx + cache_offset] = data[idx];
    if (--missing_list_[cache_idx] == 0) {
      // TODO:
      // Mark as complete
      // ==> Free buffer when send
      buffer_fill_state_++;
      if (buffer_fill_state_ >= buffer_min_size_) {
        become(main_phase());
      }
    }
  }

  caf::behavior main_phase() {
    delayed_send(this, std::chrono::seconds(5), tick_atom::value);
    return {
      [=](const std::vector<uint16_t>& data, uint32_t id) {
        concat_data(data, id);
        send_job(job_group_map_[id]);
      },
      [=](tick_atom) {
        std::cout << "tick..." << std::endl;
        // TODO:
        // send(sink_, ...)
        // delayed_send(self, tick_atom::value, ...)
        // buffer_clear(...)
        send(sink_, image_width_, cache_[current_read_idx_++]);
        std::cout << "Send cache entry size: " << cache_[current_read_idx_-1].size();
        delayed_send(this, std::chrono::seconds(6), tick_atom::value);
        // TODO: Clear buffers
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
        uint32_t rows = image_width_;
        for (auto& maps : worker_sets_) {
          for (auto& e : maps) {
            auto& worker = e.first;
            start_map->emplace(worker, hrc::now());
            send(worker, uint32_t{0}, default_iterations, req_width, req_height,
                 offset, rows, req_min_re, req_max_re, req_min_im, req_max_im);
          }
        }
      },
      [=](const std::vector<uint16_t>&, uint32_t) {
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
          double fps = 0;
          //if (total_time_all > 1)
          //  fps =
          //else
            fps = (1/total_time_all) * 100000;
          std::cout << "Assumed FPS: " << fps << std::endl;
          //buffer_min_size_ = (fps * seconds_to_buffer_) + 1; // +1 to be sure
          buffer_min_size_ = 15; // FIXME
          current_read_idx_ = 0;
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
  uint32_t image_height_;
  uint32_t image_width_;
  uint32_t image_size_;
  // fractal stream
  fractal_request_stream stream_;
  uint32_t img_id_;
  uint32_t chunk_id_;
  //
  image_sink& sink_;
  // buffer
  image_cache cache_; // <uint32_t /* image id */, vector<uint16_t> /* data */>
  id_map id_map_; // <uint32_t /* image id */, <uint32_t /* cache idx */, size_t offset>>
  missing_list missing_list_; // <uint32_t /* image id */, size_t /* missing */>
  job_group_map job_group_map_; // <uint32_t /* image_id */
  //
  uint32_t seconds_to_buffer_;
  uint32_t buffer_min_size_; // minimum entries for 3 secs
                             // ^^ can be used as ringbuffer
  uint32_t current_read_idx_;  //
  uint32_t buffer_fill_state_;
};

#endif // CLIENT_ACTOR_HPP
