#include <chrono>
#include <iostream>

#include "mainwidget.hpp"

#include <QResizeEvent>
#include <QInputDialog>

#include "cppa/cppa.hpp"

#include "ui_main.h"

using namespace std;
using namespace cppa;

MainWidget::MainWidget(QWidget *parent, Qt::WindowFlags f) :
    super(parent, f),
    m_server(nullptr),
    m_imagelabel(nullptr),
    m_next(0)
{
    set_message_handler (
        on(atom("display")) >> [=] {
            // todo find a better way to display pictures
            // (initial message sent from main)
            //cout << "next picture: " << m_next << endl;
            auto itr = m_buffer.find(m_next);
            if(itr != m_buffer.end()) {
                get(m_imagelabel, "imgLabel")->setPixmapFromByteArray(itr->second);
                m_buffer.erase(itr);
                ++m_next;
            }
            auto skip_dropped = [=] () -> bool {
                auto n = m_dropped.find(m_next);
                if (n != m_dropped.end()) {
                    m_dropped.erase(n);
                    ++m_next;
                    return true;
                }
                else {
                    return false;
                }
            };
            while(skip_dropped());
            delayed_send(self, chrono::microseconds(300), atom("display"));
        },
//        on(atom("result"), arg_match) >> [=](uint32_t, const QByteArray& ba, bool) {
//            get(m_imagelabel, "imgLabel")->setPixmapFromByteArray(ba);
//        },
        on(atom("result"), arg_match) >> [=](uint32_t id, const QByteArray& image) {
            //get(m_imagelabel, "imgLabel")->setPixmapFromByteArray(ba);
            // todo avoid a copy
            m_buffer.insert(make_pair(id, image));
        },
        on(atom("dropped"), arg_match) >> [=](uint32_t image_id) {
            if(image_id == m_next) {
                ++m_next;
            }
            else if (image_id > m_next){
                m_dropped.insert(image_id);
            }
        },
        on(atom("done")) >> [] { },
        others() >> [=] {
            cout << "[!!!] unexpected message: '"
                 << to_string(self->last_dequeued())
                 << "'." << endl;
        }
    );
}

void MainWidget::resizeEvent(QResizeEvent *event) {
    if (m_server) {
        const QSize& size = event->size();
        send_as(as_actor(),
                m_server,
                atom("resize"),
                static_cast<uint32_t>(size.width()),
                static_cast<uint32_t>(size.height()));
    }
}

void MainWidget::jumpTo() {
    cout << "[!!!] 'jump to' not implemented!" << endl;
}
