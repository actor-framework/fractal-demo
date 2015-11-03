#ifndef MAINWIDGET_HPP
#define MAINWIDGET_HPP

#include <map>
#include <set>
#include <QWidget>

#include "imagelabel.hpp"

#include "caf/all.hpp"
#include "caf/io/all.hpp"

#include "caf/mixin/actor_widget.hpp"

class MainWidget : public caf::mixin::actor_widget<QWidget> {

  Q_OBJECT

 public slots:

  void jumpTo();

 public:
  explicit MainWidget(QWidget* parent = nullptr, Qt::WindowFlags f = 0);

  inline void set_server(const caf::actor& server_actor) {
    server_ = server_actor;
  }

  ImageLabel* image_label() const {
    return imagelabel_;
  }

 protected:
  void resizeEvent(QResizeEvent* event);

 private:
  typedef caf::mixin::actor_widget<QWidget> super;

  template <typename T>
  T* get(T*& member, const char* name) {
    if (member == nullptr) {
      member = findChild<T*>(name);
      if (member == nullptr) {
        throw std::runtime_error("unable to find child: " + std::string(name));
      }
    }
    return member;
  }

  caf::actor server_;
  caf::actor controller_;
  ImageLabel* imagelabel_;
};

#endif // MAINWIDGET_HPP

