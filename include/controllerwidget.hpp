#ifndef CONTROLLERWIDGET_HPP
#define CONTROLLERWIDGET_HPP

#include <map>
#include <vector>
#include <QLabel>
#include <QWidget>
#include <QSlider>
#include <QComboBox>

#include "cppa/cppa.hpp"
#include "cppa/qtsupport/actor_widget_mixin.hpp"

class ControllerWidget : public cppa::actor_widget_mixin<QWidget> {

    Q_OBJECT

 public:

    explicit ControllerWidget(QWidget *parent = nullptr, Qt::WindowFlags f = 0);

    inline void set_controller(const cppa::actor& controller_actor) { m_controller = controller_actor; }
    void initialize();

 public slots:

    void adjustGPULimit(int newLimit);
    void adjustCPULimit(int newLimit);
    void adjustResolution(int idx);
    void adjustFractals(const QString& valid_fractal);


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

    inline QComboBox* drop_down_fractal_type(){
        return get(m_drop_down_fractal_type, "drop_down_fractal_type");
    }

    inline QLabel* res_current() {
        return get(m_res_current, "res_current");
    }

    inline QLabel* time_current() {
        return get(m_time_current, "time_current");
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

    inline void set_fractal_types(const std::map<std::string,cppa::atom_value>& fractal_types) {
        m_valid_fractal = fractal_types;
        while(drop_down_fractal_type() == nullptr); // wait for the UI
        for(auto& fractal : fractal_types) {
            drop_down_fractal_type()->addItem(QString(fractal.first.c_str()));
        }
    }


    typedef cppa::actor_widget_mixin<QWidget> super;

    cppa::actor m_controller;

    QSlider* m_cpu_slider;
    QSlider* m_gpu_slider;
    QSlider* m_resolution_slider;

    QLabel* m_res_current;
    QLabel* m_time_current;

    QComboBox* m_drop_down_fractal_type;

    std::vector<QString> m_res_strings;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> m_resolutions;
    std::map<std::string, cppa::atom_value> m_valid_fractal;
};

#endif // CONTROLLERWIDGET_HPP
