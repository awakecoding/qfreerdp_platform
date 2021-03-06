/*
 * Copyright © 2013 Hardening <rdp.effort@gmail.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the copyright holders not be used in
 * advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  The copyright holders make
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "qfreerdplistener.h"
#include "qfreerdpplatform.h"
//#include "qfreerdpintegration_impl.h"
#include "qfreerdppeer.h"
#include <QtDebug>


QFreeRdpListener::QFreeRdpListener(QFreeRdpPlatform *platform) :
	listener(0),
	listenerNotifier(0),
	mLastPeer(0),
	mPlatform(platform)
{
}


void QFreeRdpListener::rdp_incoming_peer(freerdp_listener* instance, freerdp_peer* client)
{
	qDebug() << "got an incoming connection";

	QFreeRdpListener *listener = (QFreeRdpListener *)instance->param4;
	QFreeRdpPeer *peer = new QFreeRdpPeer(listener->mPlatform, client);
	if(!peer->init()) {
		delete peer;
		return;
	}
	listener->mPlatform->registerPeer(peer);

	listener->mLock.lock();
	listener->mLastPeer = peer;
	listener->mPeerCond.wakeAll();
	listener->mLock.unlock();
}

void QFreeRdpListener::incomingNewPeer() {
	if(!listener->CheckFileDescriptor(listener))
		qDebug() << "unable to CheckFileDescriptor\n";
}


void QFreeRdpListener::initialize() {
	int fd;
	int rcount = 0;
	void* rfds[32];
	listener = freerdp_listener_new();
	listener->PeerAccepted = QFreeRdpListener::rdp_incoming_peer;
	listener->param4 = this;
	if(!listener->Open(listener, /*mPlatform->config->bind_address*/NULL, /*mPlatform->config->port*/3389)) {
		qDebug() << "unable to bind rdp socket\n";
		return;
	}

	if (!listener->GetFileDescriptor(listener, rfds, &rcount) || rcount < 1) {
		qDebug("Failed to get FreeRDP file descriptor\n");
		return;
	}

	fd = (int)(long)(rfds[0]);
	listenerNotifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
	mPlatform->getDispatcher()->registerSocketNotifier(listenerNotifier);

	connect(listenerNotifier, SIGNAL(activated(int)), this, SLOT(incomingNewPeer()));
}


QFreeRdpPeer *QFreeRdpListener::waitConnection(unsigned long delay) {
	QMutexLocker ll(&mLock);

	if(!mLastPeer && !mPeerCond.wait(&mLock, delay))
		return 0;
	QFreeRdpPeer *ret = mLastPeer;
	mLastPeer = 0;
	return ret;
}
