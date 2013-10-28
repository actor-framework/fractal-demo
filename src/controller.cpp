
#include "cppa/cppa.hpp"

#include "include/controller.hpp"

using namespace std;
using namespace cppa;

controller::controller() {

}

void controller::init() {
    become (
        others() >> [=] {
            aout << "Unexpected message: '"
                 << to_string(last_dequeued()) << "'.\n";
        }
    );
}

