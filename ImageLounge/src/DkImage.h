/*******************************************************************************************************
 DkImage.h
 Created on:	21.04.2011
 
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

#include <QtGui/QWidget>
#include <QImageWriter>
#include <QFileSystemWatcher>
#include <QFileInfo>
#include <QFile>
#include <QSettings>
#include <QImageReader>
#include <QDir>
#include <QThread>
#include <QBuffer>
#include <QStringBuilder>
#include <QDebug>
#include <QMutex>
#include <QFileIconProvider>
#include <QStringList>

#include <exiv2/exiv2.hpp>
#include <libraw/libraw.h>

// opencv
#ifdef WITH_OPENCV
#include <opencv/cv.h>
using namespace cv;
#endif

#ifdef DK_DLL
#define DllExport __declspec(dllexport)
#else
#define DllExport
#endif


// TODO: ifdef
//#include <ShObjIdl.h>
//#include <ShlObj.h>
//#include <Windows.h>

// my classes
//#include "DkNoMacs.h"
#include "DkTimer.h"
#include "DkSettings.h"


#ifdef linux
	typedef  unsigned char byte;
#endif

// basic image processing
class DkImage {

public:

#ifdef WITH_OPENCV
	static Mat qImage2Mat(QImage img) {

		Mat mat2;
		if (img.format() == QImage::Format_ARGB32 || img.format() == QImage::Format_RGB32) {
			mat2 = Mat(img.height(), img.width(), CV_8UC4, (uchar*)img.bits(), img.bytesPerLine());
			qDebug() << "ARGB32 or RGB32";
		}
		else if (img.format() == QImage::Format_RGB888) {
			mat2 = Mat(img.height(), img.width(), CV_8UC3, (uchar*)img.bits(), img.bytesPerLine());
			qDebug() << "RGB888";
		}
		else if (img.format() == QImage::Format_Indexed8) {
			mat2 = Mat(img.height(), img.width(), CV_8UC1, (uchar*)img.bits(), img.bytesPerLine());
			qDebug() << "indexed...";
		}
		else {
			qDebug() << "sorry i could not convert the image...";
			mat2 = Mat();
		}

		mat2 = mat2.clone();	// we need to own the pointer

		return mat2; 
	}

	static QImage mat2QImage(Mat img) {

		QImage qImg;

		if (img.type() == CV_8UC1) {
			Mat tmp;
			cvtColor(img, tmp, CV_GRAY2RGB);	// Qt does not support writing to index8 images
			img = tmp;
		}
		if (img.type() == CV_8UC3) {
			cv::cvtColor(img, img, CV_RGB2BGR);
			qImg = QImage(img.data, img.cols, img.rows, img.step, QImage::Format_RGB888);
		}
		if (img.type() == CV_8UC4) {
			qImg = QImage(img.data, img.cols, img.rows, img.step, QImage::Format_RGB32);
		}

		qImg = qImg.copy();

		return qImg;
	}
#endif

};


class DkMetaData {

public:
	DkMetaData(QFileInfo file = QFileInfo()) {
		this->file = file;
		mdata = false;
	};
	
	DkMetaData(const DkMetaData& metaData);

	void setFileName(QFileInfo file) {
		this->file = file;
		mdata = false;
	};
	~DkMetaData() {};

	DkMetaData& operator=(const DkMetaData& metadata) {

		if (this == &metadata)
			return *this;

		this->file = metadata.file;
		this->mdata = false;
		
		return *this;
	};


	void reloadImg();

	void saveMetaDataToFile(QFileInfo fileN = QFileInfo(), int orientation = 0);

	std::string getNativeExifValue(std::string key);
	std::string getExifValue(std::string key);
	bool setExifValue(std::string key, std::string taginfo);
	std::string getIptcValue(std::string key);
	int getOrientation();
	QImage getThumbnail();
	void saveThumbnail(QImage thumb);
	void saveOrientation(int o);
	int getHorizontalFlipped();
	void saveHorizontalFlipped(int f);
	float getRating();
	void setRating(int r);
	bool isTiff();
	bool isJpg();
	bool isRaw();
	void printMetaData(); //only for debug
	QStringList getExifKeys();
	QStringList getExifValues();
	QStringList getIptcKeys();
	QStringList getIptcValues();


private:

	void readMetaData();

	Exiv2::Image::AutoPtr exifImg;
	QFileInfo file;

	bool mdata;

};

class DkThumbNail {

public:
	enum {
		exists_not = -1,
		not_loaded,
		loaded
	};
	
	DkThumbNail(QFileInfo file = QFileInfo(), QImage img = QImage()) {
		this->img = img;
		this->file = file;
		imgExists = true;
		s = max(img.width(), img.height());
	};
	~DkThumbNail() {};

	void setImage(QImage img) {
		this->img = img;
	}

	QImage getImage() {
		return img;
	};

	QFileInfo getFile() {
		return file;
	};

	int hasImage() {
		
		if (!img.isNull())
			return loaded;
		else if (img.isNull() && imgExists)
			return not_loaded;
		else
			return exists_not;
	};

	void setImgExists(bool exists) {
		imgExists = exists;
	}

	int size() {
		return s;
	};

private:
	QImage img;
	QFileInfo file;
	int s;
	bool imgExists;

};

class DkThumbsLoader : public QThread {

	Q_OBJECT

public:
	DkThumbsLoader(std::vector<DkThumbNail>* thumbs = 0, QDir dir = QDir());
	~DkThumbsLoader() {};

	void run();
	void stop();

signals:
	void updateSignal();

public slots:
	void setLoadLimits(int start = 0, int end = 20);


private:
	std::vector<DkThumbNail>* thumbs;
	QDir dir;
	bool isActive;
	bool somethingTodo;
	QMutex mutex;
	int maxThumbSize;
	int loadLimit;
	int startIdx;
	int endIdx;

	// function
	QImage getThumbNailQt(QFileInfo file);
	//QImage getThumbNailWin(QFileInfo file);
	void init();
	void loadThumbs();

};

class DllExport DkImageLoader : public QObject {

	Q_OBJECT

public:
	DkImageLoader(QFileInfo file = QFileInfo());

	virtual ~DkImageLoader();

	static QString saveFilter;		// for system close dialog
	static QString openFilter;		// for system  open dialog
	static QStringList fileFilters;	// just the filters
	static QStringList openFilters;	// for open dialog
	static QStringList saveFilters;	// for close dialog
	QStringList ignoreKeywords;
	QStringList keywords;

	static bool isValid(QFileInfo& fileInfo);
	static int locateFile(QFileInfo& fileInfo, QDir* dir = 0);
	static QStringList getFilteredFileList(QDir dir, QStringList ignoreKeywords = QStringList(), QStringList keywords = QStringList());

	bool updateFolder;
	bool silent;

	// TODO: thread here
	void rotateImage(double angle);
	void saveFile(QFileInfo filename, QString fileFilter = "", QImage saveImg = QImage(), int compression = -1);
	void setFile(QFileInfo& filename);
	QFileInfo getFile();
	QStringList getFiles();
	void nextFile(bool silent = false);
	void previousFile(bool silent = false);
	void firstFile();
	void lastFile();
	void loadFileAt(int idx);
	void changeFile(int skipIdx, bool silent = false);
	void clearPath();
	QString getCurrentFilter();
	QDir getDir();
	QDir getSaveDir();
	void setDir(QDir& dir);
	void setSaveDir(QDir& dir);
	void setImage(QImage& img);
	void load();
	void load(QFileInfo file, bool updateFolder = true, bool silent = false);
	bool hasFile();
	QString fileName();
	QFileInfo getChangedFileInfo(int skipIdx, bool silent = false);


	bool isWatcherEnabled() {
		return watcherEnabled;
	};

	QImage getImage() {
		return img;
	};

	DkMetaData getMetaData() {
		return dataExif;
	};

signals:
	void updateImageSignal(QImage img);
	void updateInfoSignal(QString msg, int time = 3000, int position = 0);
	void updateFileSignal(QFileInfo file, QSize s);
	void updateDirSignal(QFileInfo file);
	void newErrorDialog(QString msg, QString title = "Error");
	void fileNotLoadedSignal(QFileInfo file);

public slots:
	void fileChanged(const QString& path);
	void saveFileSilentIntern(QFileInfo file, QImage saveImg = QImage());
	void saveFileIntern(QFileInfo filename, QString fileFilter = "", QImage saveImg = QImage(), int compression = -1);
	virtual bool loadFile(QFileInfo file);
	void saveRating(int rating);
	void deleteFile();
	void saveTempFile(QImage img);
	void enableWatcher(bool enable);

protected:

	QFileInfo file;
	QFileInfo virtualFile;
	QDir dir;
	QDir saveDir;
	QFileSystemWatcher *watcher;
	bool watcherEnabled;

	DkMetaData dataExif;

	// threads
	QMutex mutex;
	QThread* loaderThread;

	QImage img;
	
	// functions
	void loadDir(QDir newDir);
	void saveFileSilentThreaded(QFileInfo file, QImage img = QImage());
	bool loadGeneral(QFileInfo file);
	bool loadRohFile(QString fileName);
	void updateHistory();
	
};