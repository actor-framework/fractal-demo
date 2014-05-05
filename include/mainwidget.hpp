#ifndef MAINWIDGET_HPP
#define MAINWIDGET_HPP

#include <map>
#include <set>
#include <QWidget>

#include "imagelabel.h"

#include "cppa/cppa.hpp"
#include "cppa/qtsupport/actor_widget_mixin.hpp"

class MainWidget : public cppa::actor_widget_mixin<QWidget> {
    Q_OBJECT

 public slots:

    void jumpTo();

 public:

    explicit MainWidget(QWidget *parent = nullptr, Qt::WindowFlags f = 0);

    inline void set_server(const cppa::actor& server_actor) { m_server = server_actor; }

 protected:

    void resizeEvent(QResizeEvent * event);

 private:

    typedef cppa::actor_widget_mixin<QWidget> super;

    template<typename T>
    T* get(T*& member, const char* name) {
        if (member == nullptr) {
            member = findChild<T*>(name);
            if (member == nullptr)
                throw std::runtime_error("unable to find child: "
                                         + std::string(name));
        }
        return member;
    }

    cppa::actor m_server;
    cppa::actor m_controller;

    ImageLabel *m_imagelabel;

};

#endif // MAINWIDGET_HPP
