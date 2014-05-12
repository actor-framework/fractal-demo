
#include <sstream>
#include <QString>
#include <utility>

#include "cppa/cppa.hpp"

#include "include/controllerwidget.hpp"

using namespace std;
using namespace cppa;


ControllerWidget::ControllerWidget(QWidget *parent, Qt::WindowFlags f) :
    super(parent, f),
   // m_controller(nullptr),
    m_cpu_slider(nullptr),
    m_gpu_slider(nullptr),
    m_resolution_slider(nullptr),
    m_res_current(nullptr),
    m_time_current(nullptr),
    m_resolutions{make_pair(800,450),
                  make_pair(1024,576),
                  make_pair(1280,720),
                  make_pair(1680,945),
                  make_pair(1920,1080),
                  make_pair(2560,1440)}

{
    set_message_handler ([=](local_actor* self) -> partial_function {

        return {
            on(atom("max_cpu"), arg_match) >> [=] (size_t max_cpu) {
            set_cpu_max(max_cpu);
        },
        on(atom("max_gpu"), arg_match) >> [=] (size_t max_gpu) {
            set_gpu_max(max_gpu);
        },
        on(atom("fps"), arg_match) >> [=] (uint32_t) {

        },
        on(atom("EXIT"), arg_match) >> [=](std::uint32_t) {
            cout << "[!!!] master died" << endl;
            // quit
        },
        on(atom("addfrac"), arg_match) >> [&] (const map<string, atom_value>& fractal_types) {
                m_valid_fractal = fractal_types;
                for(auto& fractal : fractal_types) {
                    drop_down_fractal_type()->addItem(QString(fractal.first.c_str()));
                }
        },
        others() >> [=] {
            cout << "[!!!] controller ui received unexpected message: '"
                 << to_string(self->last_dequeued())
                 << "'." << endl; }
        };
      });
    for (auto& p : m_resolutions) {
        m_res_strings.emplace_back(QString::number(p.first)
                                   + "x"
                                   + QString::number(p.second));
    }
}

void ControllerWidget::initialize() {
    resolution_slider()->setRange(0,m_resolutions.size()-1);
    resolution_slider()->setValue(m_resolutions.size()-1);
    resolution_slider()->setTickInterval(1);
    resolution_slider()->setTickPosition(QSlider::TicksBelow);
}


void ControllerWidget::adjustGPULimit(int newLimit) {
    // aout << "new GPU limit: " << newLimit << endl;
    if(m_controller) {
        send_as(as_actor(), m_controller, atom("limit"), atom("opencl"), static_cast<uint32_t>(newLimit));
    }
}

void ControllerWidget::adjustCPULimit(int newLimit) {
    // aout << "new CPU limit: " << newLimit << endl;
    if(m_controller) {
        send_as(as_actor(), m_controller, atom("limit"), atom("normal"), static_cast<uint32_t>(newLimit));
    }
}

void ControllerWidget::adjustResolution(int idx) {
    if(m_controller) {
        send_as(as_actor(), m_controller, atom("resize"), m_resolutions[idx].first, m_resolutions[idx].second);
        res_current()->setText(m_res_strings[idx]);
    }
}

void ControllerWidget::adjustFractals(const QString& fractal) {
    if(m_controller) {
        cout << "Changed fractal to: " << fractal.toStdString() << endl;
        send_as(as_actor(), m_controller, atom("changefrac"), m_valid_fractal[fractal.toStdString()]);
    }
}
