#include <chrono>
#include <iostream>

#include "mainwidget.hpp"

#include <QResizeEvent>
#include <QInputDialog>

#include "caf/all.hpp"
#include "caf/io/all.hpp"

#include "ui_main.h"

using namespace std;
using namespace caf;

MainWidget::MainWidget(QWidget* parent, Qt::WindowFlags f)
    : super(parent, f),
      imagelabel_(nullptr) {
  set_message_handler ([=](local_actor* self) -> message_handler {
    return {
      [=](uint32_t width, const std::vector<uint16_t>& frac) {
        get(imagelabel_, "imgLabel")->setPixmap(width, frac);
      },
      others >> [=]{
        cerr << "[!!!] mainwidget received unexpected message: "
             << to_string(self->current_message())
             << endl;
      }
    };
  });
}

void MainWidget::resizeEvent(QResizeEvent*) {
    // if (server_) {
    //     const QSize& size = event->size();
    //     send_as(as_actor(),
    //             server_,
    //             atom("resize"),
    //             static_cast<uint32_t>(size.width()),
    //             static_cast<uint32_t>(size.height()));
    // }
    cout << "[!!!] 'resizeEvent' ignored!" << endl;
}

void MainWidget::jumpTo() {
  cout << "[!!!] 'jump to' not implemented!" << endl;
}
