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

    inline void set_server(const cppa::actor_ptr& ptr) { m_server = ptr; }

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

    cppa::actor_ptr m_server;
    ImageLabel *m_imagelabel;

    std::uint32_t m_next;
    std::set<std::uint32_t> m_dropped;
    std::map<std::uint32_t, QByteArray> m_buffer;

};

#endif // MAINWIDGET_HPP
