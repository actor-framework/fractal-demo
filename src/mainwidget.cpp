#include "hdr/mainwidget.hpp"

#include <QResizeEvent>
#include <QInputDialog>

#include "cppa/cppa.hpp"

MainWidget::MainWidget(QWidget *parent) :
    QWidget(parent), m_has_server(false)
{
}


void MainWidget::setServer(cppa::actor_ptr server) {
    m_server = server;
    m_has_server = true;
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
