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
    const uint32_t DEFAULT_WIDTH = 1024;
    const uint32_t DEFAULT_HEIGHT = 768;
    const uint32_t DEFAULT_ITERATIONS = 500;
    const uint32_t MAX_RESULTS_STORED = 7;
    const uint32_t MIN_RESULTS_STORED = 3;
    const long double DEFAULT_MIN_REAL = -0.95; //-1.9;
    const long double DEFAULT_MAX_REAL =  0.5;  // 1.0;
    const long double DEFAULT_MIN_IMAG = -0.45; //-0.9;
    const long double DEFAULT_MAX_IMAG = DEFAULT_MIN_IMAG+(DEFAULT_MAX_REAL-DEFAULT_MIN_REAL)*DEFAULT_HEIGHT/DEFAULT_WIDTH;
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

 public:

    server(cppa::actor_ptr printer, ImageLabel* lbl, MainWidget *mw);

    virtual ~server();

};

#endif
