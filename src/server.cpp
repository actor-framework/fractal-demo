#include <map>
#include <tuple>
#include <chrono>
#include <time.h>
#include <vector>
#include <limits>
#include <complex>
#include <cstdlib>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <functional>

#include "caf/all.hpp"

#include "ui_main.h"
#include "server.hpp"
#include "mainwidget.hpp"

using std::endl;
using namespace caf;

// Tick -> time between to frames
// Tock -> one second has passed

std::chrono::milliseconds tick_time(uint32_t fps) {
  return std::chrono::milliseconds(1000 / fps);
}

struct ini_helper {
  template<typename T>
  T operator()(const char* field, const T& default_value) {
    //return ini.get_or_else("fractals", field, default_value);
    return default_value;
  }
};

server::server(atom_value fractal_type)
    : drawn_images_(0),
      draw_pos_(0),
      next_id_(0),
      fractal_type_(fractal_type) {
  ini_helper rd;
  stream_.init(rd("width",    default_width),
               rd("height",   default_height),
               rd("min_real", default_min_real),
               rd("max_real", default_max_real),
               rd("min_imag", default_min_imag),
               rd("max_imag", default_max_imag),
               rd("zoom",     default_zoom));
  iterations_ = rd("iterations", default_iterations);
  // todo: read from config and make configurable at runtime
  fps_ = 2;
  max_pending_images_ = 20;
}

behavior server::make_behavior() {
  aout(this) << "server is " << to_string(actor{this}) << endl;
  trap_exit(true);
  delayed_send(this, tick_time(fps_), atom("Tick"), uint32_t{2});
  delayed_send(this, std::chrono::seconds(1), atom("Tock"));
  return {
    on(atom("Result"), arg_match) >> [=](uint32_t id, const QByteArray& ba) {
      aout(this) << "received image nr. " << id << endl;
      image_cache_.insert(std::make_pair(id, ba));
      auto i = assigned_jobs_.find(id);
      if (i == assigned_jobs_.end()) {
        return;
      }
      auto worker = i->second;
      assigned_jobs_.erase(i);
      if (image_cache_.size() < max_pending_images_) {
        send_next_job(worker);
      }
    },
    on(atom("Tick"), arg_match) >> [=](uint32_t num) {
      if (num < fps_) {
        // the last Tick is followed by a Tock
        delayed_send(this, tick_time(fps_), atom("Tick"), num + 1);
      }
      send_next_image();
    },
    on(atom("Tock")) >> [=] {
      delayed_send(this, tick_time(fps_), atom("Tick"), uint32_t{2});
      delayed_send(this, std::chrono::seconds(1), atom("Tock"));
      aout(this) << drawn_images_ << " FPS" << endl;
      drawn_images_ = 0;
      send_next_image();
    },
    on(atom("SetWorkers"), arg_match) >> [=](std::set<actor>& workers) {
      aout(this) << "new set of " << workers.size() << " worker" << endl;
      workers_.swap(workers);
      while (image_cache_.size() < max_pending_images_) {
        if (!assign_job()) {
          return;
        }
      }
    },
    on(atom("changefrac"), arg_match) >> [&](atom_value new_frac_option) {
      fractal_type_ = new_frac_option;
      // load correct stack for changed fractaltype
      if(fractal_type_  == atom("mandel")) {
          stream_.loop_stack_mandelbrot();
      } else if(fractal_type_ == atom("burnship")) {
          stream_.loop_stack_burning_ship();
      } else if(fractal_type_ == atom("tricorn")) {
          // tricorn needs his own stream
          stream_.loop_stack_mandelbrot();
      } else {
          aout(this) << "[server] couldn't get " <<
                        "coordination for changed fractal." << endl;
      }
    },
    on(atom("resize"), arg_match) >> [=](uint32_t width, uint32_t height) {
      stream_.resize(width, height);
    },
    [=](const exit_msg& msg) {
      auto worker = actor_cast<actor>(msg.source);
      auto i = workers_.find(worker);
      if (i != workers_.end()) {
        aout(this) << "a worker disconnected" << endl;
        for (auto i = assigned_jobs_.begin(); i != assigned_jobs_.end(); ++i) {
          if (i->second == worker) {
            // drop everything up until lost job id
            draw_pos_ = i->first + 1;
            auto j = image_cache_.begin();
            while (j != image_cache_.end() && j->first < draw_pos_) {
              image_cache_.erase(j);
              j = image_cache_.begin();
            }
            assigned_jobs_.erase(i);
            return;
          }
        }
      } else {
        // an unknown link has been killed, fall back to default behavior
        quit(msg.reason);
      }
    },
    on(atom("SetSink"), arg_match) >> [=](const actor& sink) {
      image_sink_ = sink;
    },
    others >> [=] {
      aout(this) << "[!!!] server received unexpected message: '"
                 << to_string(current_message())
                 << "'." << endl;
    }
  };
}

void server::send_next_job(const actor& worker) {
  if (stream_.at_end()) {
    // stream endlessly
    if (fractal_type_ == atom("mandel")) {
      stream_.loop_stack_mandelbrot();
    } else if (fractal_type_ == atom("burnship")) {
      stream_.loop_stack_burning_ship();
    } else if (fractal_type_ == atom("tricorn")) {
      // tricorn needs his own stream
      stream_.loop_stack_mandelbrot();
    } else {
      aout(this) << "[server] Reached end of stream end and didn't "
                 << "found a new one ... quit" << endl;
      quit(exit_reason::user_defined);
      return;
    }
  }
  auto next_id = next_id_++;
  auto& fr = stream_.next();
  send(worker,
       fractal_type_,
       this,
       width(fr),
       height(fr),
       iterations_,
       next_id,
       min_re(fr),
       max_re(fr),
       min_im(fr),
       max_im(fr));
  assigned_jobs_[next_id] = worker;
}

void server::send_next_image() {
  auto i = image_cache_.find(draw_pos_);
  if (i != image_cache_.end()) {
    send(image_sink_, atom("Image"), i->second);
    image_cache_.erase(i);
    ++draw_pos_;
    ++drawn_images_;
    assign_job(); // maybe we can assign a new job now
  }
}

bool server::assign_job() {
  if (workers_.size() <= assigned_jobs_.size()) {
    return false;
  }
  for (auto& w : workers_) {
    auto pred = [w](const job_map::value_type& kvp) {
      return kvp.second == w;
    };
    auto last = assigned_jobs_.end();
    auto i = std::find_if(assigned_jobs_.begin(), last, pred);
    if (i == last) {
      // worker has no job assigned yet
      send_next_job(w);
      return true;
    }
  }
  return false;
}
