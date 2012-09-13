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
    if(!m_has_server) {
        return;
    }

    bool ok;

    long double x(QInputDialog::getDouble(this, tr("Jump To: x-coord"), tr("x-coord between -2.0 and 2.0"), 0.0, -2.0, 2.0, 20, &ok));
    if(!ok){
        return;
    }

    long double y(QInputDialog::getDouble(this, tr("Jump To: y-coord"), tr("y-coord between -2.0 and 2.0"), 0.0, -2.0, 2.0, 20, &ok));
    if(!ok){
        return;
    }

    cppa::send(m_server, cppa::atom("jumpto"), x, y);
}
