
#include "cppa/cppa.hpp"

#include "include/controllerwidget.hpp"

using namespace std;
using namespace cppa;


ControllerWidget::ControllerWidget(QWidget *parent, Qt::WindowFlags f) :
    super(parent, f),
    m_controller(nullptr)
{
    set_message_handler (
        others() >> [=] {
            cout << "[!!!] unexpected message: '"
                 << to_string(self->last_dequeued())
                 << "'." << endl;
        }
    );
}
