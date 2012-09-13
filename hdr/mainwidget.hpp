#ifndef MAINWIDGET_HPP
#define MAINWIDGET_HPP

#include <QWidget>

#include "cppa/cppa.hpp"

class MainWidget : public QWidget
{
    Q_OBJECT
 public slots:

    void jumpTo();

 public:

    explicit MainWidget(QWidget *parent = 0);

    void setServer(cppa::actor_ptr server);
    
 protected:

    void resizeEvent(QResizeEvent * event);

 private:

    bool m_has_server;
    cppa::actor_ptr m_server;

};

#endif // MAINWIDGET_HPP
