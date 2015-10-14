#include <cmath>
#include <algorithm>

#include "fractal_request_stream.hpp"

using namespace std;

namespace {

using frstream = fractal_request_stream;

bool do_once(const frstream*, const fractal_request&) {
  return true;
}

bool is_zoomed_out(const frstream* s, const fractal_request& fr) {
  auto default_width = fabs(s->max_re() + (-1 * s->min_re()));
  auto current_width = fabs(max_re(fr) + (-1 * min_re(fr)));
  return current_width >= default_width;
}

bool is_zoomed_in(const frstream* s, const fractal_request& fr) {
  auto default_width = fabs(s->max_re() + (-1 * s->min_re()));
  auto current_width = fabs(max_re(fr) + (-1 * min_re(fr)));
  return current_width <= pow(s->zoom(), 50) * default_width;
}

struct is_zoomed_in_to {
  float min_width_;
  is_zoomed_in_to(const frstream* s, int zoostep_) {
    auto default_width = fabs(s->max_re() + (-1 * s->min_re()));
    min_width_ = pow(s->zoom(), zoostep_) * default_width;
  }
  bool operator()(const frstream*, const fractal_request& fr) const {
    return fabs(max_re(fr) + (-1 * min_re(fr))) <= min_width_;
  }
};

struct is_zoomed_out_to {
  float max_width_;
  is_zoomed_out_to(const frstream* s, int zoostep_) {
    auto default_width = (fabs(s->max_re() + (-1 * s->min_re())));
    max_width_ = pow(s->zoom(), zoostep_) * default_width;
  }
  bool operator()(const frstream*, const fractal_request& fr) const {
    auto width  = fabs(max_re(fr) + (-1 * min_re(fr)));
    return width >= max_width_;
  }
};

struct is_equal_point {
  float my_re;
  float my_im;
  is_equal_point(float re, float im) : my_re(re), my_im(im) { }
  bool operator()(const frstream*, const fractal_request& fr) const {
    auto half_width  = fabs(max_re(fr) + (-1 * min_re(fr))) / 2;
    auto half_height = fabs(max_im(fr) + (-1 * min_im(fr))) / 2;
    auto re = min_re(fr) + half_width;
    auto im = min_im(fr) + half_height;
    return re == my_re && im == my_im;
  }
};

struct is_near {
  float re_;
  float im_;
  float radius_;
  is_near(float re, float im, float r) : re_(re), im_(im), radius_(r) { }
  bool operator()(const frstream*, const fractal_request& fr) const {
    auto half_width  = fabs(max_re(fr) + (-1 * min_re(fr))) / 2;
    auto half_height = fabs(max_im(fr) + (-1 * min_im(fr))) / 2;
    auto re = min_re(fr) + half_width;
    auto im = min_im(fr) + half_height;
    return fabs(re_ - re) < radius_ && fabs(im_-im) < radius_;
  }
};

// utility function
inline void set_fr(fractal_request& fr,
                   float re, float half_width,
                   float im, float half_height) {
  min_re(fr) = re - half_width;
  max_re(fr) = re + half_width;
  min_im(fr) = im - half_height;
  max_im(fr) = im + half_height;
}

void no_op(const frstream*, fractal_request&) {
  // move along, nothing to see here (really)
}

void reset_op(const frstream* s, fractal_request& fr) {
  auto half_width  = fabs(s->max_re() + (-1 * s->min_re())) / 2;
  auto half_height = fabs(s->max_im() + (-1 * s->min_im())) / 2;
  set_fr(fr, 0, half_width, 0, half_height);
}

void zooin_op_(const frstream* s, fractal_request& fr) {
  auto half_width  = fabs(max_re(fr) + (-1 * min_re(fr))) / 2;
  auto half_height = fabs(max_im(fr) + (-1  *min_im(fr))) / 2;
  auto re = min_re(fr) + half_width;
  auto im = min_im(fr) + half_height;
  set_fr(fr, re, half_width * s->zoom(), im, half_height * s->zoom());
}

void zooout_op_(const frstream* s, fractal_request& fr) {
  auto half_width  = fabs(max_re(fr) + (-1 * min_re(fr))) / 2;
  auto half_height = fabs(max_im(fr) + (-1 * min_im(fr))) / 2;
  auto re = min_re(fr) + half_width;
  auto im = min_im(fr) + half_height;
  auto zf = 1 / s->zoom();
  set_fr(fr, re, half_width * zf, im, half_height * zf);
}

struct move_line_op {
  float to_re_;
  float to_im_;
  float move_re_;
  float move_im_;
  float move_dist_;
  move_line_op(float frore_, float froim_,
               float to_re, float to_im)
      : to_re_(to_re),
        to_im_(to_im) {
    move_re_ = (to_re - frore_) / 5;
    move_im_ = (to_im - froim_) / 5;
    move_dist_ = sqrt(pow(move_re_, 2) + pow(move_im_, 2));
  }
  void operator()(const frstream*, fractal_request& fr) const {
    auto half_width  = fabs(max_re(fr) + (-1 * min_re(fr))) / 2;
    auto half_height = fabs(max_im(fr) + (-1 * min_im(fr))) / 2;
    auto current_re = min_re(fr) + half_width;
    auto current_im = min_im(fr) + half_height;
    auto dist = sqrt(pow(to_re_ - current_re, 2) +
                     pow(to_im_ - current_im, 2));
    if (dist <= move_dist_) {
      current_re = to_re_;
      current_im = to_im_;
    }
    else {
      auto change_x = (to_re_ - current_re);
      auto change_y = (to_im_ - current_im);
      auto change_length = sqrt(pow(change_x, 2) + pow(change_y, 2));
      change_x = (change_x / change_length) * move_dist_;
      change_y = (change_y / change_length) * move_dist_;
      current_re += change_x;
      current_im += change_y;
    }
    set_fr(fr, current_re, half_width, current_im, half_height);
  }
};

struct move_line_zooin_op_ {
  move_line_op move_line_;
  move_line_zooin_op_(float frore_, float froim_,
                       float to_re, float to_im)
      : move_line_(frore_, froim_, to_re, to_im) {
    // nop
  }
  void operator()(const frstream* s, fractal_request& fr) const {
    move_line_(s, fr);
    if (not is_zoomed_in(s, fr)) zooin_op_(s, fr);
  }
};

struct move_line_zooout_ {
  move_line_op move_line_;
  move_line_zooout_(float frore_, float froim_,
                     float to_re, float to_im)
      : move_line_(frore_, froim_, to_re, to_im) {
    // nop
  }
  void operator()(const frstream* s, fractal_request& fr) const {
      move_line_(s, fr);
      if (not is_zoomed_in(s, fr)) zooin_op_(s, fr);
  }
};

} // namespace <anonymous>

void fractal_request_stream::add_start_move(float x, float y,
                                            float new_x, float new_y,
                                            int max_zoom) {
  operations_.emplace_back(zooin_op_, is_zoomed_in_to{this, max_zoom});
  operations_.emplace_back(move_line_zooin_op_{x, y, new_x, new_y},
                            is_equal_point{new_x, new_y});
}

void fractal_request_stream::add_move_froto_(float x, float y,
                                              float new_x,float new_y,
                                              int max_zoom) {
  float mid_x = x + ((new_x - x) / 2);
  float mid_y = y + ((new_y - y) / 2);
  operations_.emplace_back(zooin_op_, is_zoomed_in_to{this, max_zoom});
  operations_.emplace_back(move_line_zooin_op_{mid_x, mid_y, new_x, new_y},
                            is_equal_point{new_x, new_y});
  operations_.emplace_back(move_line_zooout_{x, y, mid_x, mid_y},
                            is_equal_point{mid_x, mid_y});
  operations_.emplace_back(zooout_op_, is_zoomed_out_to{this, 5});
}

void fractal_request_stream::add_end_move(float x, float y,
                                          float new_x, float new_y) {
    operations_.emplace_back(move_line_zooout_{x, y, new_x, new_y},
                              is_equal_point{new_x, new_y});
    operations_.emplace_back(zooout_op_, is_zoomed_out_to{this, 5});
}

void fractal_request_stream::add_chain(std::vector<std::pair<float,float>>& chain, int zoom) {
  if (chain.size() > 0) {
    std::reverse(std::begin(chain), std::end(chain));
    float start_end_x = 0;
    float start_end_y = 0;
    auto first = chain.begin();
    auto second = chain.begin() + 1;
    auto end = chain.end();
    add_end_move(first->first, first->second, start_end_x, start_end_y);
    for (; second != end; ++first, ++second) {
      add_move_froto_(second->first, second->second, first->first, first->second, zoom);
    }
    add_start_move(start_end_x, start_end_y, first->first, first->second, zoom);
  }
}

// Burning Ship
void fractal_request_stream::loop_stack_burning_ship() {
  std::vector<std::pair<float,float>> chain;
  // coords are visited top down
  /* ######################### */
  chain.emplace_back(1.941, 0.004);       // tiny ship
  chain.emplace_back(1.861, 0.005);       // other tiny
  chain.emplace_back(1.755, 0.0270009);   // inner ship
  chain.emplace_back(1.625, 0.034);       // other ship
  /* ######################### */
  chain.emplace_back(1.941, 0.004);       // tiny ship
  chain.emplace_back(1.861, 0.005);       // other tiny
  chain.emplace_back(1.755, 0.0270009);   // inner ship
  chain.emplace_back(1.625, 0.034);       // other ship
  /* ######################### */
  chain.emplace_back(1.941, 0.004);       // tiny ship
  chain.emplace_back(1.861, 0.005);       // other tiny
  chain.emplace_back(1.755, 0.0270009);   // inner ship
  chain.emplace_back(1.625, 0.034);       // other ship
  /* ######################### */
  add_chain(chain, 80);
  //add_chain(chain, 100);
  operations_.emplace_back(reset_op, do_once);
}

// Mandelbrot
void fractal_request_stream::loop_stack_mandelbrot() {
  std::vector<std::pair<float,float>> chain;
  // coords are visited top down
  chain.emplace_back(0.28692299709,-0.01218247138);  // geode
  chain.emplace_back(0.001643721971153,          0.822467633298876); // buzzsaw
  chain.emplace_back(-0.089,0.655);
  chain.emplace_back(-0.7458555,                 0.10550365);
  /* ######################### */
  chain.emplace_back(0.28692299709,-0.01218247138); // geode
  chain.emplace_back(0.013438870532012129028364, 0.655614218769465062251320);
  chain.emplace_back(0.001643721971153,          0.822467633298876); // buzzsaw
  chain.emplace_back(-0.089,0.655);
  chain.emplace_back(-0.7458555,                 0.10550365);
  /* ######################### */
  chain.emplace_back(0.28692299709,-0.01218247138); // geode
  chain.emplace_back(0.013438870532012129028364, 0.655614218769465062251320);
  chain.emplace_back(0.001643721971153,          0.822467633298876); // buzzsaw
  chain.emplace_back(-0.089,0.655);
  chain.emplace_back(-0.7458555,                 0.10550365);
  /* ######################### */
  chain.emplace_back(0.28692299709,-0.01218247138); // geode
  chain.emplace_back(0.013438870532012129028364, 0.655614218769465062251320);
  chain.emplace_back(0.001643721971153,          0.822467633298876); // buzzsaw
  chain.emplace_back(-0.089,0.655);
  chain.emplace_back(-0.7458555,                 0.10550365);
  /* ######################### */
  add_chain(chain, 80);
  //add_chain(chain, 100);
  operations_.emplace_back(reset_op, do_once);
}

void fractal_request_stream::resize(std::uint32_t new_width,
                                    std::uint32_t new_height) {
  float nw = new_width;
  float nh = new_height;
  width(freq_) = nw;
  height(freq_) = nh;
  ::max_im(freq_) = ::min_im(freq_)
                   + (::max_re(freq_) - ::min_re(freq_)) * nh / nw;
}

void fractal_request_stream::init(std::uint32_t width,
                                  std::uint32_t height,
                                  float min_re,
                                  float max_re,
                                  float min_im,
                                  float max_im,
                                  float zoom) {
  ::width(freq_) = width;
  ::height(freq_) = height;
  width_  = width;
  height_ = height;
  min_re_ = min_re;
  max_re_ = max_re;
  min_im_ = min_im;
  max_im_ = max_im;
  zoom_   = zoom;
  loop_stack_mandelbrot();
}

const fractal_request& fractal_request_stream::next() {
  operations_.back().first(this, freq_);
  if (operations_.back().second(this, freq_))
    operations_.pop_back();
  return freq_;
}

bool fractal_request_stream::at_end() const {
  return operations_.empty();
}
