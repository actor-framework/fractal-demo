#ifndef CLIENT_ACTOR_HPP
#define CLIENT_ACTOR_HPP

#include <queue>
#include <vector>
#include <chrono>

#include "caf/all.hpp"

#include "atoms.hpp"
#include "image_sink.hpp"
#include "fractal_request.hpp"
#include "fractal_request_stream.hpp"

class client_actor : public caf::event_based_actor {
  //                            worker     weight
  using weight_map  = std::map<caf::actor, double>;
  //                                          buffer
  using image_cache = std::queue<std::vector<uint16_t>>;
  //                                        offset     vector
  using chunk_cache = std::vector<std::pair<uint32_t, std::vector<uint16_t>>>;

public:
  client_actor(image_sink& sink, uint32_t seconds_to_buffer,
               size_t image_height, size_t image_width)
    : image_height_(image_height),
      image_width_(image_width),
      image_size_(image_height_ * image_width_),
      sink_(sink),
      cache_(),
      chunk_cache_(),
      seconds_to_buffer_(seconds_to_buffer),
      buffer_min_size_(0) {
    stream_.init(default_width,
                 default_height,
                 default_min_real,
                 default_max_real,
                 default_min_imag,
                 default_max_imag,
                 default_zoom);
  }

private:

  void send_job() {
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
    uint32_t shared_rows = 0;
    uint32_t current_row = 0;
    size_t current_idx = 0;
    for (auto& e : workers_) {
      auto& w = e.first;
      auto weight = e.second;
      uint32_t rows_to_share = 0;
      if (++current_idx < workers_.size()) {
        rows_to_share = static_cast<uint32_t>(image_height_ * weight);
        shared_rows += rows_to_share;
      } else {
        rows_to_share = image_height_ - shared_rows;
        shared_rows += rows_to_share;
      }
      send(w, default_iterations, fr_width, fr_height,
           current_row, rows_to_share, fr_min_re, fr_max_re, fr_min_im, fr_max_im);
      current_row += rows_to_share;
    }
  }

  void concat_data(const std::vector<uint16_t>& data, uint32_t offset) {
    chunk_cache_.push_back(std::make_pair(offset,data));
    if (chunk_cache_.size() == workers_.size()) {
      // All parts recieved, sort and concat vectors
      auto cmp = [&](const std::pair<uint32_t, std::vector<uint16_t>>& a,
                     const std::pair<uint32_t, std::vector<uint16_t>>& b) {
        return a.first < b.first;
      };
      std::sort(chunk_cache_.begin(), chunk_cache_.end(), cmp);
      std::vector<uint16_t> entry;
      for (auto& e : chunk_cache_)
        entry.insert(entry.end(), e.second.begin(), e.second.end());
      cache_.emplace(std::move(entry));
      chunk_cache_.clear();
    }
  }

  void resize(uint32_t width, uint32_t height) {
    image_width_ = width;
    image_height_ = height;
    image_size_ = image_width_ * image_height_;
    std::vector<caf::actor> worker_set;
    for (auto& w : workers_)
      worker_set.push_back(w.first);
    stream_.init(width,
                 height,
                 default_min_real,
                 default_max_real,
                 default_min_imag,
                 default_max_imag,
                 default_zoom);
    become(make_behavior());
    send(this, calc_weights_atom::value, worker_set, worker_set.size());
  };

  caf::behavior main_phase() {
    send(this, tick_atom::value);
    return {
      [=](const std::vector<uint16_t>& data, uint32_t id) {
        concat_data(data, id);
      },
      [=](tick_atom) {
        delayed_send(this, tick_rate_, tick_atom::value);
        if (cache_.empty()) {
          std::cout << "[WARNING] Cache empty..." << std::endl;
          return;
        }
        send(sink_, image_width_, cache_.front());
        cache_.pop();
        if (cache_.size() < buffer_max_size_)
          send_job();
      },
      [=](resize_atom, uint32_t w, uint32_t h) {
        resize(w,h);
      },
      caf::others() >> [=] {
        std::cout << to_string(current_message()) << std::endl;
      }
    };
  }

  caf::behavior init_buffer() {
    // Initial send tasks
    auto pending_chunks = std::make_shared<int>(workers_.size());
    auto pending_jobs   = std::make_shared<int>(buffer_min_size_);
    send_job();
    return {
      [=](const std::vector<uint16_t>& data, uint32_t id) {
        concat_data(data, id);
        if (! --*pending_chunks) {
          if (--*pending_jobs) {
           send_job();
            *pending_chunks = workers_.size();
          } else {
            become(main_phase());
          }
        }
      },
      [=](resize_atom, uint32_t w, uint32_t h) {
        resize(w,h);
      },
      caf::others() >> [=] {
        std::cout << to_string(current_message()) << std::endl;
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
          size_t workers_to_use) {
        // Clear all caches
        chunk_cache_.clear();
        image_cache empty;
        std::swap(cache_, empty);
        start_map->clear();
        workers_.clear();
        //
        for (size_t i = 0; i < workers_to_use; ++i)
          workers_.emplace(workers[i], 0);
        using hrc = std::chrono::high_resolution_clock;
        auto req = stream_.next(); // TODO: Get a image with much black
        uint32_t req_width = width(req);
        uint32_t req_height = height(req);
        auto req_min_re = min_re(req);
        auto req_max_re = max_re(req);
        auto req_min_im = min_im(req);
        auto req_max_im = max_im(req);
        uint32_t offset = 0;
        uint32_t rows = image_height_;
        for (auto& e : workers_) {
          auto& worker = e.first;
          start_map->emplace(worker, hrc::now());
          send(worker, default_iterations, req_width, req_height,
               offset, rows, req_min_re, req_max_re, req_min_im, req_max_im);
        }
      },
      [=](const std::vector<uint16_t>& data, uint32_t) {
        if (data.size() != image_size_) return; // old data TODO: Problem with 1 worker?
        auto sender = caf::actor_cast<caf::actor>(current_sender());
        auto t2 = std::chrono::high_resolution_clock::now();
        auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - (*start_map)[sender]).count();
        workers_[sender] = diff;
        start_map->erase(sender);
        if (start_map->empty()) {
          auto add = [](size_t lhs, const std::pair<caf::actor, double>& rhs) {
            return lhs + rhs.second;
          };
          auto total_time = std::accumulate(workers_.begin(), workers_.end(),
                                            size_t{0}, add);
          for (auto& e : workers_)
            e.second = e.second / total_time;
          total_time /= workers_.size();
          auto ms  = static_cast<double>(std::chrono::milliseconds(total_time).count());
          auto sec = static_cast<double>(std::chrono::milliseconds(1000).count());
          double fps = sec / ms;
          tick_rate_ = std::chrono::milliseconds(total_time);
          std::cout << "Assumed FPS: " << fps << std::endl;
          buffer_min_size_ = fps < 1 ? static_cast<uint32_t>(1.0 / fps)
                                     : static_cast<uint32_t>(fps);
          buffer_min_size_ *= seconds_to_buffer_; // FIXME
          buffer_max_size_ = buffer_min_size_ * 4;
          become(init_buffer());
        }
      },
      [=](resize_atom, uint32_t w, uint32_t h) {
        resize(w,h);
      },
      caf::others() >> [=] {
        std::cout << to_string(current_message()) << std::endl;
      }
    };
  }
  //
  weight_map workers_;
  // image properties
  uint32_t image_height_;
  uint32_t image_width_;
  uint32_t image_size_;
  // fractal stream
  fractal_request_stream stream_;
  //
  image_sink& sink_;
  // buffer
  image_cache cache_;
  chunk_cache chunk_cache_;
  //
  uint32_t seconds_to_buffer_;
  uint32_t buffer_min_size_;
  uint32_t buffer_max_size_;
  // tick
  std::chrono::milliseconds tick_rate_;
};

#endif // CLIENT_ACTOR_HPP
