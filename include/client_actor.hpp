#ifndef CLIENT_ACTOR_HPP
#define CLIENT_ACTOR_HPP

#include <list>
#include <vector>

#include "caf/all.hpp"

#include "fractal_request.hpp"

using test_atom = caf::atom_constant<caf::atom("test")>;

class client_actor : public caf::event_based_actor {
public:
  client_actor(std::vector<caf::actor>& workers)
    : workers_(workers)
    , best_case_(0)
    , avarage_case_(0)
    , worst_case_(0) {
    // nop
  }

private:
  caf::behavior make_behavior() override {
    return {
      [&](test_atom, caf::actor& reply_to) {
        reply_to_ = reply_to;
        // TODO: Find positions of heavy calculateable image
        auto msg = caf::make_message(0,     // image_id
                                     100,   // iterations
                                     1920,  // width
                                     1080,  // height
                                     50,    // min_re
                                     1000,  // max_re
                                     50,    // min_im
                                     1000); // max_im
        best_case_ = 0;
        avarage_case_ = 0;
        worst_case_ = 0;
        for (auto& actor : workers_) {
          pending_tests_.push_back(actor.address());
          send(actor, msg);
        }
      },
      [&](fractal_result result) {
        auto took = std::get<2>(result);
        avarage_case_ += took;
        worst_case_ = std::max(took, worst_case_);
        pending_tests_.remove(current_sender());
        if (! pending_tests_.size()) {
          avarage_case_ /= workers_.size();
          if (reply_to_) // Send the results back
            send(reply_to_, best_case_, avarage_case_, worst_case_);
        }
      }
    };
  }
  //
  std::vector<caf::actor>&   workers_;
  //
  size_t                     best_case_;
  double                     avarage_case_;
  size_t                     worst_case_;
  std::list<caf::actor_addr> pending_tests_;
  caf::actor                 reply_to_;
};

#endif // CLIENT_ACTOR_HPP
