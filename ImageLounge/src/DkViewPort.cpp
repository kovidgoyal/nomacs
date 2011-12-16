/*******************************************************************************************************
 DkViewPort.cpp
 Created on:	05.05.2011
 
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

#include "DkViewPort.h"

// DkGraphicsView --------------------------------------------------------------------
DkViewPort::DkViewPort(QWidget *parent, Qt::WFlags flags) : QGraphicsView(parent) {

	this->parent = parent;
	overviewSize = 0.15f;
	overviewMargin = 10;
	viewportRect = QRect(0, 0, width(), height());
	testLoaded = false;
	worldMatrix.reset();
	isFrameless = false;
	altKeyPressed = false;
	visibleStatusbar = false;
	rating = -1;

	imgBg = QImage();
	imgBg.load(":/nomacs/img/nomacs-bg.png");

	loader = 0;
	centerLabel = 0;
	bottomLabel = 0;
	bottomRightLabel = 0;
	topLeftLabel = 0;

	centerLabel = new DkInfoLabel(this, "", DkInfoLabel::center_label);
	bottomLabel = new DkInfoLabel(this, "", DkInfoLabel::bottom_left_label);
	bottomRightLabel = new DkInfoLabel(this, "", DkInfoLabel::bottom_right_label);
	topLeftLabel = new DkInfoLabel(this, "", DkInfoLabel::top_left_label);
	fileInfoLabel = new DkFileInfoLabel(this);
	
	// wheel label
	QPixmap wp = QPixmap(":/nomacs/img/thumbs-move.png");
	wheelButton = new QLabel(this);
	wheelButton->setAttribute(Qt::WA_TransparentForMouseEvents);
	wheelButton->setPixmap(wp);
	wheelButton->hide();

	metaDataInfo = new DkMetaDataInfo(this);

	filePreview = new DkFilePreview(this, flags);
	filePreview->hide();

	player = new DkPlayer(this);
	addActions(player->getActions().toList());

	// TODO: read rating from file...
	ratingLabel = new DkRatingLabelBg(2, this, flags);
	addActions(ratingLabel->getActions().toList());		// register actions

	overviewWindow = new DkOverview(this, flags);
	overviewWindow->setTransforms(&worldMatrix, &imgMatrix);

	setAcceptDrops(true);
	//setContentsMargins(0, 0, 0, 0);
	//setMinimumSize(1, 1);
	
	setObjectName(QString::fromUtf8("DkViewPort"));
	//setDragMode(QGraphicsView::RubberBandDrag);
	//setInteractive(false);

	//no border
	//setStyleSheet( "QGraphicsView { border-style: none; background: QLinearGradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #edeff9, stop: 1 #bebfc7); }" );
	setStyleSheet( "QGraphicsView { border-style: none; background: QLinearGradient(x1: 0, y1: 0.7, x2: 0, y2: 1, stop: 0 #edeff9, stop: 1 #d9dbe4); }" );
	//setInteractive(false);
	setMouseTracking (true);//receive mouse event everytime
	
	loader = new DkImageLoader();

	connect(loader, SIGNAL(updateImageSignal(QImage)), this, SLOT(setImage(QImage)));
	connect(loader, SIGNAL(updateInfoSignal(QString, int, int)), this, SLOT(setInfo(QString, int, int)));
	connect(loader, SIGNAL(updateDirSignal(QFileInfo)), filePreview, SLOT(updateDir(QFileInfo)));
	connect(loader, SIGNAL(fileNotLoadedSignal(QFileInfo)), this, SLOT(fileNotLoaded(QFileInfo)));
	connect(loader, SIGNAL(updateFileSignal(QFileInfo, QSize)), metaDataInfo, SLOT(setFileInfo(QFileInfo, QSize)));
	connect(filePreview, SIGNAL(loadFileSignal(QFileInfo)), this, SLOT(loadFile(QFileInfo)));
	connect(overviewWindow, SIGNAL(moveViewSignal(QPointF)), this, SLOT(moveView(QPointF)));
	connect(overviewWindow, SIGNAL(sendTransformSignal()), this, SLOT(tcpSynchronize()));
	
	connect(player, SIGNAL(previousSignal(bool)), this, SLOT(loadPrevFile(bool)));
	connect(player, SIGNAL(nextSignal(bool)), this, SLOT(loadNextFile(bool)));
	connect(ratingLabel, SIGNAL(newRatingSignal(int)), this, SLOT(updateRating(int)));
	connect(fileInfoLabel->getRatingLabel(), SIGNAL(newRatingSignal(int)), this, SLOT(updateRating(int)));
	connect(ratingLabel, SIGNAL(newRatingSignal(int)), metaDataInfo, SLOT(setRating(int)));
	
	//connect(player, SIGNAL(play(bool)), this, play());

	qDebug() << "old viewer created...";

	//// >DIR:  [7.4.2011 diem]
	//trayIcon = new QSystemTrayIcon(this->windowIcon(), this);

	// TODO:
	// one could blur the canvas if a transparent GUI is present
	// what we would need: QGraphicsBlurEffect...
	// render all widgets to the alpha channel (bw)
	// pre-render the viewport to that image... apply blur
	// and then render the blurred image after the widget is rendered...
	// performance?!

}

DkViewPort::~DkViewPort() {

	release();
}

void DkViewPort::release() {

	if (loader) delete loader;
	if (centerLabel) delete centerLabel;
	if (bottomLabel) delete bottomLabel;
	if (bottomRightLabel) delete bottomRightLabel;
	if (topLeftLabel) delete topLeftLabel;
	if (fileInfoLabel) delete fileInfoLabel;
}

#ifdef WITH_OPENCV
void DkViewPort::setImage(cv::Mat newImg) {

	//if (this->img.size() != newImg.size()) {
	//	// do we really need a deep copy here?

	// TODO: be careful with the format!
	imgQt = QImage(newImg.data, newImg.cols, newImg.rows, newImg.step, QImage::Format_RGB888);
	//}

	this->imgRect = QRect(0, 0, newImg.cols, newImg.rows);


	if (!DkSettings::DisplaySettings::keepZoom)
		worldMatrix.reset();

	updateImageMatrix();

	if (DkSettings::DisplaySettings::keepZoom)
		centerImage();

	if (centerLabel) centerLabel->stop();
	if (bottomLabel) bottomLabel->stop();
	if (bottomRightLabel) bottomRightLabel->stop();
	if (topLeftLabel) topLeftLabel->stop();

	update();
}
#endif


void DkViewPort::setImage(QImage newImg) {

	overviewWindow->setImage(QImage());	// clear overview

	imgQt = newImg;
	QRectF oldImgRect = imgRect;
	this->imgRect = QRectF(0, 0, newImg.width(), newImg.height());

	emit enableNoImageSignal(!imgQt.isNull());

	qDebug() << "new image (viewport) loaded,  size: " << newImg.size() << "channel: " << imgQt.format();

	if (!DkSettings::DisplaySettings::keepZoom || imgRect != oldImgRect)
		worldMatrix.reset();							

	updateImageMatrix();

	emit windowTitleSignal(QFileInfo(), imgQt.size());
	player->startTimer();

	overviewWindow->setImage(imgQt);

	DkMetaData md = loader->getMetaData();
	QString dateString = QString::fromStdString(md.getExifValue("DateTimeOriginal"));
	fileInfoLabel->updateInfo(QFileInfo(), dateString, md.getRating());

	if (centerLabel) centerLabel->stop();
	if (bottomLabel) bottomLabel->stop();
	if (bottomRightLabel) bottomRightLabel->stop();
	//if (topLeftLabel) topLeftLabel->stop();	// top left should be always shown	(DkSnippet??)
	
	rating = -1;
	if (loader && ratingLabel)
		ratingLabel->setRating(md.getRating());

	if (isFrameless) {
		QRect w = initialWindow();
		setFramelessGeometry(QRect(newCenter(w.size()), w.size()));
	}

	update();
}

void DkViewPort::tcpSendImage() {

	setCenterInfo("sending image...");

	if (loader)
		sendImageSignal(imgQt, loader->fileName());
	else
		sendImageSignal(imgQt, "nomacs - Image Lounge");
}

void DkViewPort::fileNotLoaded(QFileInfo file) {

	// things todo if a file was not loaded...
	player->startTimer();

}

void DkViewPort::setTitleLabel(QFileInfo file, int time) {
	
	if (fileInfoLabel) {
		fileInfoLabel->updateTitle(file);
		fileInfoLabel->updatePos(bottomOffset);
	}
}

bool DkViewPort::imageInside() {

	return viewportRect.contains(worldMatrix.mapRect(imgViewRect));
}

// frameless functions --------------------------------------------------------------------
void DkViewPort::setFrameless(bool frameless) {

	if (frameless) {
		resetView();
		setAttribute(Qt::WA_TranslucentBackground, true);
		//setAttribute(Qt::WA_DontCreateNativeAncestors);
		setCursor(Qt::OpenHandCursor);
		show();
	}
	else {
		//setAttribute(Qt::WA_TranslucentBackground, false);
		//setWindowFlags(Qt::Widget);
		//setAttribute(Qt::WA_TranslucentBackground, false);
		//setAutoFillBackground(false);
		resetView();
		changeCursor();
		// TODO: why is my background getting black??
		//show();
	}

	isFrameless = frameless;
}
void DkViewPort::setFramelessGeometry(QRect r) {

	QDesktopWidget* dw = QApplication::desktop();
	QRect sRect = dw->screenGeometry();
	QRect winRect = geometry();

	if (r.left() < 0) {
		


	}

	//float ratio = imgQt.height()/imgQt.width();

	//if (ratio > 1.0f)
	//	r.setWidth(r.width()/ratio);
	//else if (ratio < 1.0f)
	//	r.setHeight(r.height()*ratio);

	// TODO: keep on screen (compensate with img matrix what window must not do)
	parent->setGeometry(r);
}

QRect DkViewPort::initialWindow() {

	QDesktopWidget* dw = QApplication::desktop();
	QRect sRect = dw->screenGeometry();
	QSize hs = sRect.size()/2;
	QPoint offset = QPoint(hs.width(), hs.height());

	QTransform t;
	t.scale(0.5f, 0.5f);

	QRect wRect = t.mapRect(sRect);

	if (!imgQt.isNull() && imgQt.width() < wRect.width() && imgQt.height() < wRect.height())
		wRect.setSize(imgQt.size());

	wRect.moveCenter(offset);
	qDebug() << "window rect: " << wRect;
	return wRect;
}

QPoint DkViewPort::newCenter(QSize s) {

	QSize dxy = (s - parent->size())/2;
	QPoint newPos = parent->pos() - QPoint(dxy.width(), dxy.height());

	return newPos;
}

// listener functions --------------------------------------------------------------------
//Note: move 2 percent of the window
void DkViewPort::shiftLeft() {

	float delta = 2*width()/(100.0*worldMatrix.m11());
	moveView(QPointF(delta,0));
}

//Note: move 2 percent of the window
void DkViewPort::shiftRight() {

	float delta = -2*width()/(100.0*worldMatrix.m11());
	moveView(QPointF(delta,0));
}

//Note: move 2 percent of the window
void DkViewPort::shiftUp() {

	float delta = 2*height()/(100.0*worldMatrix.m11());
	moveView(QPointF(0,delta));
}

//Note: move 2 percent of the window
void DkViewPort::shiftDown() {

	float delta = -2*height()/(100.0*worldMatrix.m11());
	moveView(QPointF(0,delta));
}

void DkViewPort::toggleShowOverview() {

	DkSettings::GlobalSettings::showOverview = !DkSettings::GlobalSettings::showOverview;
	update();
}

void DkViewPort::toggleShowPlayer() {

	if (player->isVisible())
		player->hide();
	else
		player->show();
}

void DkViewPort::zoomIn() {
	
		zoom(1.1f);
}

void DkViewPort::zoomOut() {
	
		zoom(0.9f);
}

void DkViewPort::zoom(float factor, QPointF center) {

	if (imgQt.isNull())
		return;

	// TODO: within screen?
	if (isFrameless) {

		QSize newSize = parent->size()*factor;
		
		if (newSize.width() < 100 || newSize.height() < 100)
			return;

		QRect winRect = QRect(newCenter(newSize), newSize);
		
		// check size
		setFramelessGeometry(winRect);
		showZoom();
		return;
	}

	//factor/=5;//-0.1 <-> 0.1
	//factor+=1;//0.9 <-> 1.1

	//limit zoom out ---
	if (worldMatrix.m11() == 1 && factor < 1)
		return;

	if (worldMatrix.m11()*factor < 1) {
		resetView();
		return;
	}

	//limit zoom in ---
	if (worldMatrix.m11()*imgMatrix.m11() > 50 && factor > 1)
		return;

	bool blackBorder = false;

	// if no center assigned: zoom in at the image center
	if (center.x() == -1 || center.y() == -1)
		center = QPointF((float)width()/2.0f, (float)height()/2.0f);
	else {

		// if black border - do not zoom to the mouse coordinate
		if ((float)imgViewRect.width()*(worldMatrix.m11()*factor) < (float)width()) {
			center.setX((float)width()/2.0f);
			blackBorder = true;
		}
		if (((float)imgViewRect.height()*worldMatrix.m11()*factor) < (float)height()) {
			center.setY((float)height()/2.0f);
			blackBorder = true;
		}
	}

	//inverse the transform
	int a, b;
	worldMatrix.inverted().map(center.x(), center.y(), &a, &b);

	worldMatrix.translate(a-factor*a, b-factor*b);
	worldMatrix.scale(factor, factor);
	
	controlImagePosition();
	if (blackBorder && factor < 1) centerImage();	// TODO: geht auch sch�ner
	showZoom();
	changeCursor();

	update();

	if (altKeyPressed && hasFocus())
		tcpSynchronize();
	
}

void DkViewPort::resetView() {

	if (isFrameless) {
		QRect r = initialWindow();
		setFramelessGeometry(r);
		return;
	}

	worldMatrix.reset();
	showZoom();
	changeCursor();

	update();

	if (altKeyPressed && hasFocus())
		tcpSynchronize();
}

void DkViewPort::fullView() {

	worldMatrix.reset();
	zoom(1.0f/imgMatrix.m11());
	showZoom();
	changeCursor();
	update();
}

void DkViewPort::changeCursor() {

	if (worldMatrix.m11() > 1 && !imageInside())
		setCursor(Qt::OpenHandCursor);
	else
		unsetCursor();
}

void DkViewPort::showZoom() {

	QString zoomStr;
	zoomStr.sprintf("%.1f%%", imgMatrix.m11()*worldMatrix.m11()*100);
	setBottomInfo(zoomStr);

}

void DkViewPort::showPreview() {

	if (imgQt.isNull())
		return;

	if (filePreview && filePreview->isVisible()) {
		filePreview->hide();
		topOffset.setY(0);
		update();
		return;
	}

	if (filePreview) {
		filePreview->show();
		update();
		topOffset.setY(filePreview->height());
	}

	topLeftLabel->updatePos(topOffset);

}

void DkViewPort::showInfo() {

	bool showInfo;

	if (fileInfoLabel && fileInfoLabel->isVisible()) {
		fileInfoLabel->hide();
		ratingLabel->block(false);
		showInfo = false;
	}
	else {
		fileInfoLabel->show();
		ratingLabel->block(DkSettings::SlideShowSettings::display.testBit(DkSlideshowSettingsWidget::display_file_rating));
		fileInfoLabel->updatePos(bottomOffset);
		showInfo = true;
	}

	if (parent->isFullScreen())
		DkSettings::GlobalSettings::showInfo = showInfo;
}

void DkViewPort::showExif() {

	if (imgQt.isNull())
		return;

	if (metaDataInfo && metaDataInfo->isVisible()) {
		metaDataInfo->hide();
		bottomOffset = QPoint(0,0);
	}
	else if (metaDataInfo) {
		metaDataInfo->show();
		bottomOffset = QPoint(0, -metaDataInfo->height());
	}

	bottomLabel->updatePos(bottomOffset);
	bottomRightLabel->updatePos(bottomOffset);
	fileInfoLabel->updatePos(bottomOffset);
	ratingLabel->move(10, height()-ratingLabel->size().height()-10+bottomOffset.y());
}

void DkViewPort::toggleResetMatrix() {

	DkSettings::DisplaySettings::keepZoom = !DkSettings::DisplaySettings::keepZoom;
}

void DkViewPort::moveView(QPointF delta) {

	QRectF imgWorldRect = worldMatrix.mapRect(imgViewRect);
	if (imgWorldRect.width() < this->width())
		delta.setX(0);
	if (imgWorldRect.height() < this->height())
		delta.setY(0);

	worldMatrix.translate(delta.x(), delta.y());
	controlImagePosition();
	//qDebug() << "moving view: " << delta;
	//centerImage();
	update();
}

void DkViewPort::controlImagePosition(float lb, float ub) {

	QRectF imgRectWorld = worldMatrix.mapRect(imgViewRect);

	if (lb == -1)	lb = viewportRect.width()/2;
	if (ub == -1)	ub = viewportRect.height()/2;

	if (imgRectWorld.left() > lb && imgRectWorld.width() > width())
		worldMatrix.translate((lb-imgRectWorld.left())/worldMatrix.m11(), 0);

	if (imgRectWorld.top() > ub && imgRectWorld.height() > height())
		worldMatrix.translate(0, (ub-imgRectWorld.top())/worldMatrix.m11());

	if (imgRectWorld.right() < lb && imgRectWorld.width() > width())
		worldMatrix.translate((lb-imgRectWorld.right())/worldMatrix.m11(), 0);

	if (imgRectWorld.bottom() < ub && imgRectWorld.height() > height())
		worldMatrix.translate(0, (ub-imgRectWorld.bottom())/worldMatrix.m11());

}

void DkViewPort::centerImage() {

	QRectF imgWorldRect = worldMatrix.mapRect(imgViewRect);
	float dx, dy;

	// if black border - center the image
	if (imgWorldRect.width() < (float)width()) {
		dx = ((float)width()-imgWorldRect.width())*0.5f-imgViewRect.x()*worldMatrix.m11();
		dx = (dx-worldMatrix.dx())/worldMatrix.m11();
		worldMatrix.translate(dx, 0);
	}
	else if (imgWorldRect.left() > 0)
		worldMatrix.translate(-imgWorldRect.left()/worldMatrix.m11(), 0);
	else if (imgWorldRect.right() < width())
		worldMatrix.translate((width()-imgWorldRect.right())/worldMatrix.m11(), 0);

	if (imgWorldRect.height() < height()) {
		dy = (height()-imgWorldRect.height())*0.5f-imgViewRect.y()*worldMatrix.m22();
		dy = (dy-worldMatrix.dy())/worldMatrix.m22();
		worldMatrix.translate(0, dy);
	}
	else if (imgWorldRect.top() > 0) {
		worldMatrix.translate(0, -imgWorldRect.top()/worldMatrix.m22());
	}
	else if (imgWorldRect.bottom() < height()) {
		worldMatrix.translate(0, (height()-imgWorldRect.bottom())/worldMatrix.m22());
	}

}

void DkViewPort::updateImageMatrix() {

	if (imgQt.isNull())
		return;

	QRectF oldImgRect = imgViewRect;
	QTransform oldImgMatrix = imgMatrix;

	imgMatrix.reset();

	// if the image is smaller or zoom is active: paint the image as is
	if (!viewportRect.contains(imgRect) || isFrameless)
		imgMatrix = getScaledImageMatrix();
	else {
		imgMatrix.translate((float)(width()-imgQt.width())*0.5f, (float)(height()-imgQt.height())*0.5f);
		imgMatrix.scale(1.0f, 1.0f);
	}

	imgViewRect = imgMatrix.mapRect(imgRect);

	// update world matrix
	if (worldMatrix.m11() != 1) {

		float scaleFactor = oldImgMatrix.m11()/imgMatrix.m11();
		double dx = oldImgRect.x()/scaleFactor-imgViewRect.x();
		double dy = oldImgRect.y()/scaleFactor-imgViewRect.y();

		worldMatrix.scale(scaleFactor, scaleFactor);
		worldMatrix.translate(dx, dy);
	}

	qDebug() << "image matrix updated...";
}

QTransform DkViewPort::getScaledImageMatrix() {

	// the image resizes as we zoom
	float ratioImg = imgRect.width()/imgRect.height();
	float ratioWin = (float)this->width()/(float)this->height();

	QTransform imgMatrix;
	float s;
	if (imgRect.width() == 0 || imgRect.height() == 0)
		s = 1.0f;
	else
		s = (ratioImg > ratioWin) ? (float)width()/imgRect.width() : (float)height()/imgRect.height();

	imgMatrix.scale(s, s);

	QRectF imgViewRect = imgMatrix.mapRect(imgRect);
	imgMatrix.translate((width()-imgViewRect.width())*0.5f/s, (height()-imgViewRect.height())*0.5f/s);

	return imgMatrix;
}

void DkViewPort::tcpSetTransforms(QTransform newWorldMatrix, QTransform newImgMatrix, QPointF canvasSize) {

	// ok relative transform
	if (canvasSize.isNull()) {

		moveView(QPointF(newWorldMatrix.dx(), newWorldMatrix.dy())/worldMatrix.m11());
	}
	else {

		worldMatrix = newWorldMatrix;
		imgMatrix = newImgMatrix;
		updateImageMatrix();

		QPointF imgPos = QPointF(canvasSize.x()*imgQt.width(), canvasSize.y()*imgQt.height());

		// go to screen coordinates
		imgPos = imgMatrix.map(imgPos);

		// go to world coordinates
		imgPos = worldMatrix.map(imgPos);

		// compute difference to current viewport center - in world coordinates
		imgPos = QPointF(width()*0.5f, height()*0.5f) - imgPos;

		// back to screen coordinates
		float s = worldMatrix.m11();
		worldMatrix.translate(imgPos.x()/s, imgPos.y()/s);
	}

	update();
}

void DkViewPort::tcpSetWindowRect(QRect rect) {
	this->setGeometry(rect);
}

void DkViewPort::tcpSynchronize(QTransform relativeMatrix) {
	
	if (relativeMatrix.isIdentity()) {

		QPointF size = QPointF(geometry().width()/2.0f, geometry().height()/2.0f);
		size = worldMatrix.inverted().map(size);
		size = imgMatrix.inverted().map(size);
		size = QPointF(size.x()/(float)imgQt.width(), size.y()/(float)imgQt.height());

		emit sendTransformSignal(worldMatrix, imgMatrix, size);
	}
	else {
		qDebug() << "sending relative...";
		emit sendTransformSignal(relativeMatrix, QTransform(), QPointF());
	}
}

void DkViewPort::tcpShowConnections(QList<DkPeer> peers) {

	QString newPeers;

	for (int idx = 0; idx < peers.size(); idx++) {
		
		DkPeer cp = peers.at(idx);

		if (cp.getSynchronized() && newPeers.isEmpty()) {
			newPeers = "connected with: ";
			emit newClientConnectedSignal();
		}
		else if (newPeers.isEmpty())
			newPeers = "disconnected with: ";
		
		newPeers.append("\n\t");

		if (!cp.clientName.isEmpty())
			newPeers.append(cp.clientName);
		if (!cp.clientName.isEmpty() && !cp.title.isEmpty())
			newPeers.append(": ");
		if (!cp.title.isEmpty())
			newPeers.append(cp.title);
	}

	//centerLabel->setTextFlags(Qt::AlignVCenter | Qt::TextExpandTabs);
	if (centerLabel)	centerLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
	if (centerLabel)	centerLabel->setText(newPeers, 5000);
	update();
}

void DkViewPort::paintEvent(QPaintEvent* event) {

	QPainter painter(viewport());

	// draw frame
	if (isFrameless)
		drawFrame(&painter);

	if (parent->isFullScreen())
		painter.fillRect(QRect(QPoint(), size()), DkSettings::SlideShowSettings::backgroundColor);

	if (!imgQt.isNull()) {
		painter.setWorldTransform(worldMatrix);

		if (imgMatrix.m11()*worldMatrix.m11() <= (float)DkSettings::DisplaySettings::interpolateZoomLevel/100.0f)
			painter.setRenderHint(QPainter::SmoothPixmapTransform);
		
		draw(&painter);
		//Now disable matrixWorld for overlay display
		painter.setWorldMatrixEnabled (false);
	}
	else {

		painter.setRenderHint(QPainter::SmoothPixmapTransform);
		
		// fit to viewport
		QSize s = imgBg.size();
		if (s.width() > (float)(size().width()*0.5))
			s = s*((size().width()*0.5)/s.width());

		if (s.height() > size().height()*0.6)
			s = s*((size().height()*0.6)/s.height());

		QRect bgRect(QPoint(size().width()-s.width()-size().width()*0.05, size().height()-s.height()-size().height()*0.05), s);

		painter.drawImage(bgRect, imgBg, QRect(QPoint(), imgBg.size()));
	}

	//in mode zoom/panning
	if (worldMatrix.m11() > 1 && !imageInside() && !isFrameless && DkSettings::GlobalSettings::showOverview) {
		
		if (filePreview->isVisible() && overviewWindow->isVisible()) {
			overviewWindow->move(overviewMargin, filePreview->geometry().bottom()+10);
		}
		else if (overviewWindow->isVisible()) {
			overviewWindow->move(overviewMargin, overviewMargin);
		}

		if (!overviewWindow->isVisible()) {
			overviewWindow->show();
			overviewWindow->update();
			topOffset.setX(overviewWindow->width()+10);
			topLeftLabel->updatePos(topOffset);
		}
	}
	else { 
		overviewWindow->hide();
		topOffset.setX(0);
		topLeftLabel->updatePos(topOffset);
	}

	int offset = (metaDataInfo->isVisible()) ? metaDataInfo->height()+10 : 10;


	// shouldn't we do this in resize??
	if (player) player->setGeometry(width()/2-player->width()/2, height()-player->height()-offset, width()/3, 0);

	// the labels must not be 0 !!
	//if (!imgQt.isNull()) centerLabel->show();
	bottomLabel->block(imgQt.isNull()/* || ratingLabel->isVisible()*/);
	
	// TODO: correct this...
	/*if (bottomRightLabel && parent && (!parent->isFullScreen() || imgQt.isNull()))*/ bottomRightLabel->hide();
	//if (titleInfoLabel && parent && (!parent->isFullScreen() || imgQt.isNull())) titleInfoLabel->hide();

	//if (!mouseTrace.empty())
	//	drawPolygon(&painter, &mouseTrace);

	// propagate
	QGraphicsView::paintEvent(event);

}

// drawing functions --------------------------------------------------------------------
void DkViewPort::draw(QPainter *painter) {

	// remove this method?!

	//painter->setRenderHint(QPainter::SmoothPixmapTransform);	//-> uncomment for smooth aliasing

	if (isFrameless && !imgQt.hasAlphaChannel()) {
		
		float fs = min(imgViewRect.width(), imgViewRect.height());
		int frame = fs-(fs*0.9f);

		QRectF framedImgRect = imgViewRect;
		framedImgRect.setSize(framedImgRect.size() - QSize(frame, frame));

		QPointF dxy = (imgViewRect.bottomRight() - framedImgRect.bottomRight())/2;
		framedImgRect.moveCenter(framedImgRect.center()+dxy);

		painter->drawImage(framedImgRect, imgQt, imgRect);
	}
	else
		painter->drawImage(imgViewRect, imgQt, imgRect);

}

void DkViewPort::drawPolygon(QPainter *painter, QPolygon *polygon) {

	QPoint lastPoint;

	for (int idx = 0; idx < polygon->size(); idx++) {

		QPoint p = polygon->at(idx);

		if (!lastPoint.isNull())
			painter->drawLine(p, lastPoint);

		lastPoint = p;
	}

}

void DkViewPort::drawFrame(QPainter* painter) {

	if (!imgQt.isNull() && imgQt.hasAlphaChannel())
		return;

	painter->setBrush(QColor(255, 255, 255, 200));
	painter->setPen(QColor(100, 100, 100, 255));
	
	QRectF frameRect = (!imgQt.isNull()) ? imgViewRect : QRect(QPoint(), size());
	frameRect.setSize(frameRect.size() - QSizeF(1,1));
	painter->drawRect(frameRect);

}

// event listeners --------------------------------------------------------------------
void DkViewPort::focusOutEvent(QEvent *event) {
	altKeyPressed = false;
}

void DkViewPort::resizeEvent(QResizeEvent *event) {

	qDebug() << "resize event...";

	viewportRect = QRect(0, 0, width(), height());

	// do we still need that??
	QSize newSize = imgQt.size();
	newSize.scale(event->size(), Qt::IgnoreAspectRatio);

	//resize(newSize);

	newSize = (event->size()-newSize)/2;
	move(newSize.width(), newSize.height());

	// >DIR: diem - bug if zoom factor is large and window becomes small
	updateImageMatrix();
	centerImage();
	changeCursor();

	// it is not always propagated?!
	filePreview->resizeEvent(event);
	metaDataInfo->resizeEvent(event);

	overviewWindow->resize(size()*overviewSize);
	//overviewWindow->setGeometry(QRect(QPoint(10, 10), size()*overviewSize));
	overviewWindow->setViewPortRect(geometry());

	topOffset.setX(overviewWindow->width()+10);

	ratingLabel->move(10, height()-ratingLabel->size().height()-10+bottomOffset.y());
	bottomLabel->updatePos(bottomOffset);
	bottomRightLabel->updatePos(bottomOffset);
	centerLabel->updatePos();
	topLeftLabel->updatePos(topOffset);	// todo: if thumbnails are shown: move/overview move
	fileInfoLabel->updatePos(bottomOffset);

	return QGraphicsView::resizeEvent(event);
}

// key events --------------------------------------------------------------------
void DkViewPort::keyPressEvent(QKeyEvent* event) {

	if (event->key() == Qt::Key_Escape) {
		if (filePreview->isVisible()) {
			filePreview->hide();
		}
		if (metaDataInfo->isVisible()) {
			metaDataInfo->hide();
		}
		update();
	}
	if (event->key() == Qt::Key_Alt) {
		altKeyPressed = true;
	}

	QGraphicsView::keyPressEvent(event);
}

void DkViewPort::keyReleaseEvent(QKeyEvent* event) {

	if (!event->isAutoRepeat())	qDebug() << "got a key (release) event...";

	if (!event->isAutoRepeat() && event->key() == Qt::Key_Alt) {
		altKeyPressed = false;
	}

#ifdef DK_DLL
	if (!event->isAutoRepeat())
		emit keyReleaseSignal(event);	// make key presses available
#endif

	QGraphicsView::keyReleaseEvent(event);

}


// mouse events --------------------------------------------------------------------
void DkViewPort::mousePressEvent(QMouseEvent *event) {

	qDebug() << "mouse pressed...";

	enterPos = event->pos();

	// move the window - todo: NOT full screen, window inside...
	if (isFrameless) {
		setCursor(Qt::ClosedHandCursor);
		posGrab = event->globalPos();
	}
	// ok, start panning
	else if (worldMatrix.m11() > 1 && !imageInside() && event->buttons() == Qt::LeftButton) {
		setCursor(Qt::ClosedHandCursor);
		posGrab = event->pos();
	}
	else if (filePreview && filePreview->isVisible() && event->buttons() == Qt::MiddleButton) {
		qDebug() << "middle button";

		QTimer* mImgTimer = filePreview->getMoveImageTimer();
		mImgTimer->start(1);

		// show icon
		wheelButton->move(event->pos().x()-16, event->pos().y()-16);
		wheelButton->show();
	}
	
	QWidget::mousePressEvent(event);
}

void DkViewPort::mouseReleaseEvent(QMouseEvent *event) {

	if (isFrameless)
		setCursor(Qt::OpenHandCursor);

	if (worldMatrix.m11() > 1 && !imageInside())
		setCursor(Qt::OpenHandCursor);

	if (filePreview) {
		
		qDebug() << "stopping...";
		filePreview->setCurrentDx(0);
		QTimer* mImgTimer = filePreview->getMoveImageTimer();
		mImgTimer->stop();
		wheelButton->hide();
	}
	
	qDebug() << "released...";

	QWidget::mouseReleaseEvent(event);
}

void DkViewPort::mouseDoubleClickEvent(QMouseEvent *event) {

	QCoreApplication::sendEvent(parent, event);
}

void DkViewPort::mouseMoveEvent(QMouseEvent *event) {

	if (visibleStatusbar)
		getPixelInfo(event->pos());

	if (isFrameless && event->buttons() == Qt::LeftButton) {
		
		QPoint dxy = event->globalPos() - posGrab.toPoint();
		//move(dxy);
		parent->move(parent->pos() + dxy);
		posGrab = event->globalPos();
		qDebug() << "moving: " << dxy;
		return;
	}

	if (worldMatrix.m11() > 1 && event->buttons() == Qt::LeftButton) {

		QPointF cPos = event->pos();
		QPointF dxy = (cPos - posGrab);
		posGrab = cPos;
		moveView(dxy/worldMatrix.m11());

		// with shift also a hotkey for fast switching...
		if ((DkSettings::SynchronizeSettings::syncAbsoluteTransform &&
			event->modifiers() == (Qt::AltModifier | Qt::ShiftModifier)) || 
			(!DkSettings::SynchronizeSettings::syncAbsoluteTransform &&
			event->modifiers() == (Qt::AltModifier))) {
			QTransform relTransform;
			relTransform.translate(dxy.x(), dxy.y());
			tcpSynchronize(relTransform);
		}
		else if (event->modifiers() == Qt::AltModifier)
			tcpSynchronize();
	}

	// scroll thumbs preview
	if (filePreview && filePreview->isVisible() && event->buttons() == Qt::MiddleButton) {
		
		qDebug() << "middle... move";
		qDebug() << "enter: " << enterPos.x();
		float dx = std::fabs(enterPos.x() - event->pos().x())*0.015;
		dx = std::exp(dx);
		if (enterPos.x() - event->pos().x() < 0)
			dx = -dx;

		filePreview->setCurrentDx(dx);	// update dx
	}

	QWidget::mouseMoveEvent(event);
}

void DkViewPort::wheelEvent(QWheelEvent *event) {

	float factor = -event->delta();
	if (DkSettings::DisplaySettings::invertZoom) factor *= -1.0f;

	factor /= -1200.0f;
	factor += 1.0f;

	zoom( factor, event->pos());
}

void DkViewPort::leaveEvent(QEvent *event) {

	altKeyPressed = false;
}

void DkViewPort::contextMenuEvent(QContextMenuEvent *event) {

	// send this event to my parent...
	QWidget::contextMenuEvent(event);
}

void DkViewPort::setFullScreen(bool fullScreen) {

	if (fullScreen) {
		player->show(3000);
		
		if (DkSettings::GlobalSettings::showInfo) {
			fileInfoLabel->show();
			ratingLabel->block(DkSettings::SlideShowSettings::display.testBit(DkSlideshowSettingsWidget::display_file_rating));
		}
		else
			fileInfoLabel->hide();
		
		filePreview->hide();
		metaDataInfo->hide();
		toggleLena();

		// TODO: qSettings
	}
	else {
		player->hide();
		player->play(false);
		ratingLabel->block(false);
		toggleLena();
		fileInfoLabel->hide();
	}
}

void DkViewPort::getPixelInfo(const QPoint& pos) {

	if (imgQt.isNull())
		return;

	QPointF imgPos = worldMatrix.inverted().map(QPointF(pos));
	imgPos = imgMatrix.inverted().map(imgPos);

	QPoint xy = imgPos.toPoint();

	if (xy.x() < 0 || xy.y() < 0 || xy.x() >= imgQt.width() || xy.y() >= imgQt.height())
		return;

	QColor col = imgQt.pixel(xy);
	
	QString msg = "<font color='grey'>x: " % QString::number(xy.x()) % " y: " % QString::number(xy.y()) % "</font>"
		" | r: " % QString::number(col.red()) % " g: " % QString::number(col.green()) % " b: " % QString::number(col.blue());

	if (imgQt.hasAlphaChannel())
		msg = msg % " a: " % QString::number(col.alpha());

	emit statusInfoSignal(msg);

}

// edit image --------------------------------------------------------------------
void DkViewPort::rotateCW() {

	if (loader != 0)
		loader->rotateImage(90);

}

void DkViewPort::rotateCCW() {

	if (loader != 0)
		loader->rotateImage(-90);

}

void DkViewPort::rotate180() {

	if (loader != 0)
		loader->rotateImage(180);

}



// file handling --------------------------------------------------------------------
void DkViewPort::loadLena() {

	bool ok;
	QString text = QInputDialog::getText(this, tr("Lena"), tr("A remarkable woman"), QLineEdit::Normal, 0, &ok);

	// pass phrase
	if (ok && !text.isEmpty() && text == "lena") {
		testLoaded = true;
		toggleLena();
	}
	else
		QApplication::beep();
}

void DkViewPort::toggleLena() {

	if (!testLoaded)
		return;

	if (loader) {
		if (parent->isFullScreen())
			loader->load(QFileInfo(":/nomacs/img/lena-full.jpg"));
		else
			loader->load(QFileInfo(":/nomacs/img/lena.jpg"));
	}
}

void DkViewPort::settingsChanged() {

	reloadFile();

	// update the title label
	if (fileInfoLabel && fileInfoLabel->isVisible())
		fileInfoLabel->show();

	if (parent->isFullScreen())
		ratingLabel->block(DkSettings::SlideShowSettings::display.testBit(DkSlideshowSettingsWidget::display_file_rating));
}

void DkViewPort::updateRating(int rating) {
	this->rating = rating;
	
	if (fileInfoLabel)
		fileInfoLabel->updateRating(rating);
}

void DkViewPort::unloadImage() {

	if (imgQt.isNull() || rating == -1 || rating == loader->getMetaData().getRating()) 
		return;

	if (loader) loader->saveRating(rating);

}

void DkViewPort::loadFile(QFileInfo file, bool updateFolder, bool silent) {

	qDebug() << "viewport thread id: " << QThread::currentThreadId();

	testLoaded = false;
	saveXML();
	unloadImage();

	if (loader && file.isDir()) {
		QDir dir = QDir(file.absoluteFilePath());
		loader->setDir(dir);
	} else if (loader)
		loader->load(file, updateFolder, silent);
}

void DkViewPort::reloadFile() {

	saveXML();
	unloadImage();

	if (loader) {
		loader->updateFolder = false;
		loader->changeFile(0);

		if (filePreview)
			filePreview->updateDir(loader->getFile(), true);
	}
}

void DkViewPort::loadNextFile(bool silent) {

	saveXML();
	unloadImage();

	if (loader != 0 && !testLoaded)
		loader->nextFile(silent || (parent->isFullScreen() && DkSettings::SlideShowSettings::silentFullscreen));

	if (altKeyPressed && hasFocus())
		emit sendNewFileSignal(1);
}

void DkViewPort::loadPrevFile(bool silent) {

	saveXML();
	unloadImage();

	if (loader != 0 && !testLoaded)
		loader->previousFile(silent || (parent->isFullScreen() && DkSettings::SlideShowSettings::silentFullscreen));

	// TODO: what happens if a file cannot be loaded?? (currently nothing happens -> no information no loading)
	
	if (altKeyPressed && hasFocus())
		emit sendNewFileSignal(-1);
}

void DkViewPort::loadFirst() {

	saveXML();
	unloadImage();

	if (loader && !testLoaded)
		loader->firstFile();

	if (altKeyPressed && hasFocus())
		emit sendNewFileSignal(SHRT_MIN);
}

void DkViewPort::loadLast() {

	saveXML();
	unloadImage();

	if (loader && !testLoaded)
		loader->lastFile();		// TODO: add silent in DkImage

	if (altKeyPressed && hasFocus())
		emit sendNewFileSignal(SHRT_MAX);

}

void DkViewPort::loadSkipPrev10() {

	unloadImage();

	if (loader && !testLoaded)
		loader->changeFile(-DkSettings::GlobalSettings::skipImgs, (parent->isFullScreen() && DkSettings::SlideShowSettings::silentFullscreen));

	if (altKeyPressed && hasFocus())
		emit sendNewFileSignal(-DkSettings::GlobalSettings::skipImgs);
}

void DkViewPort::loadSkipNext10() {

	unloadImage();

	if (loader && !testLoaded)
		loader->changeFile(DkSettings::GlobalSettings::skipImgs, (parent->isFullScreen() && DkSettings::SlideShowSettings::silentFullscreen));

	if (altKeyPressed && hasFocus())
		emit sendNewFileSignal(DkSettings::GlobalSettings::skipImgs);
}

void DkViewPort::tcpLoadFile(qint16 idx, QString filename) {

	qDebug() << "I got a file request??";

	if (filename.isEmpty()) {

		// change the file idx according to my brother
		switch (idx) {
			case SHRT_MIN:
				loadFirst();
				break;
			case SHRT_MAX:
				loadLast();
				break;
			case 1:
				loadNextFile();
				break;
			case -1:
				loadPrevFile();
				break;
			default:
				if (loader) loader->loadFileAt(idx);
		}
	}
	else 
		loadFile(QFileInfo(filename));

}

DkImageLoader* DkViewPort::getImageLoader() {

	return loader;
}

DkPlayer* DkViewPort::getPlayer() {

	return player;
}

DkOverview* DkViewPort::getOverview() {

	return overviewWindow;
}

QImage& DkViewPort::getImage() {

	return imgQt;
}

DkFilePreview* DkViewPort::getFilePreview() {

	return filePreview;
}

DkMetaDataInfo* DkViewPort::getMetaDataWidget() {

	return metaDataInfo;
}

DkFileInfoLabel* DkViewPort::getFileInfoWidget() {

	return fileInfoLabel;
}

void DkViewPort::setInfo(QString msg, int time, int location) {

	if (location == DkInfoLabel::center_label && centerLabel)
		centerLabel->setText(msg, time);
	else if (location == DkInfoLabel::bottom_left_label && bottomLabel)
		bottomLabel->setText(msg, time);
	else if (location == DkInfoLabel::bottom_right_label && bottomRightLabel) {
		bottomRightLabel->setText(msg, time);
		bottomRightLabel->updatePos(bottomOffset);
	}
	else if (location == DkInfoLabel::top_left_label && topLeftLabel) {
		topLeftLabel->setText(msg, time);
		topLeftLabel->updatePos(topOffset);
	}
	
	update();
}

void DkViewPort::setCenterInfo(QString msg, int time) {

	if (!centerLabel)
		return;

	centerLabel->setText(msg, time);
	update();
}

void DkViewPort::setBottomInfo(QString msg, int time) {

	if (!bottomLabel)
		return;

	bottomLabel->setText(msg, time);
	update();
}

void DkViewPort::printImage() {

	QPrinter printer;

	QPrintDialog *dialog = new QPrintDialog(&printer, this);
	dialog->setWindowTitle(tr("Print Document"));
	if (!imgQt.isNull())
		dialog->addEnabledOption(QAbstractPrintDialog::PrintSelection);
	if (dialog->exec() != QDialog::Accepted)
		return;

	printf("%i x %i\n", printer.pageRect(QPrinter::Inch).height(), printer.pageRect(QPrinter::Inch).width());

	// TODO: not that stupid...
	QPainter painter(&printer);
	QRect rect = painter.viewport();
	QSize size = imgQt.size();
	size.scale(rect.size(), Qt::KeepAspectRatio);
	painter.setViewport(rect.x(), rect.y(), size.width(), size.height());
	painter.setWindow(imgQt.rect());
	painter.drawImage(0, 0, imgQt);

	painter.end();
}

// custom events --------------------------------------------------------------------
//QEvent::Type DkInfoEvent::infoEventType = static_cast<QEvent::Type>(QEvent::registerEventType());
//QEvent::Type DkLoadImageEvent::eventType = static_cast<QEvent::Type>(QEvent::registerEventType());