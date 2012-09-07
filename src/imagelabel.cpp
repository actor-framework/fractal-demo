#include "imagelabel.h"
#include <iostream>
#include <QImage>
#include <QBuffer>
#include <QPixmap>

using namespace std;

void ImageLabel::setPixmapFromByteArray(QByteArray ba) {
    QImage image;
    QBuffer buf(&ba);
    buf.open(QBuffer::ReadOnly);
    // image.load(&buf, "JPEG");
    image.load(&buf, "BMP");
    buf.close();
    QPixmap pxm;
    if (!pxm.convertFromImage(image)) {
        // freak out
    }
    this->setPixmap(pxm);
}
