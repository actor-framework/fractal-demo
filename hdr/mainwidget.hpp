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

    bool m_has_server;
    cppa::actor_ptr m_server;
    ImageLabel *m_imagelabel;

};

#endif // MAINWIDGET_HPP
