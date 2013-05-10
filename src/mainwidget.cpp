#include <iostream>

#include "mainwidget.hpp"

#include <QResizeEvent>
#include <QInputDialog>

#include "cppa/cppa.hpp"

#include "ui_main.h"

using namespace std;
using namespace cppa;

MainWidget::MainWidget(QWidget *parent, Qt::WindowFlags f) :
    super(parent, f),
    m_server(nullptr),
    m_imagelabel(nullptr)
{
    set_message_handler (
        on(atom("result"), arg_match) >> [=](uint32_t, const QByteArray& ba) {
            get(m_imagelabel, "imgLabel")->setPixmapFromByteArray(ba);
        },
        on(atom("done")) >> [] { },
        others() >> [=] {
            cout << "[!!!] unexpected message: '"
                 << to_string(self->last_dequeued())
                 << "'." << endl;
        }
    );
}

void MainWidget::resizeEvent(QResizeEvent *event) {
    if (m_server) {
        const QSize& size = event->size();
        send_as(as_actor(),
                m_server,
                atom("resize"),
                static_cast<uint32_t>(size.width()),
                static_cast<uint32_t>(size.height()));
    }
}

void MainWidget::jumpTo() {
    cout << "[!!!] jump to not implemented!" << endl;
}
