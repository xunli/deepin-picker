/* -*- Mode: C++; indent-tabs-mode: nil; tab-width: 4 -*-
 * -*- coding: utf-8 -*-
 *
 * Copyright (C) 2011 ~ 2018 Deepin, Inc.
 *               2011 ~ 2018 Wang Yong
 *
 * Author:     Wang Yong <wangyong@deepin.com>
 * Maintainer: Wang Yong <wangyong@deepin.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "eventmonitor.h"
#include <X11/Xlibint.h>

EventMonitor::EventMonitor(QObject *parent) : QThread(parent)
{
    isPress = false;
}

void EventMonitor::run()
{
    // Init x11 display.
    Display* display = XOpenDisplay(0);
    if (display == 0) {
        fprintf(stderr, "unable to open display\n");
        return;
    }

    // Receive from ALL clients, including future clients.
    XRecordClientSpec clients = XRecordAllClients;
    XRecordRange* range = XRecordAllocRange();
    if (range == 0) {
        fprintf(stderr, "unable to allocate XRecordRange\n");
        return;
    }

    // Receive KeyPress, KeyRelease, ButtonPress, ButtonRelease and MotionNotify events.
    memset(range, 0, sizeof(XRecordRange));
    range->device_events.first = KeyPress;
    range->device_events.last  = MotionNotify;

    // And create the XRECORD context.
    XRecordContext context = XRecordCreateContext(display, 0, &clients, 1, &range, 1);
    if (context == 0) {
        fprintf(stderr, "XRecordCreateContext failed\n");
        return;
    }
    XFree(range);

    // Sync x11 display message.
    XSync(display, True);

    Display* display_datalink = XOpenDisplay(0);
    if (display_datalink == 0) {
        fprintf(stderr, "unable to open second display\n");
        return;
    }

    // Enter in xrecord event loop.
    if (!XRecordEnableContext(display_datalink, context,  callback, (XPointer) this)) {
        fprintf(stderr, "XRecordEnableContext() failed\n");
        return;
    }
}

void EventMonitor::callback(XPointer ptr, XRecordInterceptData* data)
{
    ((EventMonitor *) ptr)->handleRecordEvent(data);
}

void EventMonitor::handleRecordEvent(XRecordInterceptData* data)
{
    if (data->category == XRecordFromServer) {
        xEvent * event = (xEvent *)data->data;
        
        switch (event->u.u.type) {
        case ButtonPress:
            if (filterWheelEvent(event->u.u.detail)) {
                isPress = true;
                
                if (event->u.u.detail == Button1) {
                    emit leftButtonPress(
                        event->u.keyButtonPointer.rootX,
                        event->u.keyButtonPointer.rootY);
                } else if (event->u.u.detail == Button3) {
                    emit rightButtonPress(
                        event->u.keyButtonPointer.rootX,
                        event->u.keyButtonPointer.rootY);
                }
            }

            break;
        case MotionNotify:
            if (isPress) {
                emit mouseDrag(
                    event->u.keyButtonPointer.rootX,
                    event->u.keyButtonPointer.rootY);
            } else {
                emit mouseMove(
                    event->u.keyButtonPointer.rootX,
                    event->u.keyButtonPointer.rootY);
            }

            break;
        case ButtonRelease:
            if (filterWheelEvent(event->u.u.detail)) {
                isPress = false;

                if (event->u.u.detail == Button1) {
                    emit leftButtonRelease(
                        event->u.keyButtonPointer.rootX,
                        event->u.keyButtonPointer.rootY);
                } else if (event->u.u.detail == Button3) {
                    emit rightButtonRelease(
                        event->u.keyButtonPointer.rootX,
                        event->u.keyButtonPointer.rootY);
                }
            }
            
            break;
        case KeyPress:
            emit keyPress(((unsigned char*) data->data)[1]);
            
            // If key is equal to esc, emit pressEsc signal.
            if (((unsigned char*) data->data)[1] == 9) {
                emit pressEsc();
            }

            break;
        case KeyRelease:
            emit keyRelease(((unsigned char*) data->data)[1]);

            break;
        default:
            break;
        }
    }

    fflush(stdout);
    XRecordFreeData(data);
}

bool EventMonitor::filterWheelEvent(int detail)
{
    return detail != WheelUp && detail != WheelDown && detail != WheelLeft && detail != WheelRight;
}

