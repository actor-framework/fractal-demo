#ifndef SERVER_HPP
#define SERVER_HPP

#include <set>
#include <map>
#include <vector>
#include <functional>

#include <QLabel>
#include <QObject>
#include <QPicture>
#include <QByteArray>

#include "cppa/cppa.hpp"

#include "imagelabel.h"
#include "mainwidget.hpp"
#include "fractal_cppa.hpp"

typedef std::tuple<long double, long double, long double, long double> ld_tuple;
typedef std::function<ld_tuple (long double, long double, long double, long double)> equation;
typedef std::function<bool (const long double, const long double, const long double, const long double)> condition;
typedef std::pair<equation, condition> stack_element;

namespace {
    const uint32_t TIME_BETWEEN_PICUTRES = 2;
    const uint32_t DEFAULT_WIDTH = 1024;
    const uint32_t DEFAULT_HEIGHT = 768;
    const uint32_t DEFAULT_ITERATIONS = 500;
    const uint32_t MAX_RESULTS_STORED = 12;
    const uint32_t MIN_RESULTS_STORED = 3;
    const long double DEFAULT_MIN_REAL = -1.9; // must be <= 0.0
    const long double DEFAULT_MAX_REAL =  1.0; // must be >= 0.0
    const long double DEFAULT_MIN_IMAG = -0.9; // must be <= 0.0
    const long double DEFAULT_MAX_IMAG = DEFAULT_MIN_IMAG+(DEFAULT_MAX_REAL-DEFAULT_MIN_REAL)*DEFAULT_HEIGHT/DEFAULT_WIDTH;
    const long double DEFAULT_ZOOM_FACTOR = 0.5; // must be <= 0.0
}

class server : public QObject, public cppa::event_based_actor {

    Q_OBJECT

 signals:
    void setPixmapWithByteArray(QByteArray);

 private:

    std::vector<cppa::actor_ptr> m_available_workers;
    const cppa::actor_ptr m_printer;

    std::uint32_t m_next_id;
    std::uint32_t m_assign_id;

    const complex_d m_power;

    uint32_t m_width;
    uint32_t m_height;

    long double m_min_re;
    long double m_max_re;
    long double m_min_im;
    long double m_max_im;

    uint32_t m_iterations;

    std::map<uint32_t, cppa::actor_ptr> m_assignments;
    std::map<uint32_t, QByteArray> m_results;
    std::set<uint32_t> m_missing_ids;

    std::vector<stack_element> m_operations;

    void init();

    void initialize_stack();

    void add_start_move(long double from_x, long double from_y, long double to_x, long double to_y, int max_zoom);
    void add_move_from_to(long double from_x, long double from_y, long double to_x, long double to_y, int max_zoom);
    void add_end_move(long double from_x, long double from_y, long double to_x, long double to_y);
    void add_chain(std::vector<std::pair<long double, long double> >& chain, int zoom);

 public:

    server(/*cppa::actor_ptr printer, */ ImageLabel* lbl, MainWidget *mw);

    virtual ~server();

};

#endif
