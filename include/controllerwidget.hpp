#ifndef CONTROLLERWIDGET_HPP
#define CONTROLLERWIDGET_HPP

#include <QWidget>

#include "cppa/cppa.hpp"
#include "cppa/qtsupport/actor_widget_mixin.hpp"

class ControllerWidget : public cppa::actor_widget_mixin<QWidget> {
    Q_OBJECT

 public:

    explicit ControllerWidget(QWidget *parent = nullptr, Qt::WindowFlags f = 0);

    inline void set_controller(const cppa::actor_ptr& ptr) { m_controller = ptr; }

 private:

    typedef cppa::actor_widget_mixin<QWidget> super;

    cppa::actor_ptr m_controller;

};

#endif // CONTROLLERWIDGET_HPP
