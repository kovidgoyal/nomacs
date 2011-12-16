/*******************************************************************************************************
 DkMenu.h
 Created on:	09.08.2011
 
 nomacs is a fast and small image viewer with the capability of synchronizing multiple instances
 
 Copyright (C) 2011 Markus Diem <markus@nomacs.org>
 Copyright (C) 2011 Stefan Fiel <stefan@nomacs.org>
 Copyright (C) 2011 Florian Kleber <florian@nomacs.org>

 This file is part of nomacs.

 nomacs is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 nomacs is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.

 *******************************************************************************************************/

#pragma once


#include <QtGui/QAction>
#include <QtGui/QMenuBar>

#include <QtCore/QFileInfo>
#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QTimer>

#include "DkNetwork.h"

/**
 * This class perfectly acts like a QMenuBar.
 * However, if timeToShow != -1 it is hidden
 * just like Windows 7 menus.
 * You need to install a shortcut that calls
 * showMenu() on Key_Alt events.
 * Note: if the menu is hidden, no actions
 * are propagated. So you should register
 * your actions additionally in the QMainWindow.
 **/ 
class DkMenuBar : public QMenuBar {
Q_OBJECT

public:

	
	/**
	 * Creates a DkMenuBar.
	 * @param parent the parent widget.
	 * @param timeToShow the time to show in ms. If set to -1 it acts like a QMenu.
	 **/ 
	DkMenuBar(QWidget *parent = 0, int timeToShow = 5000);

	/**
	 * Override method.
	 * @param menu a menu.
	 * @return QAction* the action returned by QMenuBar::addAction().
	 **/ 
	QAction* addMenu(QMenu *menu);
	
	/**
	 * Override method.
	 * @param title the menu title. (& indicates the shortcut key)
	 * @return QMenu* the menu returned by QMenuBar::addAction().
	 **/ 
	QMenu* addMenu(const QString & title);
	
	/**
	 * Override method.
	 * @param icon the menu icon.
	 * @param title the menu title.
	 * @return QMenu* the menu returned by QMenuBar::addAction().
	 **/ 
	QMenu* addMenu(const QIcon& icon, const QString& title);

	void setTimeToShow(int setTimeToShow);


public slots:

	/**
	 * Shows or hides the menu.
	 * If the menu is shown, the timer for the hide event is started.
	 **/ 
	void showMenu();

	/**
	 * Hides the menu if it is not active.
	 * In this context, active means that the mouse is over the menubar
	 * or one of its children is visible.
	 **/
	void hideMenu();

protected:
	void enterEvent(QEvent* event);
	void leaveEvent(QEvent* event);

private:
	QList<QMenu*> menus;
	bool active;
	int timeToShow;
	QPointer<QTimer> timerMenu;

};

class DkHistoryMenu : public QMenu {
	Q_OBJECT

public:
	DkHistoryMenu(const QString& title, QWidget* parent = 0, QStringList* recentFiles = 0) : QMenu(title, parent) {

		this->recentFiles = recentFiles;
	};

	void setVisible(bool visible) {
		
		if (visible) updateActions();
		QMenu::setVisible(visible);
	};

signals:
	void loadFileSignal(QFileInfo file);
	

public slots:

	void updateActions() {
		
		QMenu::clear();

		int numItems = 0;
		for (int idx = 0; idx < recentFiles->size(); idx++) {

			if (numItems >= 10) // TODO: setting??
				break;


			QFileInfo file = recentFiles->at(idx);

			// TODO: this line is sometimes (iPhone: ducking) slow!!
			if (!file.exists())
				continue;

			QString title = (file.isDir()) ? file.absoluteFilePath() : file.fileName();

			QAction* recentFileAction = new QAction(title, this);
			connect(recentFileAction, SIGNAL(triggered()), this, SLOT(loadRecentFile()));

			QMenu::addAction(recentFileAction);
			numItems++;
		}

		if (numItems == 0) {
			QAction* noItems = new QAction(tr("no entries"), this);
			noItems->setEnabled(false);
			QMenu::addAction(noItems);
		}
	};

	void loadRecentFile() {

		QAction* sender = dynamic_cast<QAction*>(QObject::sender());

		int fileIdx = -1;
		for (int idx = 0; idx < recentFiles->size(); idx++) {

			if (recentFiles->at(idx).isEmpty())
				continue;

			QFileInfo file = recentFiles->at(idx);

			if (file.fileName() == sender->text() || file.absoluteFilePath() == sender->text()) {
				fileIdx = idx;
				break;
			}
		}

		if (fileIdx == -1)
			return;

		emit loadFileSignal(recentFiles->at(fileIdx));
	};

protected:

	QStringList* recentFiles;
};


class DkTcpAction : public QAction {
	Q_OBJECT

public:
	DkTcpAction() : QAction(0) {};

	DkTcpAction(DkPeer peer, QObject* parent = 0) : QAction(parent) {
		this->peer = peer;
		init();
	};
	DkTcpAction(DkPeer peer, const QString& text, QObject* parent = 0) : QAction(text, parent) {
		this->peer = peer;
		init();
	};
	DkTcpAction(DkPeer peer, const QIcon& icon, const QString& text, QObject* parent) : QAction(icon, text, parent) {
		this->peer = peer;
		init();
	};

	~DkTcpAction() {};

	void init() {
		setCheckable(true);
		setChecked(peer.getSynchronized());
		connect(this, SIGNAL(triggered(bool)), this, SLOT(synchronize(bool)));
	};

	void setTcpActions(QList<QAction*> actions) {
		tcpActions = actions;
	};

signals:
	void synchronizeWithSignal(quint16);
	void disableSynchronizeWithSignal(quint16);

	public slots:
		void synchronize(bool checked) {

			if (checked)
				emit synchronizeWithSignal(peer.peerId);
			else
				emit disableSynchronizeWithSignal(peer.peerId);

			for (int idx = 0; idx < tcpActions.size(); idx++)
				tcpActions.at(idx)->setEnabled(!checked);

			qDebug() << "emitted a synchronize message...\n";
		}

protected:
	DkPeer peer;
	QList<QAction*> tcpActions;

};


class DkTcpMenu : public QMenu {
	Q_OBJECT

public:

	DkTcpMenu(QWidget* parent = 0, DkManagerThread* clientThread = 0) : QMenu(parent) {
		this->clientThread = clientThread;

		if (clientThread)
			connect(this, SIGNAL(synchronizeWithSignal(quint16)), clientThread, SLOT(synchronizeWith(quint16)));

		noClientsFound = false;
	};


	DkTcpMenu(const QString& title, QWidget* parent = 0, DkManagerThread* clientThread = 0) : QMenu(title, parent) {
		this->clientThread = clientThread;

		if (clientThread)
			connect(this, SIGNAL(synchronizeWithSignal(quint16)), clientThread, SLOT(synchronizeWith(quint16)));

		noClientsFound = false;
	};

	~DkTcpMenu() {};

	void setClientManager(DkManagerThread* clientThread) {
		this->clientThread = clientThread;
		if (clientThread)
			connect(this, SIGNAL(synchronizeWithSignal(quint16)), clientThread, SLOT(synchronizeWith(quint16)));
	};

	void addTcpAction(QAction* tcpAction) {
		tcpActions.append(tcpAction);
	};

	void showNoClientsFound(bool show) {
		noClientsFound = show;
	};

	void setVisible(bool visible) {

		if (visible)
			updatePeers();

		QMenu::setVisible(visible);

	};

	void clear() {
		QMenu::clear();
		peers.clear();
		clients.clear();
		tcpActions.clear();

	}

signals:
	void synchronizeWithSignal(quint16);

protected:
	void updatePeers() {	// find other clients on paint
		
		if (!clientThread)
			return;

		QList<DkPeer> newPeers = clientThread->getPeerList();	// TODO: remove old style

		// just update if the peers have changed...
		QMenu::clear();

		// show dummy action
		if (newPeers.empty() && noClientsFound) {
			qDebug() << "dummy node...\n";
			QAction* defaultAction = new QAction(tr("no clients found"), this);
			defaultAction->setEnabled(false);
			addAction(defaultAction);
			return;
		}

		if (!noClientsFound || !newPeers.empty()) {
			
			for (int idx = 0; idx < tcpActions.size(); idx++)
				addAction(tcpActions.at(idx));

			//QList<QAction*>::iterator actionIter = tcpActions.begin();
			//while (actionIter != tcpActions.end()) {
			//	addAction(*actionIter);
			//	actionIter++;
			//}
		}

		for (int idx = 0; idx < newPeers.size(); idx++) {

			DkPeer currentPeer = newPeers[idx];

			QString title = (noClientsFound) ? currentPeer.title : currentPeer.clientName % QString(": ") % currentPeer.title;

			DkTcpAction* peerEntry = new DkTcpAction(currentPeer, title, this);
			if (!noClientsFound) 
				peerEntry->setTcpActions(tcpActions);
			
			connect(peerEntry, SIGNAL(synchronizeWithSignal(quint16)), clientThread, SLOT(synchronizeWith(quint16)));
			connect(peerEntry, SIGNAL(disableSynchronizeWithSignal(quint16)), clientThread, SLOT(stopSynchronizeWith(quint16)));

			addAction(peerEntry);

		}

		peers = newPeers;

	};

	QList<DkPeer> peers;
	QList<QAction*> clients;
	QList<QAction*> tcpActions;
	DkManagerThread* clientThread;
	bool noClientsFound;
};