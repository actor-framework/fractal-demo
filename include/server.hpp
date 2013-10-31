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
#include "config_map.hpp"
#include "fractal_request.hpp"
#include "fractal_request_stream.hpp"

class server : public cppa::event_based_actor {

 public:

    server(config_map& config, cppa::actor_ptr counter);

    void init();

 private:

    std::uint32_t m_interval;   // in msecs
    std::uint32_t m_queuesize;
    std::uint32_t m_next_id;
    //std::uint32_t m_assign_id;
    std::uint32_t m_iterations;

    cppa::actor_ptr m_counter;

    std::set<cppa::actor_ptr> m_workers;
    std::map<cppa::actor_ptr, std::uint32_t> m_jobs;

    fractal_request_stream m_stream;

    void send_next_job(const cppa::actor_ptr& worker);

};

#endif
