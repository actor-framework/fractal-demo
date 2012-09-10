#ifndef SERVER_HPP
#define SERVER_HPP

#include <set>
#include <map>
#include <vector>

#include <QLabel>
#include <QObject>
#include <QPicture>
#include <QByteArray>

#include "cppa/cppa.hpp"

#include "imagelabel.h"
#include "mainwidget.hpp"
#include "fractal_cppa.hpp"

class server : public QObject, public cppa::event_based_actor {

    Q_OBJECT

 signals:
    void setPixmapWithByteArray(QByteArray);

 private:

    std::vector<cppa::actor_ptr> m_available_workers;
    cppa::actor_ptr m_printer;

    std::uint32_t m_assign_id;
    std::uint32_t m_next_id;

    complex_d m_power;
    complex_d m_constant;

    uint32_t m_width;
    uint32_t m_height;

    const double m_min_re;
    const double m_max_re;
    const double m_min_im;
    const double m_max_im;

    double m_re_factor;
    double m_im_factor;

    const uint32_t m_iterations;

    double m_real_mult;
    double m_imag_mult;

    double m_min_re_shifting;
    double m_max_re_shifting;
    double m_min_im_shifting;
    double m_max_im_shifting;

    std::vector<double> m_zoom_steps;
    uint32_t m_zoom_idx;
    const uint32_t m_max_zoom_steps;

    std::map<uint32_t, cppa::actor_ptr> m_assignments;
    std::map<uint32_t, QByteArray> m_results;
    std::set<uint32_t> m_missing_ids;

    void init();

 public:

    server(cppa::actor_ptr printer, uint32_t width, uint32_t height, double min_real, double max_real, double min_imag, uint32_t iterations, ImageLabel* lbl, MainWidget *mw);

    virtual ~server();

};

#endif
