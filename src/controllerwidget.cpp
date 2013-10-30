
#include "cppa/cppa.hpp"

#include "include/controllerwidget.hpp"

using namespace std;
using namespace cppa;


ControllerWidget::ControllerWidget(QWidget *parent, Qt::WindowFlags f) :
    super(parent, f),
    m_controller(nullptr),
    m_cpu_slider(nullptr),
    m_gpu_slider(nullptr)
{
    set_message_handler (
        on(atom("setMax"), arg_match) >> [=] (int max_cpu, int max_gpu) {
            set_gpu_max(max_gpu);
            set_cpu_max(max_cpu);
        },
        others() >> [=] {
            cout << "[!!!] unexpected message: '"
                 << to_string(self->last_dequeued())
                 << "'." << endl;
        }
    );
}


void ControllerWidget::adjustGPULimit(int newLimit) {
    cout << "new GPU limit: " << newLimit << endl;
    if(m_controller != nullptr) {
        send(m_controller, atom("limWorkers"), true);
    }
}

void ControllerWidget::adjustCPULimit(int newLimit) {
    cout << "new CPU limit: " << newLimit << endl;
    if(m_controller != nullptr) {
        send(m_controller, atom("limWorkers"), false);
    }
}

void ControllerWidget::adjustResolution() {
    cout << "new resolution:" << endl;
}
