#include <iostream>

#include <QImage>
#include <QBuffer>
#include <QPixmap>

#include "config.hpp"
#include "imagelabel.h"

using namespace std;

void ImageLabel::setPixmapFromByteArray(QByteArray ba) {
    QImage image;
    QBuffer buf(&ba);
    buf.open(QBuffer::ReadOnly);
    // image.load(&buf, "JPEG");
    image.load(&buf, image_format);
    buf.close();
    QPixmap pxm;
    if (!pxm.convertFromImage(image)) {
        // freak out
    }

    setPixmap(pxm);
}


