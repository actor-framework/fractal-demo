#ifndef CLIENT_ACTOR_HPP
#define CLIENT_ACTOR_HPP

#include <list>
#include <vector>
#include <chrono>

#include "caf/all.hpp"

#include "image_sink.hpp"
#include "fractal_request.hpp"
#include "fractal_request_stream.hpp"

using test_atom = caf::atom_constant<caf::atom("test")>;
using calc_weights_atom = caf::atom_constant<caf::atom("weights")>;

class client_actor : public caf::event_based_actor {
  //                             worker     weight
  using weight_map  = std::map<caf::actor, double>;
  //                  worker sets
  using worker_sets = std::vector<weight_map>;
  //
  using image_cache = std::vector<std::vector<uint16_t>>;
  //                      image id       image_cache id + offset
  using idx_map = std::map<uint32_t, std::pair<uint32_t, uint32_t>>;
public:
  client_actor(image_sink& sink)
    : worker_sets_()
    , sink_(sink) {
    stream_.init(default_width,
                 default_height,
                 default_min_real,
                 default_max_real,
                 default_min_imag,
                 default_max_imag,
                 default_zoom);
  }

private:

  caf::behavior main_phase() {
    return {
      [=](const std::vector<uint16_t>&, uint32_t) {
        //send to sink  width + data
      },
      caf::others() >> [=]{
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
        auto req = stream_.next();
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
          for (auto& map : worker_sets_) {
            auto total_time = std::accumulate(map.begin(), map.end(), double{0},
                                              add);
            std::cout << "total_time: " << total_time << std::endl;
            for (auto& e : map) {
              double weight = e.second / total_time;
              e.second = weight;
              std::cout << "weight is: " << weight << std::endl;
            }
          }
          // TODO: - buffer 3 seconds
          //       - change behavior to main_phase
        }
      },
      caf::others() >> [=]{
        std::cout << to_string(current_message()) << std::endl;
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
};

#endif // CLIENT_ACTOR_HPP
