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

class server : public cppa::event_based_actor {

 private:

    typedef std::function<ld_tuple (long double,
                                    long double,
                                    long double,
                                    long double)> equation;
    typedef std::function<bool (const long double,
                                const long double,
                                const long double,
                                const long double)> condition;
    typedef std::pair<equation, condition> stack_element;

    cppa::actor_ptr m_gui;

    uint32_t m_interval; // in msecs
    uint32_t m_iterations;
    uint32_t m_queuesize;
    double m_zoom;

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

    std::map<uint32_t, cppa::actor_ptr> m_assignments;
    std::map<uint32_t, QByteArray> m_results;
    std::set<uint32_t> m_missing_ids;

    std::vector<stack_element> m_operations;

    void initialize_stack();

    void add_start_move(long double from_x,
                        long double from_y,
                        long double to_x,
                        long double to_y,
                        int max_zoom);
    void add_move_from_to(long double from_x,
                          long double from_y,
                          long double to_x,
                          long double to_y,
                          int max_zoom);
    void add_end_move(long double from_x,
                      long double from_y,
                      long double to_x,
                      long double to_y);
    void add_chain(std::vector<std::pair<long double, long double> >& chain,
                   int zoom);

 public:

    server(const cppa::actor_ptr& m_gui,
           uint32_t interval,
           uint32_t iterations,
           uint32_t queuesize,
           double zoom);

    virtual ~server();

    void init();

};

#endif
