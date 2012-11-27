#ifndef MAINWIDGET_HPP
#define MAINWIDGET_HPP

#include <QWidget>

#include "cppa/cppa.hpp"
#include "cppa/qtsupport/actor_widget_mixin.hpp"
#include "imagelabel.h"

class MainWidget : public cppa::actor_widget_mixin<QWidget>
{
    Q_OBJECT

 public slots:

    void jumpTo();

 public:

    explicit MainWidget(QWidget *parent = nullptr, Qt::WindowFlags f = 0);
    
 protected:

    void resizeEvent(QResizeEvent * event);

 private:

    typedef cppa::actor_widget_mixin<QWidget> super;

    bool m_has_server;
    cppa::actor_ptr m_server;
    ImageLabel *m_imagelabel;

};

#endif // MAINWIDGET_HPP
