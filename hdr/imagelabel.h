#ifndef IMAGE_LABEL_HPP
#define IMAGE_LABEL_HPP

#include <QImage>
#include <QLabel>
#include <QWidget>
#include <QResizeEvent>

class ImageLabel : public QLabel {

    Q_OBJECT

 public slots:

    void setPixmapFromByteArray(QByteArray);

 public:

    inline ImageLabel(QWidget * parent = 0, Qt::WindowFlags f = 0)
    : QLabel(parent, f) { }

};

#endif
