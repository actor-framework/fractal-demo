#ifndef FRACTAL_CPPA_HPP
#define FRACTAL_CPPA_HPP

#include <map>
#include <set>
#include <chrono>
#include <time.h>
#include <vector>
#include <complex>
#include <cstdlib>
#include <sstream>
#include <iostream>

#include <QImage>
#include <QBuffer>
#include <QByteArray>

#include "cppa/cppa.hpp"
#include "cppa/match.hpp"

typedef std::complex<long double> complex_d;

typedef long double (complex_d::*complex_getter)() const;
typedef void   (complex_d::*complex_setter)(long double);

static const long double FRACTAL_PI = 3.1415926535897932384626433832795028841971693993751058209749445923078164062862089986280348253421170679;

inline cppa::actor_ptr spawn_printer() {
    using namespace std;
    using namespace cppa;
    return factory::event_based([](set<actor_ptr>* known_actors) {
        self->become (
            on(atom("quit")) >> [=] {
                self->quit();
            },
            on(atom("DOWN"), arg_match) >> [=](uint32_t reason) {
                auto who = self->last_sender();
                if (who) {
                    cout << "actor with id " << who->id() 
                         << " failed, reason: " << reason << endl;
                    known_actors->erase(who);
                }
            },
            on_arg_match >> [=](const std::string& str) {
                if (known_actors->count(self->last_sender()) == 0) {
                    self->monitor(self->last_sender());
                }
                cout << str << endl;
            }
        );
    }).spawn();
}

inline std::vector<std::string> split(const std::string& str, char delim) {
    std::vector<std::string> result;
    std::stringstream strs{str};
    std::string tmp;
    while (std::getline(strs, tmp, delim)) result.push_back(tmp);
    return result;
}

template<typename T>
auto conv(const std::string& str) -> cppa::option<T> {
    T result;
    if (std::istringstream(str) >> result) return result;
    return {};
}

inline std::function<cppa::option<std::string> (const std::string&)> get_extractor(const std::string& identifier) {
    auto tmp = [&](const std::string& kvp) -> cppa::option<std::string> {
        auto vec = split(kvp, '=');
        if (vec.size() == 2) {
            if (vec.front() == "--"+identifier) {
                return vec.back();
            }
        }
        return {};
    };
    return tmp;
}

#endif
