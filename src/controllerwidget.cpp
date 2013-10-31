
#include "cppa/cppa.hpp"

#include "include/controllerwidget.hpp"

using namespace std;
using namespace cppa;


ControllerWidget::ControllerWidget(QWidget *parent, Qt::WindowFlags f) :
    super(parent, f),
    m_master(nullptr),
    m_cpu_slider(nullptr),
    m_gpu_slider(nullptr),
    m_resolution_slider(nullptr)
{
    set_message_handler (
        on(atom("setMax"), arg_match) >> [=] (uint32_t max_cpu, uint32_t max_gpu) {
            set_cpu_max(max_cpu);
            set_gpu_max(max_gpu);
        },
        on(atom("EXIT"), arg_match) >> [=](std::uint32_t) {
            cout << "[!!!] master died" << endl;
            // quit
        },
        others() >> [=] {
            cout << "[!!!] unexpected message: '"
                 << to_string(self->last_dequeued())
                 << "'." << endl;
        }
    );
}

void ControllerWidget::initialize(std::vector<std::pair<std::uint32_t, std::uint32_t>> resolutions) {
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
    if(m_master != nullptr) {
        send(m_master, atom("limWorkers"), static_cast<uint32_t>(newLimit), true);
    }
}

void ControllerWidget::adjustCPULimit(int newLimit) {
    cout << "new CPU limit: " << newLimit << endl;
    if(m_master != nullptr) {
        send(m_master, atom("limWorkers"), static_cast<uint32_t>(newLimit), false);
    }
}

void ControllerWidget::adjustResolution(int idx) {
    cout << "new resolution: " << m_resolutions[idx].first << "x" << m_resolutions[idx].second << endl;
    if(m_master != nullptr) {
        send(m_master, atom("resize"), m_resolutions[idx].first, m_resolutions[idx].second);
    }
}
