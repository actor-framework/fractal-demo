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
#include "config_map.hpp"
#include "mainwidget.hpp"
#include "q_byte_array_info.hpp"

using std::endl;
using namespace caf;

// Tick -> time between to frames
// Tock -> one second has passed

std::chrono::milliseconds tick_time(uint32_t fps) {
  return std::chrono::milliseconds(1000 / fps);
}

struct ini_helper {
  config_map& ini;
  ini_helper(config_map& cmap) : ini(cmap) { }
  template<typename T>
  T operator()(const char* field, const T& default_value) {
    return ini.get_or_else("fractals", field, default_value);
  }
};

server::server(config_map& ini, atom_value fractal_type)
    : m_drawn_images(0),
      m_draw_pos(0),
      m_next_id(0),
      m_fractal_type(fractal_type) {
  ini_helper rd{ini};
  m_stream.init(rd("width",    default_width),
                rd("height",   default_height),
                rd("min_real", default_min_real),
                rd("max_real", default_max_real),
                rd("min_imag", default_min_imag),
                rd("max_imag", default_max_imag),
                rd("zoom",     default_zoom));
  m_iterations = rd("iterations", default_iterations);
  // todo: read from config and make configurable at runtime
  m_fps = 2;
  m_max_pending_images = 20;
}

behavior server::make_behavior() {
  aout(this) << "server is " << to_string(actor{this}) << endl;
  trap_exit(true);
  delayed_send(this, tick_time(m_fps), atom("Tick"), uint32_t{2});
  delayed_send(this, std::chrono::seconds(1), atom("Tock"));
  return {
    on(atom("Result"), arg_match) >> [=](uint32_t id, const QByteArray& ba) {
      aout(this) << "received image nr. " << id << endl;
      m_image_cache.insert(std::make_pair(id, ba));
      auto i = m_assigned_jobs.find(id);
      if (i == m_assigned_jobs.end()) {
        return;
      }
      auto worker = i->second;
      m_assigned_jobs.erase(i);
      if (m_image_cache.size() < m_max_pending_images) {
        send_next_job(worker);
      }
    },
    on(atom("Tick"), arg_match) >> [=](uint32_t num) {
      if (num < m_fps) {
        // the last Tick is followed by a Tock
        delayed_send(this, tick_time(m_fps), atom("Tick"), num + 1);
      }
      send_next_image();
    },
    on(atom("Tock")) >> [=] {
      delayed_send(this, tick_time(m_fps), atom("Tick"), uint32_t{2});
      delayed_send(this, std::chrono::seconds(1), atom("Tock"));
      aout(this) << m_drawn_images << " FPS" << endl;
      m_drawn_images = 0;
      send_next_image();
    },
    on(atom("SetWorkers"), arg_match) >> [=](std::set<actor>& workers) {
      aout(this) << "new set of " << workers.size() << " worker" << endl;
      m_workers.swap(workers);
      while (m_image_cache.size() < m_max_pending_images) {
        if (!assign_job()) {
          return;
        }
      }
    },
    on(atom("changefrac"), arg_match) >> [&](atom_value new_frac_option) {
      m_fractal_type = new_frac_option;
      // load correct stack for changed fractaltype
      if(m_fractal_type  == atom("mandel")) {
          m_stream.loop_stack_mandelbrot();
      } else if(m_fractal_type == atom("burnship")) {
          m_stream.loop_stack_burning_ship();
      } else if(m_fractal_type == atom("tricorn")) {
          // tricorn needs his own stream
          m_stream.loop_stack_mandelbrot();
      } else {
          aout(this) << "[server] couldn't get " <<
                        "coordination for changed fractal." << endl;
      }
    },
    on(atom("resize"), arg_match) >> [=](uint32_t width, uint32_t height) {
      m_stream.resize(width, height);
    },
    [=](const exit_msg& msg) {
      auto worker = actor_cast<actor>(msg.source);
      auto i = m_workers.find(worker);
      if (i != m_workers.end()) {
        aout(this) << "a worker disconnected" << endl;
        for (auto i = m_assigned_jobs.begin(); i != m_assigned_jobs.end(); ++i) {
          if (i->second == worker) {
            // drop everything up until lost job id
            m_draw_pos = i->first + 1;
            auto j = m_image_cache.begin();
            while (j != m_image_cache.end() && j->first < m_draw_pos) {
              m_image_cache.erase(j);
              j = m_image_cache.begin();
            }
            m_assigned_jobs.erase(i);
            return;
          }
        }
      } else {
        // an unknown link has been killed, fall back to default behavior
        quit(msg.reason);
      }
    },
    on(atom("SetSink"), arg_match) >> [=](const actor& sink) {
      m_image_sink = sink;
    },
    others() >> [=] {
      aout(this) << "[!!!] server received unexpected message: '"
                 << to_string(last_dequeued())
                 << "'." << endl;
    }
  };
}

void server::send_next_job(const actor& worker) {
  if (!m_stream.next()) {
    // stream endlessly
    if (m_fractal_type == atom("mandel")) {
        m_stream.loop_stack_mandelbrot();
    } else if (m_fractal_type == atom("burnship")) {
        m_stream.loop_stack_burning_ship();
    } else if (m_fractal_type == atom("tricorn")) {
        //tricorn needs his own stream
        m_stream.loop_stack_mandelbrot();
    } else {
      aout(this) << "[server] Reached end of stream end and didn't "
                 << "found a new one ... quit" << endl;
      quit(exit_reason::user_defined);
      return;
    }
  }
  auto next_id = m_next_id++;
  auto& fr = m_stream.request();
  send(worker,
       m_fractal_type,
       this,
       width(fr),
       height(fr),
       m_iterations,
       next_id,
       min_re(fr),
       max_re(fr),
       min_im(fr),
       max_im(fr));
  m_assigned_jobs[next_id] = worker;
}

void server::send_next_image() {
  auto i = m_image_cache.find(m_draw_pos);
  if (i != m_image_cache.end()) {
    send(m_image_sink, atom("Image"), i->second);
    m_image_cache.erase(i);
    ++m_draw_pos;
    ++m_drawn_images;
  }
}

bool server::assign_job() {
  if (m_workers.size() <= m_assigned_jobs.size()) {
    return false;
  }
  for (auto& w : m_workers) {
    if (m_assigned_jobs.count(w) == 0) {
      send_next_job(w);
    }
  }
  return true;
}
