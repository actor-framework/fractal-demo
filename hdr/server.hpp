#ifndef SERVER_HPP
#define SERVER_HPP

#include <QLabel>
#include <QObject>
#include <QPicture>

#include "cppa/cppa.hpp"

#include "imagelabel.h"
#include "fractal_cppa.hpp"

class server : public QObject, public cppa::event_based_actor {

    Q_OBJECT

 signals:
    void setPixmapWithByteArray(QByteArray);

 private:

    std::vector<cppa::actor_ptr> m_available_workers;
    cppa::actor_ptr m_printer;

    std::uint32_t m_id;

    bool m_done;

    complex_d m_power;
    complex_d m_constant;

    const int m_width;
    const int m_height;

    double m_min_re;
    double m_max_re;
    double m_min_im;
    double m_max_im;

    double m_re_factor;
    double m_im_factor;

    const int m_iterations;

    void init();

 public:

    server(cppa::actor_ptr printer, int width, int height, double min_real, double max_real, double min_imag, int iterations, ImageLabel* lbl);

    virtual ~server();

};

#endif
