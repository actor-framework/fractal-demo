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
    float_type m_min_width;
    is_zoomed_in_to(const frstream* s, int zoom_step) {
        auto default_width = fabs(s->max_re() + (-1 * s->min_re()));
        m_min_width = pow(s->zoom(), zoom_step) * default_width;
    }
    bool operator()(const frstream*, const fractal_request& fr) const {
        return fabs(max_re(fr) + (-1 * min_re(fr))) <= m_min_width;
    }
};

struct is_zoomed_out_to {
    float_type m_max_width;
    is_zoomed_out_to(const frstream* s, int zoom_step) {
        auto default_width = (fabs(s->max_re() + (-1 * s->min_re())));
        m_max_width = pow(s->zoom(), zoom_step) * default_width;
    }
    bool operator()(const frstream*, const fractal_request& fr) const {
        auto width  = fabs(max_re(fr) + (-1 * min_re(fr)));
        return width >= m_max_width;
    }
};

struct is_equal_point {
    float_type my_re;
    float_type my_im;
    is_equal_point(float_type re, float_type im) : my_re(re), my_im(im) { }
    bool operator()(const frstream*, const fractal_request& fr) const {
        auto half_width  = fabs(max_re(fr) + (-1 * min_re(fr))) / 2;
        auto half_height = fabs(max_im(fr) + (-1 * min_im(fr))) / 2;
        auto re = min_re(fr) + half_width;
        auto im = min_im(fr) + half_height;
        return re == my_re && im == my_im;
    }
};

struct is_near {
    float_type m_re;
    float_type m_im;
    float_type m_radius;
    is_near(float_type re, float_type im, float_type r) : m_re(re), m_im(im), m_radius(r) { }
    bool operator()(const frstream*, const fractal_request& fr) const {
        auto half_width  = fabs(max_re(fr) + (-1 * min_re(fr))) / 2;
        auto half_height = fabs(max_im(fr) + (-1 * min_im(fr))) / 2;
        auto re = min_re(fr) + half_width;
        auto im = min_im(fr) + half_height;
        return fabs(m_re - re) < m_radius && fabs(m_im-im) < m_radius;
    }
};

// utility function
inline void set_fr(fractal_request& fr,
                   float_type re, float_type half_width,
                   float_type im, float_type half_height) {
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

void zoom_in_op(const frstream* s, fractal_request& fr) {
    auto half_width  = fabs(max_re(fr) + (-1 * min_re(fr))) / 2;
    auto half_height = fabs(max_im(fr) + (-1  *min_im(fr))) / 2;
    auto re = min_re(fr) + half_width;
    auto im = min_im(fr) + half_height;
    set_fr(fr, re, half_width * s->zoom(), im, half_height * s->zoom());
}

void zoom_out_op(const frstream* s, fractal_request& fr) {
    auto half_width  = fabs(max_re(fr) + (-1 * min_re(fr))) / 2;
    auto half_height = fabs(max_im(fr) + (-1 * min_im(fr))) / 2;
    auto re = min_re(fr) + half_width;
    auto im = min_im(fr) + half_height;
    auto zf = 1 / s->zoom();
    set_fr(fr, re, half_width * zf, im, half_height * zf);
}

struct move_line_op {
    float_type m_to_re;
    float_type m_to_im;
    float_type m_move_re;
    float_type m_move_im;
    float_type m_move_dist;
    move_line_op(float_type from_re, float_type from_im, float_type to_re, float_type to_im)
    : m_to_re(to_re), m_to_im(to_im) {
        m_move_re = (to_re - from_re)/5;
        m_move_im = (to_im - from_im)/5;
        m_move_dist = sqrt(pow(m_move_re, 2) + pow(m_move_im, 2));
    }
    void operator()(const frstream*, fractal_request& fr) const {
        auto half_width  = fabs(max_re(fr) + (-1 * min_re(fr))) / 2;
        auto half_height = fabs(max_im(fr) + (-1 * min_im(fr))) / 2;
        auto current_re = min_re(fr) + half_width;
        auto current_im = min_im(fr) + half_height;
        auto dist = sqrt(pow(m_to_re - current_re, 2) +
                         pow(m_to_im - current_im, 2));
        if (dist <= m_move_dist) {
            current_re = m_to_re;
            current_im = m_to_im;
        }
        else {
            auto change_x = (m_to_re - current_re);
            auto change_y = (m_to_im - current_im);
            auto change_length = sqrt(pow(change_x, 2) + pow(change_y, 2));
            change_x = (change_x / change_length) * m_move_dist;
            change_y = (change_y / change_length) * m_move_dist;
            current_re += change_x;
            current_im += change_y;
        }
        set_fr(fr, current_re, half_width, current_im, half_height);
    }
};

struct move_line_zoom_in_op {
    move_line_op m_move_line;
    move_line_zoom_in_op(float_type from_re, float_type from_im, float_type to_re, float_type to_im)
    : m_move_line(from_re, from_im, to_re, to_im) { }
    void operator()(const frstream* s, fractal_request& fr) const {
        m_move_line(s, fr);
        if (not is_zoomed_in(s, fr)) zoom_in_op(s, fr);
    }
};

struct move_line_zoom_out {
    move_line_op m_move_line;
    move_line_zoom_out(float_type from_re, float_type from_im,  float_type to_re, float_type to_im)
    : m_move_line(from_re, from_im, to_re, to_im) { }
    void operator()(const frstream* s, fractal_request& fr) const {
        m_move_line(s, fr);
        if (not is_zoomed_in(s, fr)) zoom_in_op(s, fr);
    }
};

} // namespace <anonymous>

void fractal_request_stream::add_start_move(float_type from_x, float_type from_y, float_type to_x, float_type to_y, int max_zoom) {
    m_operations.emplace_back(zoom_in_op, is_zoomed_in_to{this, max_zoom});
    m_operations.emplace_back(move_line_zoom_in_op{from_x, from_y, to_x, to_y},
                              is_equal_point{to_x, to_y});
}

void fractal_request_stream::add_move_from_to(float_type from_x, float_type from_y, float_type to_x, float_type to_y, int max_zoom) {
    float_type mid_x = from_x + ((to_x - from_x) / 2);
    float_type mid_y = from_y + ((to_y - from_y) / 2);
    m_operations.emplace_back(zoom_in_op, is_zoomed_in_to{this, max_zoom});
    m_operations.emplace_back(move_line_zoom_in_op{mid_x, mid_y, to_x, to_y},
                              is_equal_point{to_x, to_y});
    m_operations.emplace_back(move_line_zoom_out{from_x, from_y, mid_x, mid_y},
                              is_equal_point{mid_x, mid_y});
    m_operations.emplace_back(zoom_out_op, is_zoomed_out_to{this, 5});
}

void fractal_request_stream::add_end_move(float_type from_x,
                          float_type from_y,
                          float_type to_x,
                          float_type to_y) {
    m_operations.emplace_back(move_line_zoom_out{from_x, from_y, to_x, to_y},
                              is_equal_point{to_x, to_y});
    m_operations.emplace_back(zoom_out_op, is_zoomed_out_to{this, 5});
}

void fractal_request_stream::add_chain(std::vector<std::pair<float_type,float_type>>& chain, int zoom) {
    if (chain.size() > 0) {
        std::reverse(std::begin(chain), std::end(chain));
        float_type start_end_x = 0;
        float_type start_end_y = 0;
        auto first = chain.begin();
        auto second = chain.begin() + 1;
        auto end = chain.end();
//        cout << "[CHAIN] (" << first->first << "/" << first -> second
//             << ") => (" << start_end_x << "/" << start_end_y << ")" << endl;
        add_end_move(first->first, first->second, start_end_x, start_end_y);
        for (; second != end; ++first, ++second) {
//            cout << "[CHAIN] (" << second->first << "/" << second->second
//                 << ") => (" << first->first << "/" << first -> second << ")"
//                 << endl;
            add_move_from_to(second->first, second->second, first->first, first->second, zoom);
        }
//        cout << "[CHAIN] (" << start_end_x << "/" << start_end_y
//             << ") => (" << first->first << "/" << first -> second << ")"
//             << endl;
        add_start_move(start_end_x, start_end_y, first->first, first->second, zoom);
    }
}

void fractal_request_stream::loop_stack() {
    std::vector<std::pair<float_type,float_type>> chain;
    // coords are visited top down
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
    chain.emplace_back(0.28692299709,-0.01218247138); // geode
    chain.emplace_back(0.013438870532012129028364, 0.655614218769465062251320);
    chain.emplace_back(0.001643721971153,          0.822467633298876); // buzzsaw
    chain.emplace_back(-0.089,0.655);
    chain.emplace_back(-0.7458555,                 0.10550365);
    /* ######################### */
    add_chain(chain, 100);
    m_operations.emplace_back(reset_op, do_once);

    /* nowhereland (not enough iterations or coloring method sucks)*/
    // chain.emplace_back(0.73752777,-0.12849548); // nebula
    // chain.emplace_back(-0.7700, -0.1300);
    // chain.emplace_back(0.73752777,-0.12849548); // nebula

    /* can not zoom enough*/
    // chain.emplace_back(-1.369,0.005);
    // chain.emplace_back(0.3750001200618655, -0.2166393884377127);
    // chain.emplace_back(-1.74975914513036646, -0.00000000368513796); // vortex
    // chain.emplace_back(-1.74975914513271613, -0.00000000368338015); // microbug
    // chain.emplace_back(-1.74975914513272790, -0.00000000368338638); // nucleus
    // chain.emplace_back(-0.13856524454488, -0.64935990748190);
    
    /* not interesting */
    // chain.emplace_back(-1.6735,-0.0003318); // x-wing
}

void fractal_request_stream::resize(std::uint32_t new_width, std::uint32_t new_height) {
    float_type nw = new_width;
    float_type nh = new_height;
    width(m_freq) = nw;
    height(m_freq) = nh;
    ::max_im(m_freq) = ::min_im(m_freq)
                     + (::max_re(m_freq) - ::min_re(m_freq)) * nh / nw;
}

void fractal_request_stream::init(std::uint32_t width,
                                  std::uint32_t height,
                                  float_type min_re,
                                  float_type max_re,
                                  float_type min_im,
                                  float_type max_im,
                                  float_type zoom) {
    ::width(m_freq) = width;
    ::height(m_freq) = height;
    m_width  = width;
    m_height = height;
    m_min_re = min_re;
    m_max_re = max_re;
    m_min_im = min_im;
    m_max_im = max_im;
    m_zoom   = zoom;
    loop_stack();
}

bool fractal_request_stream::next() {
    if (not m_operations.empty()) {
        m_operations.back().first(this, m_freq);
        if (m_operations.back().second(this, m_freq)) m_operations.pop_back();
        return true;
    }
    return false;
}
