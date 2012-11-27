#include "hdr/mainwidget.hpp"

#include <QResizeEvent>
#include <QInputDialog>

#include "cppa/cppa.hpp"

#include "ui_main.h"

using namespace cppa;

MainWidget::MainWidget(QWidget *parent, Qt::WindowFlags f) :
    super(parent, f),
    m_has_server(false),
    m_server(nullptr),
    m_imagelabel(nullptr)
{
    set_message_handler (
        on(atom("server"), arg_match) >> [=](const actor_ptr& server) {
            m_server = server;
            m_has_server = true;
        },
        on(atom("display"), arg_match) >> [=](const QByteArray& ba) {
            get(m_imagelabel, "imgLabel")->setPixmapFromByteArray(ba);
        }
    );
}

void MainWidget::resizeEvent(QResizeEvent *event) {
    if(m_has_server) {
        const QSize& size = event->size();
        cppa::send(m_server, cppa::atom("resize"), size.width(), size.height());
    }
}

void MainWidget::jumpTo() {
    std::cout << "[!!!] jump to not implemented!" << std::endl;
}
