
#include <sstream>
#include <QString>
#include <utility>
#include <iterator>
#include <algorithm>

#include <iostream>

#include "caf/all.hpp"

#include "include/config.hpp"
#include "include/controllerwidget.hpp"

using namespace std;
using namespace caf;

ControllerWidget::ControllerWidget(QWidget* parent, Qt::WindowFlags f)
    : super(parent, f),
      cpu_slider_(nullptr),
      gpu_slider_(nullptr),
      resolution_slider_(nullptr),
      color_slider_(nullptr),
      res_current_(nullptr),
      time_current_(nullptr),
      drop_down_fractal_type_(nullptr),
      resolutions_{make_pair(800, 450),   make_pair(1024, 576),
                    make_pair(1280, 720),  make_pair(1680, 945),
                    make_pair(1920, 1080), make_pair(2560, 1440)} {
  set_message_handler ([=](local_actor* self) -> message_handler {
    return {
      on(atom("max_cpu"), arg_match) >> [=](size_t max_cpu) {
        aout(self) << "max_cpu = " << max_cpu << endl;
        set_cpu_max(max_cpu);
      },
      on(atom("max_gpu"), arg_match) >> [=](size_t max_gpu) {
        aout(self) << "max_gpu = " << max_gpu << endl;
        set_gpu_max(max_gpu);
      },
      on(atom("fps"), arg_match) >> [=](uint32_t fps) {
        aout(self) << "fps = " << fps << endl;
      },
      [](const exit_msg&) {
        cout << "[!!!] master died" << endl;
      },
      on(atom("fraclist"), arg_match) >> [=](const map<string, atom_value>& fractal_types) {
        set_fractal_types(fractal_types);
      },
      others >> [=]{
          cout << "[!!!] controller ui received unexpected message: "
               << to_string(self->current_message())
               << endl;
      }
    };
  });
  for (auto& p : resolutions_) {
    res_strings_.emplace_back(QString::number(p.first) + "x"
                               + QString::number(p.second));
  }
}

void ControllerWidget::initialize() {
  resolution_slider()->setRange(0,resolutions_.size()-1);
  resolution_slider()->setValue(resolutions_.size()-1);
  resolution_slider()->setTickInterval(1);
  resolution_slider()->setTickPosition(QSlider::TicksBelow);
  cpu_slider()->setTickInterval(1);
  cpu_slider()->setTickPosition(QSlider::TicksBelow);
  gpu_slider()->setTickInterval(1);
  gpu_slider()->setTickPosition(QSlider::TicksBelow);
  // TODO: Not implemented yet, so make this invisible
  drop_down_fractal_type()->setVisible(false);
  color_slider()->setRange(0, 359);
  color_slider()->setTickInterval(1);
  color_slider()->setTickPosition(QSlider::TicksBelow);
  color_slider()->setValue(default_color); // 180 is default color
}


void ControllerWidget::adjustGPULimit(int newLimit) {
  if(controller_) {
    send_as(as_actor(), controller_, atom("limit"), atom("opencl"), static_cast<uint32_t>(newLimit));
  }
}

void ControllerWidget::adjustCPULimit(int newLimit) {
  if(controller_) {
    send_as(as_actor(), controller_, atom("limit"), atom("normal"), static_cast<uint32_t>(newLimit));
  }
}

void ControllerWidget::adjustResolution(int idx) {
  if(controller_) {
    send_as(as_actor(), controller_, atom("resize"), resolutions_[idx].first,
            resolutions_[idx].second);
    res_current()->setText(res_strings_[idx]);
  }
}

void ControllerWidget::adjustFractals(const QString& fractal) {
  if(controller_) {
    send_as(as_actor(), controller_, atom("changefrac"),
            valid_fractal_[fractal.toStdString()]);
  }
}

void ControllerWidget::adjustColor(int newColor) {
  std::cout << "Adjust color: " << newColor << std::endl;
}
