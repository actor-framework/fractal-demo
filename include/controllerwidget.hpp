#ifndef CONTROLLERWIDGET_HPP
#define CONTROLLERWIDGET_HPP

#include <map>
#include <vector>
#include <QLabel>
#include <QWidget>
#include <QSlider>
#include <QComboBox>

#include "caf/all.hpp"
#include "caf/io/all.hpp"
#include "caf/mixin/actor_widget.hpp"

class ControllerWidget : public caf::mixin::actor_widget<QWidget> {
  Q_OBJECT
 public:
  explicit ControllerWidget(QWidget *parent = nullptr, Qt::WindowFlags f = 0);
  inline void set_controller(const caf::actor& controller_actor) {
    controller_ = controller_actor;
  }
  void initialize();

 public slots:
    void adjustGPULimit(int newLimit);
    void adjustCPULimit(int newLimit);
    void adjustResolution(int idx);
    void adjustFractals(const QString& valid_fractal);

 private:
  template <typename T>
  T* get(T*& member, const char* name) {
    if (member == nullptr) {
      member = findChild<T*>(name);
      if (member == nullptr)
        throw std::runtime_error("unable to find child: " + std::string(name));
    }
    return member;
  }

  inline QSlider* cpu_slider() {
    return get(cpu_slider_, "cpu_slider");
  }

  inline QSlider* gpu_slider() {
    return get(gpu_slider_, "gpu_slider");
  }

  inline QSlider* resolution_slider() {
    return get(resolution_slider_, "res_slider");
  }

  inline QComboBox* drop_down_fractal_type() {
    return get(drop_down_fractal_type_, "drop_down_fractal_type");
  }

  inline QLabel* res_current() {
    return get(res_current_, "res_current");
  }

  inline QLabel* time_current() {
    return get(time_current_, "time_current");
  }

  inline void set_cpu_max(size_t max) {
    cpu_slider()->setRange(0, static_cast<int>(max));
    cpu_slider()->setTickInterval(1);
    cpu_slider()->setTickPosition(QSlider::TicksBelow);
  }

  inline void set_gpu_max(size_t max) {
    gpu_slider()->setRange(0, static_cast<int>(max));
    gpu_slider()->setTickInterval(1);
    gpu_slider()->setTickPosition(QSlider::TicksBelow);
  }

  inline void set_fps(uint32_t time) {
    time_current()->setNum(static_cast<int>(time));
  }

  inline void set_fractal_types(const std::map<std::string, caf::atom_value>& fractal_types) {
    valid_fractal_ = fractal_types;
    while (drop_down_fractal_type() == nullptr)
      ; // wait for the UI
    for (auto& fractal : fractal_types) {
      drop_down_fractal_type()->addItem(QString(fractal.first.c_str()));
    }
  }

  typedef caf::mixin::actor_widget<QWidget> super;

  caf::actor controller_;

  QSlider* cpu_slider_;
  QSlider* gpu_slider_;
  QSlider* resolution_slider_;

  QLabel* res_current_;
  QLabel* time_current_;

  QComboBox* drop_down_fractal_type_;

  std::vector<QString> res_strings_;
  std::vector<std::pair<std::uint32_t, std::uint32_t>> resolutions_;
  std::map<std::string, caf::atom_value> valid_fractal_;
};

#endif // CONTROLLERWIDGET_HPP
