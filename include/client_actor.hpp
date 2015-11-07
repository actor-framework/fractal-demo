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
      sink_(sink) {
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
    //uint32_t fr_width  = width(fr);
    //uint32_t fr_height = height(fr);
    uint32_t fr_width = image_width_;
    uint32_t fr_height = image_height_;
    auto fr_min_re = min_re(fr);
    auto fr_max_re = max_re(fr);
    auto fr_min_im = min_im(fr);
    auto fr_max_im = max_im(fr);
    // share data
    uint32_t current_row = 0;
    size_t current_idx = 0;
    for (auto& e : workers_) {
      auto& w = e.first;
      auto weight = e.second;
      uint32_t rows_to_share = 0;
      if (current_idx != workers_.size() - 1)
        rows_to_share = image_height_ * weight;
      else
        rows_to_share = image_height_ - current_row;
      send(w, default_iterations, fr_width, fr_height,
           current_row, rows_to_share, fr_min_re, fr_max_re, fr_min_im, fr_max_im);
      current_row += rows_to_share;
    }
    pending_chunks_ = workers_.size();
  }

  std::vector<uint16_t> last_entry;

  void concat_data(const std::vector<uint16_t>& data, uint32_t offset) {
    chunk_cache_.push_back(std::make_pair(offset,data));
    if (chunk_cache_.size() == pending_chunks_) {
      // All parts recieved, sort and concat vectors
      auto cmp = [&](const std::pair<uint32_t, std::vector<uint16_t>>& a,
                     const std::pair<uint32_t, std::vector<uint16_t>>& b) {
        return a.first < b.first;
      };
      std::sort(chunk_cache_.begin(), chunk_cache_.end(), cmp);
      std::vector<uint16_t> entry;
      for (auto& e : chunk_cache_)
        entry.insert(entry.end(), e.second.begin(), e.second.end());
      chunk_cache_.clear();
      std::swap(last_entry, entry);
      //send(sink_, image_width_, entry);
      send_job();
    }
  }

  void resize(uint32_t width, uint32_t height) {
    image_width_ = width;
    image_height_ = height;
    image_size_ = image_width_ * image_height_;
    /*stream_.init(width,
                 height,
                 default_min_real,
                 default_max_real,
                 default_min_imag,
                 default_max_imag,
                 default_zoom);*/
    //send(this, calc_weights_atom::value, workers_.size());
  }

  void calculate_weights(uint32_t workers) {
    workers_.clear();
    uint32_t idx = 0;
    double total_times = 0;
    for (auto& e : all_workers_) {
      if (++idx > workers) break;
      total_times += e.second;
      workers_.insert(std::make_pair(e.first, 0));
    }
    idx = 0;
    for (auto& e : all_workers_) {
      if (++idx > workers) break;
      workers_[e.first] = e.second / total_times;
    }
  }


  caf::behavior make_behavior() override {
    auto init = std::make_shared<bool>(false);
    auto left = std::make_shared<int>(0);
    delayed_send(this, std::chrono::seconds(16), tick_atom::value);
    return {
      [=](init_atom, const std::vector<caf::actor>& all_workers) {
        for (auto& w : all_workers)
          all_workers_.push_back(std::make_pair(w, 0));
        send(this, calc_weights_atom::value);
      },
      [=](calc_weights_atom) {
        std::cout << "calc weight" << std::endl;
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
        *left = all_workers_.size();
        for (auto& e : all_workers_) {
          auto& worker = e.first;
          times_.insert(std::make_pair(worker, std::chrono::high_resolution_clock::now()));
          send(worker, default_iterations, req_width, req_height,
               offset, rows, req_min_re, req_max_re, req_min_im, req_max_im);
        }
      },
      [=](tick_atom) {
        send(sink_, image_width_, last_entry);
        std::chrono::milliseconds rate;
        switch(image_width_) {
        case 800:
          rate = std::chrono::milliseconds(100); // 10 fps
          break;
        case 1024:
          rate = std::chrono::milliseconds(150); // 6,7 fps
          break;
        case 1280:
          rate = std::chrono::milliseconds(200); // 5 fps
        case 1680:
          rate = std::chrono::milliseconds(250); // 4 fps
        case 1920:
          rate = std::chrono::milliseconds(300); // 3 fps
        case 2560:
        default:
          rate = std::chrono::milliseconds(500); // 2 fps
        }
        delayed_send(this, rate, tick_atom::value);
      },
      [=](const std::vector<uint16_t>& data, uint32_t offset) {
        if (! *init) {
          // finishe init
          auto sender = caf::actor_cast<caf::actor>(current_sender());
          for (auto& w : all_workers_) {
            if (w.first == sender) {
              auto t2 = std::chrono::high_resolution_clock::now();
              auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - times_[sender]).count();
              w.second = diff;
              break;
            }
          }
          //
          --*left;
          if (*left == 0) {
            *init = true;
            calculate_weights(all_workers_.size());
            send_job();
          }
        } else {
          concat_data(data, offset);
        }
      },
      [=](resize_atom, uint32_t w, uint32_t h) {
        resize(w, h);
      },
      [=](limit_atom, normal_atom, uint32_t workers) {
        calculate_weights(workers);
      },
      caf::others() >> [=] {
        std::cout << to_string(current_message()) << std::endl;
      }
    };
  }

  //
  std::map<caf::actor, std::chrono::time_point<std::chrono::high_resolution_clock>> times_ = {};
  std::vector<std::pair<caf::actor, size_t>> all_workers_ = {};
  size_t pending_chunks_ = 0;
  //
  weight_map workers_ = {};
  // image properties
  uint32_t image_height_ = 0;
  uint32_t image_width_  = 0;
  uint32_t image_size_   = 0;
  // fractal stream
  fractal_request_stream stream_;
  //
  image_sink& sink_;
  chunk_cache chunk_cache_ = {};
};

#endif // CLIENT_ACTOR_HPP
