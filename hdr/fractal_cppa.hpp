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
#include "cppa/exit_reason.hpp"

typedef std::complex<long double> complex_d;

typedef long double (complex_d::*complex_getter)() const;
typedef void   (complex_d::*complex_setter)(long double);

inline std::vector<std::string> split(const std::string& str, char delim) {
    std::vector<std::string> result;
    std::stringstream strs{str};
    std::string tmp;
    while (std::getline(strs, tmp, delim)) result.push_back(tmp);
    return result;
}

inline std::function<cppa::option<std::string> (const std::string&)>
       get_extractor(const std::string& identifier) {
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
