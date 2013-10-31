#ifndef CONTROLLERWIDGET_HPP
#define CONTROLLERWIDGET_HPP

#include <vector>
#include <QLabel>
#include <QWidget>
#include <QSlider>

#include "cppa/cppa.hpp"
#include "cppa/qtsupport/actor_widget_mixin.hpp"

class ControllerWidget : public cppa::actor_widget_mixin<QWidget> {

    Q_OBJECT

 public:

    explicit ControllerWidget(QWidget *parent = nullptr, Qt::WindowFlags f = 0);

    inline void set_controller(const cppa::actor_ptr& ptr) { m_controller = ptr; }
    void initialize(std::vector<std::pair<std::uint32_t, std::uint32_t>> resolutions);

 public slots:

    void adjustGPULimit(int newLimit);
    void adjustCPULimit(int newLimit);
    void adjustResolution(int idx);

 private:

    template<typename T>
    T* get(T*& member, const char* name) {
        if (member == nullptr) {
            member = findChild<T*>(name);
            if (member == nullptr)
                throw std::runtime_error("unable to find child: "
                                         + std::string(name));
        }
        return member;
    }

    inline QSlider* cpu_slider() {
        return get(m_cpu_slider, "cpu_slider");
    }

    inline QSlider* gpu_slider() {
        return get(m_gpu_slider, "gpu_slider");
    }

    inline QSlider* resolution_slider() {
        return get(m_resolution_slider, "res_slider");
    }

    inline QLabel* res_current() {
        return get(m_res_current, "res_current");
    }

    inline void set_cpu_max(std::uint32_t max) {
        cpu_slider()->setRange(0, static_cast<int>(max));
        cpu_slider()->setTickInterval(1);
        cpu_slider()->setTickPosition(QSlider::TicksBelow);
    }

    inline void set_gpu_max(std::uint32_t max) {
        gpu_slider()->setRange(0, static_cast<int>(max));
        gpu_slider()->setTickInterval(1);
        gpu_slider()->setTickPosition(QSlider::TicksBelow);
    }


    typedef cppa::actor_widget_mixin<QWidget> super;

    cppa::actor_ptr m_controller;

    QSlider* m_cpu_slider;
    QSlider* m_gpu_slider;
    QSlider* m_resolution_slider;

    QLabel* m_res_current;

    std::vector<std::pair<std::uint32_t, std::uint32_t>> m_resolutions;
};

#endif // CONTROLLERWIDGET_HPP
