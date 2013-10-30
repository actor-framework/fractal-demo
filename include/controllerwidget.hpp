#ifndef CONTROLLERWIDGET_HPP
#define CONTROLLERWIDGET_HPP

#include <QWidget>
#include <QSlider>

#include "cppa/cppa.hpp"
#include "cppa/qtsupport/actor_widget_mixin.hpp"

class ControllerWidget : public cppa::actor_widget_mixin<QWidget> {

    Q_OBJECT

 public:

    explicit ControllerWidget(QWidget *parent = nullptr, Qt::WindowFlags f = 0);

    inline void set_controller(const cppa::actor_ptr& ptr) { m_controller = ptr; }

 public slots:

    void adjustGPULimit(int newLimit);
    void adjustCPULimit(int newLimit);
    void adjustResolution();

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

    inline void set_cpu_max(std::uint32_t max) {
        cpu_slider()->setRange(0, static_cast<int>(max));
    }

    inline void set_gpu_max(std::uint32_t max) {
        gpu_slider()->setRange(0, static_cast<int>(max));
    }


    typedef cppa::actor_widget_mixin<QWidget> super;

    cppa::actor_ptr m_controller;

    QSlider* m_cpu_slider;
    QSlider* m_gpu_slider;


};

#endif // CONTROLLERWIDGET_HPP
