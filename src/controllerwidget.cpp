
#include "cppa/cppa.hpp"

#include "include/controllerwidget.hpp"

using namespace std;
using namespace cppa;


ControllerWidget::ControllerWidget(QWidget *parent, Qt::WindowFlags f) :
    super(parent, f),
    m_controller(nullptr),
    m_cpu_slider(nullptr),
    m_gpu_slider(nullptr),
    m_resolution_slider(nullptr)
{
    set_message_handler (
        on(atom("max_cpu"), arg_match) >> [=] (size_t max_cpu) {
            set_cpu_max(max_cpu);
        },
        on(atom("max_gpu"), arg_match) >> [=] (size_t max_gpu) {
            set_gpu_max(max_gpu);
        },
        on(atom("EXIT"), arg_match) >> [=](std::uint32_t) {
            cout << "[!!!] master died" << endl;
            // quit
        },
        others() >> [=] {
            cout << "[!!!] controller received unexpected message: '"
                 << to_string(self->last_dequeued())
                 << "'." << endl;
        }
    );
}

void ControllerWidget::initialize(std::vector<std::pair<std::uint32_t, 
                                                        std::uint32_t>> resolutions) {
//    m_resolutions.push_back(make_pair(800,450));
//    m_resolutions.push_back(make_pair(1024,576));
//    m_resolutions.push_back(make_pair(1280,720));
//    m_resolutions.push_back(make_pair(1680,945));
//    m_resolutions.push_back(make_pair(1920,1080));
//    m_resolutions.push_back(make_pair(2560,1440));
    m_resolutions.swap(resolutions);
    resolution_slider()->setRange(0,m_resolutions.size()-1);
    resolution_slider()->setValue(m_resolutions.size()-1);
    resolution_slider()->setTickInterval(1);
    resolution_slider()->setTickPosition(QSlider::TicksBelow);
}


void ControllerWidget::adjustGPULimit(int newLimit) {
    cout << "new GPU limit: " << newLimit << endl;
    if(m_controller != nullptr) {
        send(m_controller, atom("limit"), atom("opencl"), static_cast<uint32_t>(newLimit));
    }
}

void ControllerWidget::adjustCPULimit(int newLimit) {
    cout << "new CPU limit: " << newLimit << endl;
    if(m_controller != nullptr) {
        send(m_controller, atom("limit"), atom("normal"), static_cast<uint32_t>(newLimit));
    }
}

void ControllerWidget::adjustResolution(int idx) {
    if(m_controller != nullptr) {
        send(m_controller, atom("resize"), m_resolutions[idx].first, m_resolutions[idx].second);
    }
}
