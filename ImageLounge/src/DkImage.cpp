/*******************************************************************************************************
 DkImage.cpp
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

#include "DkImage.h"

// well this is pretty shitty... but we need the filter without description too
QStringList DkImageLoader::fileFilters = QString("*.png *.jpg *.tif *.bmp *.ppm *.xbm *.xpm *.gif *.pbm *.pgm *.jpeg *.tiff *.ico *.nef *.crw *.cr2 *.arw *.roh").split(' ');

// formats we can save
QString DkImageLoader::saveFilter = QString("PNG (*.png);;JPEG (*.jpg *.jpeg);;") %
	QString("TIFF (*.tif *.tiff);;") %
	QString("Windows Bitmap (*.bmp);;") %
	QString("Portable Pixmap (*.ppm);;") %
	QString("X11 Bitmap (*.xbm);;") %
	QString("X11 Pixmap (*.xpm)");

// formats we can save
QStringList DkImageLoader::saveFilters = saveFilter.split(QString(";;"));

QString DkImageLoader::openFilter = QString("Image Files (*.jpg *.png *.tif *.bmp *.gif *.pbm *.pgm *.xbm *.xpm *.ppm *.jpeg *.tiff *.ico *.nef *.crw *.cr2 *.arw *.roh);;") %
	QString(saveFilter) %
	QString(";;Graphic Interchange Format (*.gif);;") %
	QString("Portable Bitmap (*.pbm);;") %
	QString("Portable Graymap (*.pgm);;") %
	QString("Icon Files (*.ico);;") %
	QString("Nikon Raw (*.nef);;") %
	QString("Canon Raw (*.crw *.cr2);;") %
	QString("Sony Raw (*arw);;") %
	QString("Rohkost (*.roh);;");
	

// formats we can load
QStringList DkImageLoader::openFilters = openFilter.split(QString(";;"));

DkImageLoader::DkImageLoader(QFileInfo file) {

	qRegisterMetaType<QFileInfo>("QFileInfo");
	loaderThread = new QThread;
	loaderThread->start();
	moveToThread(loaderThread);
	
	dir = DkSettings::GlobalSettings::lastDir;
	saveDir = DkSettings::GlobalSettings::lastSaveDir;

	updateFolder = true;
	silent = false;
	 
	this->file = file;
	this->virtualFile = file;

	//watcher = 0;
	// init the watcher
	watcher = new QFileSystemWatcher();
	connect(watcher, SIGNAL(fileChanged(QString)), this, SLOT(fileChanged(QString)));

	if (file.exists())
		loadDir(file.absoluteDir());
}

DkImageLoader::~DkImageLoader() {

	loaderThread->exit(0);
	loaderThread->wait();
	delete loaderThread;

	qDebug() << "dir open: " << dir.absolutePath();
	qDebug() << "filepath: " << saveDir.absolutePath();
}

void DkImageLoader::clearPath() {
	
	img = QImage();
	file = QFileInfo();
	dataExif = DkMetaData(file);	// unload exif too
	//dir = QDir();
}

void DkImageLoader::loadDir(QDir newDir) {

	if (newDir.exists())
		dir = newDir;

	dir.setNameFilters(fileFilters);
	dir.setSorting(QDir::LocaleAware);
}

void DkImageLoader::nextFile(bool silent) {
	
	qDebug() << "loading next file";
	changeFile(1, silent);
	
}

void DkImageLoader::previousFile(bool silent) {
	
	qDebug() << "loading previous file";
	changeFile(-1, silent);
}

void DkImageLoader::changeFile(int skipIdx, bool silent) {

	//if (!img.isNull() && !file.exists())
	//	return;
	if (!file.exists() && !virtualFile.exists()) {
		qDebug() << virtualFile.absoluteFilePath() << "does not exist...";
		return;
	}

	mutex.lock();
	QFileInfo loadFile = getChangedFileInfo(skipIdx);
	qDebug() << "loading: " << file.absoluteFilePath();
	mutex.unlock();

	if (loadFile.exists())
		load(loadFile, true, silent);
}

QFileInfo DkImageLoader::getChangedFileInfo(int skipIdx, bool silent) {

	QDir newDir = (virtualFile.exists()) ? virtualFile.absoluteDir() : file.absoluteDir();
	loadDir(newDir);

	// locate the current file
	QStringList files = getFilteredFileList(dir, ignoreKeywords, keywords);
	QString cFilename = (virtualFile.exists()) ? virtualFile.fileName() : file.fileName();
	int cFileIdx = 0;
	int newFileIdx = 0;

	//qDebug() << "virtual file " << virtualFile.absoluteFilePath();
	//qDebug() << "file" << file.absoluteFilePath();

	if (file.exists() || virtualFile.exists()) {

		for ( ; cFileIdx < files.size(); cFileIdx++) {

			if (files[cFileIdx] == cFilename)
				break;
		}

		qDebug() << "my idx " << cFileIdx;
		newFileIdx = cFileIdx + skipIdx;

		if (DkSettings::GlobalSettings::loop) {
			newFileIdx %= files.size();

			while (newFileIdx < 0)
				newFileIdx = files.size() + newFileIdx;

		}
		else if (cFileIdx > 0 && newFileIdx < 0) {
			newFileIdx = 0;
		}
		else if (cFileIdx < files.size()-1 && newFileIdx >= files.size()) {
			newFileIdx = files.size()-1;
		}
		else if (newFileIdx < 0) {
			QString msg = "You have reached the beginning";
			if (!silent)
				updateInfoSignal(msg, 1000);
			return QFileInfo();
		}
		else if (newFileIdx >= files.size()) {
			QString msg = "You have reached the end";
			
			if (!silent)
				updateInfoSignal(msg, 1000);
			return QFileInfo();
		}
		qDebug() << "idx: " << newFileIdx;
	}

	// file requested becomes current file
	return (files.isEmpty()) ? QFileInfo() : QFileInfo(dir, files[newFileIdx]);


}

QStringList DkImageLoader::getFiles() {

	return getFilteredFileList(dir, ignoreKeywords, keywords);
}

void DkImageLoader::firstFile() {

	loadFileAt(0);
}

void DkImageLoader::lastFile() {
	
	loadFileAt(-1);
}

void DkImageLoader::loadFileAt(int idx) {

	if (!img.isNull() && !file.exists())
		return;

	mutex.lock();

	if (!dir.exists()) {
		QDir newDir = (virtualFile.exists()) ? virtualFile.absoluteDir() : file.absolutePath();	
		loadDir(newDir);
	}

	QStringList files = getFilteredFileList(dir, ignoreKeywords, keywords);

	qDebug() << "virtual file: " << virtualFile.absoluteFilePath();
	qDebug() << "real file " << file.absoluteFilePath();

	if (dir.exists()) {

		if (idx == -1) {
			idx = files.size()-1;
		}
		else if (DkSettings::GlobalSettings::loop) {
			idx %= files.size();

			while (idx < 0)
				idx = files.size() + idx;

		}
		else if (idx < 0 && !DkSettings::GlobalSettings::loop) {
			QString msg = "You have reached the beginning";
			updateInfoSignal(msg, 1000);
			mutex.unlock();
			return;
		}
		else if (idx >= files.size()) {
			QString msg = "You have reached the end";
			updateInfoSignal(msg, 1000);
			mutex.unlock();
			return;
		}

	}

	// file requested becomes current file
	QFileInfo loadFile = QFileInfo(dir, files[idx]);
	qDebug() << "loading: " << loadFile.absoluteFilePath();

	mutex.unlock();
	load(loadFile);

}


void DkImageLoader::load() {

	load(file);
}

void DkImageLoader::load(QFileInfo file, bool updateFolder, bool silent) {

	this->updateFolder = updateFolder;
	this->silent = silent;
	QMetaObject::invokeMethod(this, "loadFile", Qt::QueuedConnection, Q_ARG(QFileInfo, file));
}

bool DkImageLoader::loadFile(QFileInfo file) {

	qDebug() << "loading...";
	QMutexLocker locker(&mutex);
	
	if (!file.exists()) {
		
		if (!silent) {
			QString msg = "Sorry, the file: " % file.fileName() % " does not exist...";
			updateInfoSignal(msg);
		}
		return false;
	}
	else if (!file.permission(QFile::ReadUser)) {
		
		if (!silent) {
			QString msg = "Sorry, you are not allowed to read: " % file.fileName();
			updateInfoSignal(msg);
		}
		return false;
	}
	
	DkTimer dt;

	//test exif
	//DkExif dataExif = DkExif(file);
	//int orientation = dataExif.getOrientation();
	//dataExif.getExifKeys();
	//dataExif.getExifValues();
	//dataExif.getIptcKeys();
	//dataExif.getIptcValues();
	//dataExif.saveOrientation(90);

	if (!silent)
		emit updateInfoSignal("loading...", -1);

	//bool imgLoaded = img.load(file.absoluteFilePath());
	
	bool imgLoaded;
	try {
		imgLoaded = loadGeneral(file);
	} catch(...) {
		imgLoaded = false;
	}
	
	if (!silent)
		emit updateInfoSignal("loading...", 1);	// stop showing

	qDebug() << "image loaded in: " << QString::fromStdString(dt.getTotal());
	//qDebug() << "loader thread id: " << QThread::currentThreadId();
	this->virtualFile = file;

	if (imgLoaded) {
		
		dataExif = DkMetaData(file);
		int orientation = dataExif.getOrientation();

		if (orientation != -1 && !dataExif.isTiff() && orientation != 0) {
			QTransform rotationMatrix;
			rotationMatrix.rotate((double)orientation);
			img = img.transformed(rotationMatrix);
		}

		// update watcher
		if (this->file.exists() && watcher)
			watcher->removePath(this->file.absoluteFilePath());
		
		if (/*updateFolder && */watcher)	// diem: with updateFolder images are just updated once
			watcher->addPath(file.absoluteFilePath());
		
		emit updateImageSignal(img);
		emit updateFileSignal(file, img.size());

		if (updateFolder) {
			emit updateDirSignal(file);
			this->file = file;
			loadDir(file.absoluteDir());
		}

		// update history
		updateHistory();

	}
	else {
		if (!silent) {
			QString msg = "Sorry, I could not load: " % file.fileName();
			updateInfoSignal(msg);
		}
		fileNotLoadedSignal(file);

		qDebug() << "I did load it silent: " << silent;
		return false;
	}

	return true;
}


bool DkImageLoader::loadRohFile(QString fileName){

	bool imgLoaded = true;

	void *pData;
	FILE * pFile;
	unsigned char test[2];
	unsigned char *buf;
	int rohW = 4000;
	int rohH = 2672;
	pData = malloc(sizeof(unsigned char) * (rohW*rohH*2));
	buf = (unsigned char *)malloc(sizeof(unsigned char) * (rohW*rohH));

	try{
		pFile = fopen (fileName.toStdString().c_str(), "rb" );

		

		fread(pData, 2, rohW*rohH, pFile);

		fclose(pFile);

		for (long i=0; i < (rohW*rohH); i++){
		
			test[0] = ((unsigned char*)pData)[i*2];
			test[1] = ((unsigned char*)pData)[i*2+1];
			test[0] = test[0] >> 4;
			test[0] = test[0] & 15;
			test[1] = test[1] << 4;
			test[1] = test[1] & 240;

			buf[i] = (test[0] | test[1]);
		
		}

		img = QImage((const uchar*) buf, rohW, rohH, QImage::Format_Indexed8);
	
		//img = img.copy();
		QVector<QRgb> colorTable;

		for (int i = 0; i < 256; i++)
			colorTable.push_back(QColor(i, i, i).rgb());
		img.setColorTable(colorTable);

	} catch(...) {
		imgLoaded = false;
	}
	
	free(buf);
	free(pData);


	return imgLoaded;

}

bool DkImageLoader::loadGeneral(QFileInfo file) {

	bool imgLoaded = false;

	QString newSuffix = file.suffix();

	
	if (newSuffix.contains(QRegExp("(roh)", Qt::CaseInsensitive))) {

		imgLoaded = loadRohFile(file.absoluteFilePath());

	} else if (!newSuffix.contains(QRegExp("(nef|crw|cr2|arw)", Qt::CaseInsensitive))) {

		// if image has Indexed8 + alpha channel -> we crash... sorry for that
		imgLoaded = img.load(file.absoluteFilePath());
	
	} else {

		try {

#ifdef WITH_OPENCV

			LibRaw iProcessor;
			QImage image;
			int orientation = 0;

			iProcessor.open_file(file.absoluteFilePath().toStdString().c_str());

			//// (-w) Use camera white balance, if possible (otherwise, fallback to auto_wb)
			//iProcessor.imgdata.params.use_camera_wb = 1;
			//// (-a) Use automatic white balance obtained after averaging over the entire image
			//iProcessor.imgdata.params.use_auto_wb = 1;
			//// (-q 3) Adaptive homogeneity-directed de-mosaicing algorithm (AHD)
			//iProcessor.imgdata.params.user_qual = 3;
			//iProcessor.imgdata.params.output_tiff = 1;
			////iProcessor.imgdata.params.four_color_rgb = 1;
			////iProcessor.imgdata.params.output_color = 1; //sRGB  (0...raw)
			//// RAW data filtration mode during data unpacking and post-processing
			//iProcessor.imgdata.params.filtering_mode = LIBRAW_FILTERING_AUTOMATIC;

			iProcessor.unpack();
			//iProcessor.dcraw_process();
			//iProcessor.dcraw_ppm_tiff_writer("test.tiff");

			unsigned short cols = iProcessor.imgdata.sizes.width,//.raw_width,
			 			   rows = iProcessor.imgdata.sizes.height;//.raw_height;

			Mat rawMat, rgbImg;

			if (iProcessor.imgdata.idata.filters == 0)
			{
				//iProcessor.dcraw_process();
				rawMat = Mat(rows, cols, CV_32FC3);
				rawMat.setTo(0);
				std::vector<Mat> rawCh;
				split(rawMat, rawCh);

				for (unsigned int row = 0; row < rows; row++)
				{
					float *ptrR = rawCh[0].ptr<float>(row);
					float *ptrG = rawCh[1].ptr<float>(row);
					float *ptrB = rawCh[2].ptr<float>(row);
					//float *ptrE = rawCh[3].ptr<float>(row);

					for (unsigned int col = 0; col < cols; col++)
					{
						ptrR[col] = (float)iProcessor.imgdata.image[cols*row + col][0]/(float)iProcessor.imgdata.color.maximum;
						ptrG[col] = (float)iProcessor.imgdata.image[cols*row + col][1]/(float)iProcessor.imgdata.color.maximum;
						ptrB[col] = (float)iProcessor.imgdata.image[cols*row + col][2]/(float)iProcessor.imgdata.color.maximum;
						//ptrE[colIdx] = (float)iProcessor.imgdata.image[iProcessor.imgdata.sizes.width*row + col][3]/(float)iProcessor.imgdata.color.maximum;
					}
				}
				merge(rawCh, rawMat);
				rawMat.convertTo(rgbImg, CV_8U, 255);
				
				image = QImage(rgbImg.data, rgbImg.cols, rgbImg.rows, rgbImg.step, QImage::Format_RGB888);

			}
			else
			{

				qDebug() << "----------------";
				qDebug() << "Bayer Pattern: " << QString::fromStdString(iProcessor.imgdata.idata.cdesc);
				qDebug() << "Camera manufacturer: " << QString::fromStdString(iProcessor.imgdata.idata.make);
				qDebug() << "Camera model: " << QString::fromStdString(iProcessor.imgdata.idata.model);
				qDebug() << "canon_ev " << (float)iProcessor.imgdata.color.canon_ev;

				//qDebug() << "white: [%.3f %.3f %.3f %.3f]\n", iProcessor.imgdata.color.cam_mul[0],
				//	iProcessor.imgdata.color.cam_mul[1], iProcessor.imgdata.color.cam_mul[2],
				//	iProcessor.imgdata.color.cam_mul[3]);
				//qDebug() << "black: %i\n", iProcessor.imgdata.color.black);
				//qDebug() << "maximum: %.i %i\n", iProcessor.imgdata.color.maximum,
				//	iProcessor.imgdata.params.adjust_maximum_thr);
				//qDebug() << "gamma: %.3f %.3f %.3f %.3f %.3f %.3f\n",
				//	iProcessor.imgdata.params.gamm[0],
				//	iProcessor.imgdata.params.gamm[1],
				//	iProcessor.imgdata.params.gamm[2],
				//	iProcessor.imgdata.params.gamm[3],
				//	iProcessor.imgdata.params.gamm[4],
				//	iProcessor.imgdata.params.gamm[5]);
				
				qDebug() << "----------------";

				if (strcmp(iProcessor.imgdata.idata.cdesc, "RGBG")) throw DkException("Wrong Bayer Pattern (not RGBG)\n", __LINE__, __FILE__);

				rawMat = Mat(rows, cols, CV_32FC1);
				//rawMat.setTo(0);
				float mulWhite[4];
				//mulWhite[0] = iProcessor.imgdata.color.cam_mul[0] > 10 ? iProcessor.imgdata.color.cam_mul[0]/255.0f : iProcessor.imgdata.color.cam_mul[0];
				//mulWhite[1] = iProcessor.imgdata.color.cam_mul[1] > 10 ? iProcessor.imgdata.color.cam_mul[1]/255.0f : iProcessor.imgdata.color.cam_mul[1];
				//mulWhite[2] = iProcessor.imgdata.color.cam_mul[2] > 10 ? iProcessor.imgdata.color.cam_mul[2]/255.0f : iProcessor.imgdata.color.cam_mul[2];
				//mulWhite[3] = iProcessor.imgdata.color.cam_mul[3] > 10 ? iProcessor.imgdata.color.cam_mul[3]/255.0f : iProcessor.imgdata.color.cam_mul[3];
				//if (mulWhite[3] == 0)
				//	mulWhite[3] = mulWhite[1];

				mulWhite[0] = iProcessor.imgdata.color.cam_mul[0];
				mulWhite[1] = iProcessor.imgdata.color.cam_mul[1];
				mulWhite[2] = iProcessor.imgdata.color.cam_mul[2];
				mulWhite[3] = iProcessor.imgdata.color.cam_mul[3];

				float dynamicRange = iProcessor.imgdata.color.maximum-iProcessor.imgdata.color.black;

				float w = (mulWhite[0] + mulWhite[1] + mulWhite[2] + mulWhite[3])/4.0f;
				float maxW = 1.0f;//mulWhite[0];

				if (w > 2.0f)
					maxW = 256.0f;
				if (w > 2.0f && QString(iProcessor.imgdata.idata.make).compare("Canon", Qt::CaseInsensitive) == 0)
					maxW = 512.0f;	// some cameras would even need ~800 - why?

				//if (maxW < mulWhite[1])
				//	maxW = mulWhite[1];
				//if (maxW < mulWhite[2])
				//	maxW = mulWhite[2];
				//if (maxW < mulWhite[3])
				//	maxW = mulWhite[3];
				
				mulWhite[0] /= maxW;
				mulWhite[1] /= maxW;
				mulWhite[2] /= maxW;
				mulWhite[3] /= maxW;

				//if (iProcessor.imgdata.color.cmatrix[0][0] != 0) {
				//	mulWhite[0] = iProcessor.imgdata.color.cmatrix[0][0];
				//	mulWhite[1] = iProcessor.imgdata.color.cmatrix[0][1];
				//	mulWhite[2] = iProcessor.imgdata.color.cmatrix[0][2];
				//	mulWhite[3] = iProcessor.imgdata.color.cmatrix[0][3];
				//}

				//if (iProcessor.imgdata.color.rgb_cam[0][0] != 0) {
				//	mulWhite[0] = iProcessor.imgdata.color.rgb_cam[0][0];
				//	mulWhite[1] = iProcessor.imgdata.color.rgb_cam[1][1];
				//	mulWhite[2] = iProcessor.imgdata.color.rgb_cam[2][2];
				//	mulWhite[3] = iProcessor.imgdata.color.rgb_cam[1][1];
				//}
				//if (iProcessor.imgdata.color.cam_xyz[0][0] != 0) {
				//	mulWhite[0] = iProcessor.imgdata.color.cam_xyz[0][0];
				//	mulWhite[1] = iProcessor.imgdata.color.cam_xyz[1][1];
				//	mulWhite[2] = iProcessor.imgdata.color.cam_xyz[2][2];
				//	mulWhite[3] = iProcessor.imgdata.color.cam_xyz[1][1];
				//}


				if (mulWhite[3] == 0)
					mulWhite[3] = mulWhite[1];



				////DkUtils::printDebug(DK_MODULE, "----------------\n", (float)iProcessor.imgdata.color.maximum);
				////DkUtils::printDebug(DK_MODULE, "Bayer Pattern: %s\n", iProcessor.imgdata.idata.cdesc);
				////DkUtils::printDebug(DK_MODULE, "Camera manufacturer: %s\n", iProcessor.imgdata.idata.make);
				////DkUtils::printDebug(DK_MODULE, "Camera model: %s\n", iProcessor.imgdata.idata.model);
				////DkUtils::printDebug(DK_MODULE, "canon_ev %f\n", (float)iProcessor.imgdata.color.canon_ev);

				////DkUtils::printDebug(DK_MODULE, "white: [%.3f %.3f %.3f %.3f]\n", iProcessor.imgdata.color.cam_mul[0],
				////	iProcessor.imgdata.color.cam_mul[1], iProcessor.imgdata.color.cam_mul[2],
				////	iProcessor.imgdata.color.cam_mul[3]);
				////DkUtils::printDebug(DK_MODULE, "white (processing): [%.3f %.3f %.3f %.3f]\n", mulWhite[0],
				////	mulWhite[1], mulWhite[2],
				////	mulWhite[3]);
				////DkUtils::printDebug(DK_MODULE, "black: %i\n", iProcessor.imgdata.color.black);
				////DkUtils::printDebug(DK_MODULE, "maximum: %.i %i\n", iProcessor.imgdata.color.maximum,
				////	iProcessor.imgdata.params.adjust_maximum_thr);
				////DkUtils::printDebug(DK_MODULE, "----------------\n", (float)iProcessor.imgdata.color.maximum);



				float gamma = (float)iProcessor.imgdata.params.gamm[0];///(float)iProcessor.imgdata.params.gamm[1];
				float gammaTable[65536];
				for (int i = 0; i < 65536; i++) {
					gammaTable[i] = (float)(1.099f*pow((float)i/65535.0f, gamma)-0.099f);
				}


				for (uint row = 0; row < rows; row++)
				{
					float *ptrRaw = rawMat.ptr<float>(row);

					for (uint col = 0; col < cols; col++)
					{
						 int colorIdx = iProcessor.COLOR(row, col);
						 ptrRaw[col] = (float)(iProcessor.imgdata.image[cols*(row) + col][colorIdx]);
						 //ptrRaw[col] = (float)iProcessor.imgdata.color.curve[(int)ptrRaw[col]];
						 
						 ptrRaw[col] -= iProcessor.imgdata.color.black;
						 ptrRaw[col] /= dynamicRange;
						 
						 //// clip
						 //if (ptrRaw[col] > 1.0f) ptrRaw[col] = 1.0f;
						 //if (ptrRaw[col] < 0.0f) ptrRaw[col] = 0.0f;
						 

						 //if (ptrRaw[col] <= 1.0f)
						ptrRaw[col] *= mulWhite[colorIdx];
						ptrRaw[col] = ptrRaw[col] > 1.0f ? 1.0f : ptrRaw[col]; 
						//ptrRaw[col] = (float)(pow((float)ptrRaw[col], gamma));
						//ptrRaw[col] *= 255.0f;		
						
						ptrRaw[col] = ptrRaw[col] <= 0.018f ? (ptrRaw[col]*(float)iProcessor.imgdata.params.gamm[1]) *255.0f :
							 									gammaTable[(int)(ptrRaw[col]*65535.0f)]*255;
							//									(1.099f*(float)(pow((float)ptrRaw[col], gamma))-0.099f)*255.0f;
						 //ptrRaw[col] *= mulWhite[colorIdx];
						 //ptrRaw[col] = ptrRaw[col] > 255.0f ? 255.0f : ptrRaw[col];
						 //ptrRaw[col] *=255.0f;
					}
				}

				//Mat cropMat(rawMat, Rect(1, 1, rawMat.cols-1, rawMat.rows-1));
				//rawMat.release();
				//rawMat = cropMat;
				//rawMat.setTo(0);

				//normalize(rawMat, rawMat, 255, 0, NORM_MINMAX);

				rawMat.convertTo(rawMat,CV_8U);
				//cvtColor(rawMat, rgbImg, CV_BayerBG2RGB);
				unsigned long type = (unsigned long)iProcessor.imgdata.idata.filters;
				type = type & 255;

				if (type == 180) cvtColor(rawMat, rgbImg, CV_BayerBG2RGB);      //bitmask  10 11 01 00  -> 3(G) 2(B) 1(G) 0(R) -> RG RG RG
				//												                                                                  GB GB GB
				else if (type == 30) cvtColor(rawMat, rgbImg, CV_BayerRG2RGB);		//bitmask  00 01 11 10	-> 0 1 3 2
				else if (type == 225) cvtColor(rawMat, rgbImg, CV_BayerGB2RGB);		//bitmask  11 10 00 01
				else if (type == 75) cvtColor(rawMat, rgbImg, CV_BayerGR2RGB);		//bitmask  01 00 10 11
				else throw DkException("Wrong Bayer Pattern (not BG, RG, GB, GR)\n", __LINE__, __FILE__);

				if (iProcessor.imgdata.sizes.pixel_aspect != 1.0f) {
					resize(rgbImg, rawMat, Size(), (double)iProcessor.imgdata.sizes.pixel_aspect, 1.0f);
					rgbImg = rawMat;
				}
				image = QImage(rgbImg.data, rgbImg.cols, rgbImg.rows, rgbImg.step/*rgbImg.cols*3*/, QImage::Format_RGB888);

				//orientation is done in loadGeneral with libExiv
				//orientation = iProcessor.imgdata.sizes.flip;
				//switch (orientation) {
				//case 0: orientation = 0; break;
				//case 3: orientation = 180; break;
				//case 5:	orientation = -90; break;
				//case 6: orientation = 90; break;
				//}
			}

			img = image.copy();
			//if (orientation!=0) {
			//	QTransform rotationMatrix;
			//	rotationMatrix.rotate((double)orientation);
			//	img = img.transformed(rotationMatrix);
			//}
			imgLoaded = true;

			iProcessor.recycle();

#else
	throw DkException("Not compiled using OpenCV - could not load any RAW image", __LINE__, __FILE__);
	qDebug() << "Not compiled using OpenCV - could not load any RAW image";
#endif
		} catch (...) {
			qWarning() << "failed to load raw image...";
		}

	}

	return imgLoaded;
}

void DkImageLoader::saveFile(QFileInfo file, QString fileFilter, QImage saveImg, int compression) {

		QMetaObject::invokeMethod(this, "saveFileIntern", 
			Qt::QueuedConnection, 
			Q_ARG(QFileInfo, file), 
			Q_ARG(QString, fileFilter), 
			Q_ARG(QImage, saveImg),
			Q_ARG(int, compression));
}

void DkImageLoader::saveTempFile(QImage img) {

	QFileInfo tmpPath = QFileInfo(DkSettings::GlobalSettings::tmpPath + "\\");
	
	if (!DkSettings::GlobalSettings::useTmpPath || !tmpPath.exists()) {
		qDebug() << tmpPath.absolutePath() << "does not exist";
		return;
	}

	qDebug() << "tmpPath: " << tmpPath.absolutePath();
	
	// TODO: let user set filename + extension?
	QString fileExt = ".png";

	// TODO: call save file silent threaded...
	for (int idx = 1; idx < 10000; idx++) {
	
		QString fileName = "img";

		if (idx < 10)
			fileName += "000";
		else if (idx < 100)
			fileName += "00";
		else if (idx < 1000)
			fileName += "0";
		
		fileName += QString::number(idx) + fileExt;

		QFileInfo tmpFile = QFileInfo(tmpPath.absolutePath(), fileName);

		if (!tmpFile.exists()) {
			saveFileSilentThreaded(tmpFile, img);

			//this->virtualFile = tmpFile;	// why doesn't it work out -> file does not exist (locked?)
			//setImage(img);

			emit updateFileSignal(tmpFile, img.size());
			
			//if (updateFolder) {
			//	emit updateDirSignal(tmpFile);
			//	this->file = tmpFile;
			//	loadDir(tmpFile.absoluteDir());
			//}
			
			qDebug() << tmpFile.absoluteFilePath() << "saved...";
			break;
		}
	}

}

void DkImageLoader::saveFileIntern(QFileInfo file, QString fileFilter, QImage saveImg, int compression) {
	
	QMutexLocker locker(&mutex);
	
	if (img.isNull() && saveImg.isNull()) {
		QString msg = "I can't save an empty file, sorry...\n";
		emit newErrorDialog(msg);
		return;
	}
	if (!file.absoluteDir().exists()) {
		QString msg = QString("Sorry, the directory: ") % file.absolutePath() % QString(" does not exist\n");
		emit newErrorDialog(msg);
		return;
	}
	if (file.exists() && !file.isWritable()) {
		QString msg = QString("Sorry, I can't write to the file: ") % file.fileName();
		emit newErrorDialog(msg);
		return;
	}

	QString filePath = file.absoluteFilePath();

	// if the user did not specify the suffix - append the suffix of the file filter
	QString newSuffix = file.suffix();
	if (newSuffix == "") {
		
		newSuffix = fileFilter.remove(0, fileFilter.indexOf("."));
		printf("new suffix: %s\n", newSuffix.toStdString().c_str());

		int endSuffix = -1;
		if (newSuffix.indexOf(")") == -1)
			endSuffix =  newSuffix.indexOf(" ");
		else if (newSuffix.indexOf(" ") == -1)
			endSuffix =  newSuffix.indexOf(")");
		else
			endSuffix = min(newSuffix.indexOf(")"), newSuffix.indexOf(" "));

		filePath.append(newSuffix.left(endSuffix));
	}
	
	// update watcher
	if (this->file.exists() && watcher)
		watcher->removePath(this->file.absoluteFilePath());
	
	QImage sImg = (saveImg.isNull()) ? img : saveImg;
		
	emit updateInfoSignal("saving...", -1);
	QImageWriter* imgWriter = new QImageWriter(filePath);
	imgWriter->setCompression(compression);
	imgWriter->setQuality(compression);
	bool saved = imgWriter->write(sImg);
	delete imgWriter;
	//bool saved = sImg.save(filePath, 0, compression);
	emit updateInfoSignal("saving...", 1);
	qDebug() << "jpg compression: " << compression;

	if (saved) {
		
		try {
			dataExif.saveMetaDataToFile(QFileInfo(filePath)/*, dataExif.getOrientation()*/);
		} catch (...) {
			qDebug() << "could not copy meta-data to file" << filePath;
		}

		// assign the new save directory
		saveDir = QDir(file.absoluteDir());
		DkSettings::GlobalSettings::lastSaveDir = file.absolutePath();

		DkSettings s;
		s.save();
				
		// reload my dir (if it was changed...)
		this->file = QFileInfo(filePath);
		this->virtualFile = file;
		this->img = sImg;
		loadDir(file.absoluteDir());

		emit updateImageSignal(this->img);
		emit updateFileSignal(file, img.size());

		printf("I could save the image...\n");
	}
	else {
		QString msg = QString("Sorry, I can't write to the file: ") % file.fileName();
		emit newErrorDialog(msg);
	}

	if (watcher) watcher->addPath(file.absoluteFilePath());

}

void DkImageLoader::saveFileSilentThreaded(QFileInfo file, QImage img) {

	QMetaObject::invokeMethod(this, "saveFileSilentIntern", Qt::QueuedConnection, Q_ARG(QFileInfo, file), Q_ARG(QImage, img));
}

void DkImageLoader::saveFileSilentIntern(QFileInfo file, QImage saveImg) {

	QMutexLocker locker(&mutex);
	
	// update watcher
	if (this->file.exists() && watcher)
		watcher->removePath(this->file.absoluteFilePath());
	
	emit updateInfoSignal("saving...", -1);
	QString filePath = file.absoluteFilePath();
	bool saved = (saveImg.isNull()) ? img.save(filePath) : saveImg.save(filePath);
	emit updateInfoSignal("saving...", 1);	// stop the label
	
	if (saved && watcher)
		watcher->addPath(file.absoluteFilePath());
	else if (watcher)
		watcher->addPath(this->file.absoluteFilePath());
}

void DkImageLoader::saveRating(int rating) {

	QMutexLocker locker(&mutex);

	try {
		dataExif.setRating(rating);
	}catch(...) {
		emit updateInfoSignal("Sorry, I destroyed your file...");
	}
}

void DkImageLoader::enableWatcher(bool enable) {
	
	watcherEnabled = enable;
}

void DkImageLoader::updateHistory() {

	DkSettings::GlobalSettings::lastDir = file.absolutePath();

	DkSettings::GlobalSettings::recentFiles.removeAll(file.absoluteFilePath());
	DkSettings::GlobalSettings::recentFolders.removeAll(file.absolutePath());

	DkSettings::GlobalSettings::recentFiles.push_front(file.absoluteFilePath());
	DkSettings::GlobalSettings::recentFolders.push_front(file.absolutePath());

	DkSettings::GlobalSettings::recentFiles.removeDuplicates();
	DkSettings::GlobalSettings::recentFolders.removeDuplicates();

	for (int idx = 0; idx < DkSettings::GlobalSettings::recentFiles.size()-20; idx++)
		DkSettings::GlobalSettings::recentFiles.pop_back();

	for (int idx = 0; idx < DkSettings::GlobalSettings::recentFiles.size()-20; idx++)
		DkSettings::GlobalSettings::recentFiles.pop_back();

	DkSettings s = DkSettings();
	s.save();
}

// image manipulation --------------------------------------------------------------------
void DkImageLoader::deleteFile() {

	if (file.exists()) {

		QFile* fileHandle = new QFile(file.absoluteFilePath());

		//if (fileHandle->permissions() != QFile::WriteUser) {		// on unix this may lead to troubles (see qt doc)
		//	emit updateInfoSignal("You don't have permissions to delete: \n" % file.fileName());
		//	return;
		//}

		QFileInfo fileToDelete = file;

		// load the next file
		QFileInfo loadFile = getChangedFileInfo(1, true);

		if (loadFile.exists())
			load(loadFile, true, true);
		else {
			clearPath();
			emit updateImageSignal(QImage());
		}
		
		if (fileHandle->remove())
			emit updateInfoSignal(fileToDelete.fileName() % " deleted...", 5000);
		else
			emit updateInfoSignal("Sorry, I could not delete: " % fileToDelete.fileName());
	}

}

void DkImageLoader::rotateImage(double angle) {

	if (img.isNull())
		return;

	if (file.exists() && watcher)
		watcher->removePath(this->file.absoluteFilePath());

	try {
		
		mutex.lock();
		QTransform rotationMatrix;
		rotationMatrix.rotate(angle);

		img = img.transformed(rotationMatrix);
		mutex.unlock();

		updateImageSignal(img);
		updateFileSignal(file, img.size());
		
		mutex.lock();
		
		updateInfoSignal("saving...", -1);
		dataExif.saveOrientation((int)angle);
		updateInfoSignal("saving...", 1);
		qDebug() << "exif data saved (rotation)?";
		mutex.unlock();

	}
	catch(DkException de) {

		mutex.unlock();
		updateInfoSignal("saving...", 1);

		// make a silent save -> if the image is just cached, do not save it
		if (file.exists())
			saveFileSilentThreaded(file);
	}
	catch(...) {	// if file is locked... or permission is missing
		mutex.unlock();
		QString msg = "Sorry, I could not write to: " % file.fileName();
		updateInfoSignal(msg);
	}

	if (watcher) watcher->addPath(this->file.absoluteFilePath());

}

void DkImageLoader::fileChanged(const QString& path) {

	// ignore if watcher was disabled
	if (path == file.absoluteFilePath() && watcherEnabled) {
		QMutexLocker locker(&mutex);
		load(QFileInfo(path), false, true);
	}
}

bool DkImageLoader::hasFile() {

	return file.exists();
}

QFileInfo DkImageLoader::getFile() {

	return file;
}

QDir DkImageLoader::getDir() {

	return dir;
}

QStringList DkImageLoader::getFilteredFileList(QDir dir, QStringList ignoreKeywords, QStringList keywords) {

	dir.setSorting(QDir::LocaleAware);
	QStringList fileList = dir.entryList(fileFilters);

	for (int idx = 0; idx < ignoreKeywords.size(); idx++) {
		QRegExp exp = QRegExp("^((?!" + ignoreKeywords[idx] + ").)*$");
		exp.setCaseSensitivity(Qt::CaseInsensitive);
		fileList = fileList.filter(exp);
	}

	for (int idx = 0; idx < keywords.size(); idx++) {
		fileList = fileList.filter(keywords[idx], Qt::CaseInsensitive);
	}

	return fileList;
}

QDir DkImageLoader::getSaveDir() {

	if (!saveDir.exists())
		return dir;
	else
		return saveDir;
}

QString DkImageLoader::getCurrentFilter() {

	QString cSuffix = file.suffix();

	for (int idx = 0; idx < saveFilters.size(); idx++) {

		if (saveFilters[idx].contains(cSuffix))
			return saveFilters[idx];
	}

	return "";
}

bool DkImageLoader::isValid(QFileInfo& fileInfo) {

	if (!fileInfo.exists())
		return false;

	printf("accepting file...\n");

	QString fileName = fileInfo.fileName();
	for (int idx = 0; idx < fileFilters.size(); idx++) {

		QRegExp exp = QRegExp(fileFilters.at(idx), Qt::CaseInsensitive);
		exp.setPatternSyntax(QRegExp::Wildcard);
		if (exp.exactMatch(fileName))
			return true;
	}

	printf("I did not accept... honestly...\n");

	return false;

}

int DkImageLoader::locateFile(QFileInfo& fileInfo, QDir* dir) {

	if (!fileInfo.exists())
		return -1;

	bool newDir = (dir) ? false : true;

	if (!dir) {
		dir = new QDir(fileInfo.absoluteDir());
		dir->setNameFilters(fileFilters);
		dir->setSorting(QDir::LocaleAware);
	}

	// locate the current file
	QStringList files = dir->entryList(fileFilters);
	QString cFilename = fileInfo.fileName();

	int fileIdx = 0;
	for ( ; fileIdx < files.size(); fileIdx++) {

		if (files[fileIdx] == cFilename)
			break;
	}

	if (fileIdx == files.size()) fileIdx = -1;

	if (newDir)
		delete dir;

	return fileIdx;
}

void DkImageLoader::setFile(QFileInfo& file) {
	
	QDir oldDir = this->file.absoluteDir();
	
	this->file = file;
	this->virtualFile = file;
	loadDir(file.absoluteDir());

}

void DkImageLoader::setDir(QDir& dir) {

	QDir oldDir = file.absoluteDir();

	// locate the current file
	QStringList files = getFilteredFileList(dir, ignoreKeywords, keywords);
	
	if (files.empty()) {
		emit updateInfoSignal(dir.absolutePath() % "\n does not contain any image", 4000);	// stop showing
		return;
	}

	loadDir(dir);

	firstFile();
}

void DkImageLoader::setSaveDir(QDir& dir) {
	this->saveDir = dir;
}

void DkImageLoader::setImage(QImage& img) {
	
	this->img = img;
}

QString DkImageLoader::fileName() {
	return file.fileName();
}

// DkThumbsLoader --------------------------------------------------------------------

DkThumbsLoader::DkThumbsLoader(std::vector<DkThumbNail>* thumbs, QDir dir) {

	this->thumbs = thumbs;
	this->dir = dir;
	this->isActive = true;
	this->maxThumbSize = DkSettings::DisplaySettings::thumbSize;
	init();
}

void DkThumbsLoader::init() {

	QStringList files = DkImageLoader::getFilteredFileList(dir);
	startIdx = -1;
	endIdx = -1;
	somethingTodo = false;
	
	DkTimer dt;
	for (int idx = 0; idx < files.size(); idx++) {
		QFileInfo cFile = QFileInfo(dir, files[idx]);
		thumbs->push_back(DkThumbNail(cFile));
	}
	qDebug() << "thumb stubs loaded in: " << QString::fromStdString(dt.getTotal());
}

void DkThumbsLoader::run() {

	if (!thumbs)
		return;

	while (true) {

		mutex.lock();
		DkTimer dt;
		usleep(10000);

		//QMutexLocker(&this->mutex);
		if (!isActive) {
			qDebug() << "thumbs loader stopped...";
			mutex.unlock();
			break;
		}
		mutex.unlock();

		if (somethingTodo)
			loadThumbs();
	}

	//// locate the current file
	//QStringList files = dir.entryList(DkImageLoader::fileFilters);

	//DkTimer dtt;

	//for (int idx = 0; idx < files.size(); idx++) {

	//	QMutexLocker(&this->mutex);
	//	if (!isActive) {
	//		break;
	//	}

	//	QFileInfo cFile = QFileInfo(dir, files[idx]);

	//	if (!cFile.exists() || !cFile.isReadable())
	//		continue;

	//	QImage img = getThumbNailQt(cFile);
	//	//QImage img = getThumbNailWin(cFile);
	//	thumbs->push_back(DkThumbNail(cFile, img));
	//}

}

void DkThumbsLoader::loadThumbs() {


	std::vector<DkThumbNail>::iterator thumbIter = thumbs->begin()+startIdx;

	for (int idx = startIdx; idx < endIdx; idx++, thumbIter++) {

		mutex.lock();
		
		// does somebody want me to stop?
		if (!isActive) {
			mutex.unlock();
			return;
		}
		
		DkThumbNail* thumb = &(*thumbIter);
		if (!thumb->hasImage()) {
			thumb->setImage(getThumbNailQt(thumb->getFile()));
			if (thumb->hasImage())	// could I load the thumb?
				emit updateSignal();
			else
				thumb->setImgExists(false);
		}
		mutex.unlock();
	}

	somethingTodo = false;
}

void DkThumbsLoader::setLoadLimits(int start, int end) {

	//QMutexLocker(&this->mutex);
	//if (start < startIdx || startIdx == -1)	startIdx = (start >= 0 && start < thumbs->size()) ? start : 0;
	//if (end > endIdx || endIdx == -1)		endIdx = (end > 0 && end < thumbs->size()) ? end : thumbs->size();
	startIdx = (start >= 0 && (unsigned int) start < thumbs->size()) ? start : 0;
	endIdx = (end > 0 && (unsigned int) end < thumbs->size()) ? end : thumbs->size();

	somethingTodo = true;
}

//QImage DkThumbsLoader::getThumbNailWin(QFileInfo file) {
//
//	CoInitialize(NULL);
//
//	DkTimer dt;
//
//	QImage thumb;
//
//	// allocate some unmanaged memory for our strings and divide the file name
//	// into a folder path and file name.
//	//String* fileName = file.absoluteFilePath();
//	//IntPtr dirPtr = Marshal::StringToHGlobalUni(Path::GetDirectoryName(fileName));
//	//IntPtr filePtr = Marshal::StringToHGlobalUni(Path::GetFileName(fileName));
//
//	QString winPath = QDir::toNativeSeparators(file.absolutePath());
//	QString winFile = QDir::toNativeSeparators(file.fileName());
//	winPath.append("\\");	
//
//	WCHAR* wDirName = new WCHAR[winPath.length()];
//	WCHAR* wFileName = new WCHAR[winFile.length()];
//
//	int dirLength = winPath.toWCharArray(wDirName);
//	int fileLength = winFile.toWCharArray(wFileName);
//
//	wDirName[dirLength] = L'\0';
//	wFileName[fileLength] = L'\0';
//
//	IShellFolder* pDesktop = NULL;
//	IShellFolder* pSub = NULL;
//	IExtractImage* pIExtract = NULL;
//	LPITEMIDLIST pList = NULL;
//
//	// get the desktop directory
//	if (SUCCEEDED(SHGetDesktopFolder(&pDesktop)))
//	{   
//		// get the pidl for the directory
//		HRESULT hr = pDesktop->ParseDisplayName(NULL, NULL, wDirName, NULL, &pList, NULL);
//		if (FAILED(hr)) {
//			//throw new Exception(S"Failed to parse the directory name");
//
//			return thumb;
//		}
//
//		// get the directory IShellFolder interface
//		hr = pDesktop->BindToObject(pList, NULL, IID_IShellFolder, (void**)&pSub);
//		if (FAILED(hr))	{
//			//throw new Exception(S"Failed to bind to the directory");
//			return thumb;
//		}
//
//		// get the file's pidl
//		hr = pSub->ParseDisplayName(NULL, NULL, wFileName, NULL, &pList, NULL);
//		if (FAILED(hr))	{
//			//throw new Exception(S"Failed to parse the file name");
//			return thumb;
//		}
//
//		// get the IExtractImage interface
//		LPCITEMIDLIST pidl = pList;
//		hr = pSub->GetUIObjectOf(NULL, 1, &pidl, IID_IExtractImage,
//			NULL, (void**)&pIExtract);
//
//		// set our desired image size
//		SIZE size;
//		size.cx = maxThumbSize;
//		size.cy = maxThumbSize;      
//
//		if(pIExtract == NULL) {
//			return thumb;
//		}         
//
//		HBITMAP hBmp = NULL;
//
//		// The IEIFLAG_ORIGSIZE flag tells it to use the original aspect
//		// ratio for the image size. The IEIFLAG_QUALITY flag tells the 
//		// interface we want the image to be the best possible quality.
//		DWORD dwFlags = IEIFLAG_ORIGSIZE | IEIFLAG_QUALITY;      
//
//		OLECHAR pathBuffer[MAX_PATH];
//		hr = pIExtract->GetLocation(pathBuffer, MAX_PATH, NULL, &size, 4, &dwFlags);         // TODO: color depth!! (1)
//		if (FAILED(hr)) {
//			//throw new Exception(S"The call to GetLocation failed");
//			return thumb;
//		}
//
//		hr = pIExtract->Extract(&hBmp);
//
//		// It is possible for Extract to fail if there is no thumbnail image
//		// so we won't check for success here
//
//		pIExtract->Release();
//
//		if (hBmp != NULL) {
//			thumb = QPixmap::fromWinHBITMAP(hBmp, QPixmap::Alpha).toImage();
//		}      
//	}
//
//	// Release the COM objects we have a reference to.
//	pDesktop->Release();
//	pSub->Release(); 
//
//	// delete the unmanaged memory we allocated
//	//Marshal::FreeCoTaskMem(dirPtr);
//	//Marshal::FreeCoTaskMem(filePtr);
//	//delete[] wDirName;
//	//delete[] wFileName;
//
//
//	return thumb;
//}

QImage DkThumbsLoader::getThumbNailQt(QFileInfo file) {

	
	DkTimer dt;

	//// see if we can read the thumbnail from the exif data
	DkMetaData dataExif(file);
	QImage thumb = dataExif.getThumbnail();
	int orientation = dataExif.getOrientation();
	int imgW = thumb.width();
	int imgH = thumb.height();
	int tS = DkSettings::DisplaySettings::thumbSize;

	// as found at: http://olliwang.com/2010/01/30/creating-thumbnail-images-in-qt/
	QImageReader imageReader(file.absoluteFilePath());

	if (thumb.isNull() || thumb.width() < tS && thumb.height() < tS) {

		imgW = imageReader.size().width();
		imgH = imageReader.size().height();	// locks the file!
	}
	else if (!thumb.isNull())
		qDebug() << "EXIV thumb loaded: " << thumb.width() << " x " << thumb.height();
	
	if (imgW > maxThumbSize || imgH > maxThumbSize) {
		if (imgW > imgH) {
			imgH = (float)maxThumbSize / imgW * imgH;
			imgW = maxThumbSize;
		} 
		else if (imgW < imgH) {
			imgW = (float)maxThumbSize / imgH * imgW;
			imgH = maxThumbSize;
		}
		else {
			imgW = maxThumbSize;
			imgH = maxThumbSize;
		}
	}

	if (thumb.isNull() || thumb.width() < tS && thumb.height() < tS) {
		// flip size if the image is rotated by 90�
		if (dataExif.isTiff() && abs(orientation) == 90) {
			int tmpW = imgW;
			imgW = imgH;
			imgH = tmpW;
		}

		QSize initialSize = imageReader.size();

		imageReader.setScaledSize(QSize(imgW, imgH));
		thumb = imageReader.read();

		// is there a nice solution to do so??
		imageReader.setFileName("josef");	// image reader locks the file -> but there should not be one so we just set it to another file...

		// there seems to be a bug in exiv2
		if ((initialSize.width() > 400 || initialSize.height() > 400) && DkSettings::DisplaySettings::saveThumb)	// TODO settings
			dataExif.saveThumbnail(thumb);
	}
	else {
		thumb = thumb.scaled(QSize(imgW, imgH), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
		qDebug() << "thumb loaded from exif...";
	}

	if (orientation != -1 && !dataExif.isTiff()) {
		QTransform rotationMatrix;
		rotationMatrix.rotate((double)orientation);
		thumb = thumb.transformed(rotationMatrix);
	}

	qDebug() << "[thumb] " << file.fileName() << " loaded in: " << QString::fromStdString(dt.getTotal());

	return thumb;
}

void DkThumbsLoader::stop() {
	
	//QMutexLocker(&this->mutex);
	isActive = false;
	qDebug() << "stopping thread: " << this->thread()->currentThreadId();
}

DkMetaData::DkMetaData(const DkMetaData& metaData) {

	//const Exiv2::Image::AutoPtr exifImg((metaData.exifImg));
	this->file = metaData.file;
	this->mdata = false;
	// TODO: not too cool...

}

int DkMetaData::getOrientation() {
	readMetaData();

	if (!mdata)
		return -1;

	int orientation;
		
	Exiv2::ExifData &exifData = exifImg->exifData();


	if (exifData.empty()) {
		orientation = -1;
	} else {

		Exiv2::ExifKey key = Exiv2::ExifKey("Exif.Image.Orientation");
		Exiv2::ExifData::iterator pos = exifData.findKey(key);

		 if (pos == exifData.end() || pos->count() == 0) {
			 qDebug() << "Orientation is not set in the Exif Data";
			 orientation = -1;
		 } else {
			Exiv2::Value::AutoPtr v = pos->getValue();

			orientation = (int)pos->toFloat();

			//Exiv2::UShortValue* prv = dynamic_cast<Exiv2::UShortValue*>(v.release());
			//Exiv2::UShortValue::AutoPtr rv = Exiv2::UShortValue::AutoPtr(prv);
			//orientation = (int)rv->value_[0];

			switch (orientation) {
			case 6: orientation = 90;
				break;
			case 7: orientation = 90;
				break;
			case 3: orientation = 180;
				break;
			case 4: orientation = 180;
				break;
			case 8: orientation = -90;
				break;
			case 5: orientation = -90;
				break;
			default: orientation = 0;
				break;
			}	
		}
	}

	 return orientation;
}

QImage DkMetaData::getThumbnail() {

	readMetaData();

	if (!mdata)
		return QImage();

	Exiv2::ExifData &exifData = exifImg->exifData();

	if (exifData.empty())
		return QImage();

	QImage qThumb;
	try {

		Exiv2::ExifThumb thumb(exifData);
		Exiv2::DataBuf buffer = thumb.copy();
		// ok, get the buffer...
		std::pair<Exiv2::byte*, long> stdBuf = buffer.release();
		QByteArray ba = QByteArray((char*)stdBuf.first, (int)stdBuf.second);
		qThumb.loadFromData(ba);
		//qDebug() << "thumbs size: " << qThumb.size();
	}
	catch (...) {
		qDebug() << "Sorry, I could not load the thumb from the exif data...";
	}
		
	return qThumb;
}

void DkMetaData::saveThumbnail(QImage thumb) {

	readMetaData();	
	
	if (!mdata)
		return;

	Exiv2::ExifData exifData = exifImg->exifData();

	if (exifData.empty())
		exifData = Exiv2::ExifData();

	// ok, let's try to save the thumbnail...
	try {
		//Exiv2::ExifThumb eThumb(exifData);
		//eThumb.setJpegThumbnail((byte*)thumb.bits(), (long)thumb.bitPlaneCount());

		Exiv2::ExifThumb eThumb(exifData);

		if (isTiff()) {
			eThumb.erase();

			Exiv2::ExifData::const_iterator pos = exifData.findKey(Exiv2::ExifKey("Exif.Image.NewSubfileType"));
			if (pos == exifData.end() || pos->count() != 1 || pos->toLong() != 0) {
				 throw DkException("Exif.Image.NewSubfileType missing or not set as main image", __LINE__, __FILE__);
			 }
			 // Remove sub-IFD tags
			 std::string subImage1("SubImage1");
			 for (Exiv2::ExifData::iterator md = exifData.begin(); md != exifData.end();)
			 {
				 if (md->groupName() == subImage1)
					 md = exifData.erase(md);
				 else
					 ++md;
			 }
		}

		QByteArray data;
		QBuffer buffer(&data);
		buffer.open(QIODevice::WriteOnly);
		thumb.save(&buffer, "JPEG");

		if (isTiff()) {
			Exiv2::DataBuf buf((Exiv2::byte *)data.data(), data.size());
			Exiv2::ULongValue val;
			val.read("0");
			val.setDataArea(buf.pData_, buf.size_);
			exifData["Exif.SubImage1.JPEGInterchangeFormat"] = val;
			exifData["Exif.SubImage1.JPEGInterchangeFormatLength"] = uint32_t(buf.size_);
			exifData["Exif.SubImage1.Compression"] = uint16_t(6); // JPEG (old-style)
			exifData["Exif.SubImage1.NewSubfileType"] = uint32_t(1); // Thumbnail image
			qDebug() << "As you told me to, I am writing the tiff thumbs...";

		} else {
			eThumb.setJpegThumbnail((Exiv2::byte *)data.data(), data.size());
			qDebug() << "As you told me to, I am writing the thumbs...";
		}

		exifImg->setExifData(exifData);
		exifImg->writeMetadata();

		//Exiv2::Image::AutoPtr exifImgN;
		//
		//exifImgN = Exiv2::ImageFactory::open(QFileInfo("C:/img.tif").absoluteFilePath().toStdString());
		//exifImgN->readMetadata();
		//exifImgN->setExifData(exifData);
		//exifImgN->writeMetadata();



	} catch (...) {
		qDebug() << "I could not save the thumbnail...\n";
	}
}

QStringList DkMetaData::getExifKeys() {
		
	QStringList exifKeys;

	Exiv2::ExifData &exifData = exifImg->exifData();
	Exiv2::ExifData::const_iterator end = exifData.end();

	if (exifData.empty()) {
		return exifKeys;
		
	} else {
	
		for (Exiv2::ExifData::const_iterator i = exifData.begin(); i != end; ++i) {

			std::string tmp = i->key();
			exifKeys << QString(tmp.c_str());

			qDebug() << QString::fromStdString(tmp);

		}
	}


	return exifKeys;
}

QStringList DkMetaData::getExifValues() {

	QStringList exifValues;

	Exiv2::ExifData &exifData = exifImg->exifData();
	Exiv2::ExifData::const_iterator end = exifData.end();

	if (exifData.empty()) {
		return exifValues;

	} else {

		for (Exiv2::ExifData::const_iterator i = exifData.begin(); i != end; ++i) {

			std::string tmp = i->value().toString();
			exifValues << QString(tmp.c_str());
		}
	}


	return exifValues;
}

QStringList DkMetaData::getIptcKeys() {

	QStringList iptcKeys;

	Exiv2::IptcData &iptcData = exifImg->iptcData();
	Exiv2::IptcData::iterator endI = iptcData.end();

	if (iptcData.empty()) {
		qDebug() << "iptc data is empty";

		return iptcKeys;

	} else {
		for (Exiv2::IptcData::iterator md = iptcData.begin(); md != endI; ++md) {
			
			std::string tmp = md->key();
			iptcKeys << QString(tmp.c_str());

			qDebug() << QString::fromStdString(tmp);
		}
	}

	return iptcKeys;
}

QStringList DkMetaData::getIptcValues() {
	QStringList iptcValues;

	Exiv2::IptcData &iptcData = exifImg->iptcData();
	Exiv2::IptcData::iterator endI = iptcData.end();

	if (iptcData.empty()) {
		return iptcValues;
	} else {
		for (Exiv2::IptcData::iterator md = iptcData.begin(); md != endI; ++md) {

			std::string tmp = md->value().toString();
			iptcValues << QString(tmp.c_str());

		}
	}

	return iptcValues;
}

std::string DkMetaData::getNativeExifValue(std::string key) {
	std::string info = "";

	readMetaData();
	if (!mdata)
		return info;

	Exiv2::ExifData &exifData = exifImg->exifData();

	if (!exifData.empty()) {

		Exiv2::ExifData::iterator pos;

		try {
			Exiv2::ExifKey ekey = Exiv2::ExifKey(key);
			pos = exifData.findKey(ekey);


		} catch(...) {
			return "";
		}

		if (pos == exifData.end() || pos->count() == 0) {
			qDebug() << "Information is not set in the Exif Data";
		} else {
			Exiv2::Value::AutoPtr v = pos->getValue();
			info = pos->toString();
		}
	}

	return info;
}

std::string DkMetaData::getExifValue(std::string key) {
	
	std::string info = "";

	readMetaData();
	if (!mdata)
		return info;

	Exiv2::ExifData &exifData = exifImg->exifData();

	if (!exifData.empty()) {

		Exiv2::ExifData::iterator pos;

		try {
			Exiv2::ExifKey ekey = Exiv2::ExifKey("Exif.Image." + key);
			pos = exifData.findKey(ekey);

			if (pos == exifData.end() || pos->count() == 0) {
				Exiv2::ExifKey ekey = Exiv2::ExifKey("Exif.Photo." + key);	
				pos = exifData.findKey(ekey);
			}
		} catch(...) {
			try {
			key = "Exif.Photo." + key;
			Exiv2::ExifKey ekey = Exiv2::ExifKey(key);	
			pos = exifData.findKey(ekey);
			} catch (... ) {
				return "";
			}
		}

		if (pos == exifData.end() || pos->count() == 0) {
			qDebug() << "Information is not set in the Exif Data";
		} else {
			Exiv2::Value::AutoPtr v = pos->getValue();
			//Exiv2::StringValue* prv = dynamic_cast<Exiv2::StringValue*>(v.release());
			//Exiv2::StringValue::AutoPtr rv = Exiv2::StringValue::AutoPtr(prv);

			//info = rv->toString();
			info = pos->toString();
		}
	}

	return info;

}

std::string DkMetaData::getIptcValue(std::string key) {
	std::string info = "";

	readMetaData();
	if (!mdata)
		return info;

	Exiv2::IptcData &iptcData = exifImg->iptcData();

	if (!iptcData.empty()) {

		Exiv2::IptcData::iterator pos;

		try {
			Exiv2::IptcKey ekey = Exiv2::IptcKey(key);
			pos = iptcData.findKey(ekey);
		} catch (...) {
			return "";
		}

		if (pos == iptcData.end() || pos->count() == 0) {
			qDebug() << "Orientation is not set in the Exif Data";
		} else {
			Exiv2::Value::AutoPtr v = pos->getValue();
			//Exiv2::StringValue* prv = dynamic_cast<Exiv2::StringValue*>(v.release());
			//Exiv2::StringValue::AutoPtr rv = Exiv2::StringValue::AutoPtr(prv);

			//info = rv->toString();
			info = pos->toString();
		}
	}

	return info;
}

bool DkMetaData::setExifValue(std::string key, std::string taginfo) {

	readMetaData();
	if (!mdata)
		return false;

	Exiv2::ExifData &exifData = exifImg->exifData();

	if (!exifData.empty()) {
		
		Exiv2::Exifdatum& tag = exifData[key];
		
		if (!tag.setValue(taginfo)) {
			exifImg->setExifData(exifData);
			exifImg->writeMetadata();
			return true;
		} else
			qDebug() << "could not write Exif Data";
			return false;
	}

	return false;

	//Exiv2::Value::AutoPtr v = Exiv2::Value::create(Exiv2::asciiString);
	//// Set the value to a string
	//v->read("1999:12:31 23:59:59");
	//// Add the value together with its key to the Exif data container
	//Exiv2::ExifKey key("Exif.Photo.DateTimeOriginal");
	//exifData.add(key, v.get());
}

void DkMetaData::saveOrientation(int o) {

	readMetaData();
	if (!mdata) {
		throw DkFileException("could not read exif data\n", __LINE__, __FILE__);
	}
	if (o!=90 && o!=-90 && o!=180 && o!=0 && o!=270) {
		qDebug() << "wrong rotation parameter";
		throw DkIllegalArgumentException("wrong rotation parameter\n", __LINE__, __FILE__);
	}
	if (o==-180) o=180;
	if (o== 270) o=-90;

	int orientation;

	Exiv2::ExifData &exifData = exifImg->exifData();
	Exiv2::ExifKey key = Exiv2::ExifKey("Exif.Image.Orientation");


	////----------
	//print all Exif values
	//Exiv2::ExifData::const_iterator end = exifData.end();
	//for (Exiv2::ExifData::const_iterator i = exifData.begin(); i != end; ++i) {
	//	const char* tn = i->typeName();
	//	std::cout << std::setw(44) << std::setfill(' ') << std::left
	//		<< i->key() << " "
	//		<< "0x" << std::setw(4) << std::setfill('0') << std::right
	//		<< std::hex << i->tag() << " "
	//		<< std::setw(9) << std::setfill(' ') << std::left
	//		<< (tn ? tn : "Unknown") << " "
	//		<< std::dec << std::setw(3)
	//		<< std::setfill(' ') << std::right
	//		<< i->count() << "  "
	//		<< std::dec << i->value()
	//		<< "\n";

	//}
	////----------

	if (exifData.empty()) {
		exifData["Exif.Image.Orientation"] = uint16_t(1);
		qDebug() << "Orientation added to Exif Data";
	}
		
	Exiv2::ExifData::iterator pos = exifData.findKey(key);

	if (pos == exifData.end() || pos->count() == 0) {
		exifData["Exif.Image.Orientation"] = uint16_t(1);

		pos = exifData.findKey(key);
		qDebug() << "Orientation added to Exif Data";
	}

	Exiv2::Value::AutoPtr v = pos->getValue();
	Exiv2::UShortValue* prv = dynamic_cast<Exiv2::UShortValue*>(v.release());
	Exiv2::UShortValue::AutoPtr rv = Exiv2::UShortValue::AutoPtr(prv);
	orientation = (int) rv->value_[0];
	if (orientation <= 0 || orientation > 8) orientation = 1;

	switch (orientation) {
		case 1: if (o!=0) orientation = (o == -90) ? 8 : (o==90 ? 6 : 3);
			break;
		case 2: if (o!=0) orientation = (o == -90) ? 5 : (o==90 ? 7 : 4);
			break;
		case 3: if (o!=0) orientation = (o == -90) ? 6 : (o==90 ? 8 : 1);
			break;
		case 4: if (o!=0) orientation = (o == -90) ? 7 : (o==90 ? 5 : 2);
			break;
		case 5: if (o!=0) orientation = (o == -90) ? 4 : (o==90 ? 2 : 7);
			break;
		case 6: if (o!=0) orientation = (o == -90) ? 1 : (o==90 ? 3 : 8);
			break;
		case 7: if (o!=0) orientation = (o == -90) ? 2 : (o==90 ? 4 : 5);
			break;
		case 8: if (o!=0) orientation = (o == -90) ? 3 : (o==90 ? 1 : 6);
			break;
	}
	rv->value_[0] = (unsigned short) orientation;

	//////----------
	////print all Exif values
	///*Exiv2::ExifData::const_iterator */end = exifData.end();
	//for (Exiv2::ExifData::const_iterator i = exifData.begin(); i != end; ++i) {
	//	const char* tn = i->typeName();
	//	std::cout << std::setw(44) << std::setfill(' ') << std::left
	//		<< i->key() << " "
	//		<< "0x" << std::setw(4) << std::setfill('0') << std::right
	//		<< std::hex << i->tag() << " "
	//		<< std::setw(9) << std::setfill(' ') << std::left
	//		<< (tn ? tn : "Unknown") << " "
	//		<< std::dec << std::setw(3)
	//		<< std::setfill(' ') << std::right
	//		<< i->count() << "  "
	//		<< std::dec << i->value()
	//		<< "\n";
	//}
	//////----------

	pos->setValue(rv.get());
	//metadaten schreiben
	exifImg->setExifData(exifData);
	exifImg->writeMetadata();
	
}

int DkMetaData::getHorizontalFlipped() {
	
	readMetaData();
	if (!mdata)
		return -1;

	int flipped;

	Exiv2::ExifData &exifData = exifImg->exifData();

	if (exifData.empty()) {
		flipped = -1;
	} else {

		Exiv2::ExifKey key = Exiv2::ExifKey("Exif.Image.Orientation");
		Exiv2::ExifData::iterator pos = exifData.findKey(key);

		if (pos == exifData.end() || pos->count() == 0) {
			qDebug() << "Orientation is not set in the Exif Data";
			flipped = -1;
		} else {
			Exiv2::Value::AutoPtr v = pos->getValue();
			Exiv2::UShortValue* prv = dynamic_cast<Exiv2::UShortValue*>(v.release());
			Exiv2::UShortValue::AutoPtr rv = Exiv2::UShortValue::AutoPtr(prv);
			flipped = (int)rv->value_[0];

			switch (flipped) {
			case 2: flipped = 1;
				break;
			case 7: flipped = 1;
				break;
			case 4: flipped = 1;
				break;
			case 5: flipped = 1;
				break;
			default: flipped = 0;
				break;
			}
		}
	}

	return flipped;
}

void DkMetaData::saveHorizontalFlipped(int f) {
	
	readMetaData();
	if (!mdata)
		return;

	int flipped;

	Exiv2::ExifData &exifData = exifImg->exifData();
	Exiv2::ExifKey key = Exiv2::ExifKey("Exif.Image.Orientation");

	if (exifData.empty()) {
		exifData["Exif.Image.Orientation"] = uint16_t(1);
		qDebug() << "Orientation added to Exif Data";
	}

	Exiv2::ExifData::iterator pos = exifData.findKey(key);

	if (pos == exifData.end() || pos->count() == 0) {
		exifData["Exif.Image.Orientation"] = uint16_t(1);
		pos = exifData.findKey(key);
		qDebug() << "Orientation added to Exif Data";
	}

	Exiv2::Value::AutoPtr v = pos->getValue();
	Exiv2::UShortValue* prv = dynamic_cast<Exiv2::UShortValue*>(v.release());
	Exiv2::UShortValue::AutoPtr rv = Exiv2::UShortValue::AutoPtr(prv);
	flipped = (int)rv->value_[0];
		
	if (flipped <= 0 || flipped > 8) flipped = 1;

	switch (flipped) {
		case 1: flipped = f != 0 ? 2 : flipped ;
			break;
		case 2: flipped = f != 0 ? 1 : flipped ;
			break;
		case 3: flipped = f != 0 ? 4 : flipped ;
			break;
		case 4: flipped = f != 0 ? 3 : flipped ;
			break;
		case 5: flipped = f != 0 ? 8 : flipped ;
			break;
		case 6: flipped = f != 0 ? 7 : flipped ;
			break;
		case 7: flipped = f != 0 ? 6 : flipped;
			break;
		case 8: flipped = f != 0 ? 5 : flipped ;
			break;
	}

	rv->value_[0] = (unsigned short) flipped;

	pos->setValue(rv.get());
	//metadaten schreiben
	exifImg->setExifData(exifData);

	exifImg->writeMetadata();
		
	
}

//only for debug
void DkMetaData::printMetaData() {
	
	readMetaData();
	if (!mdata)
		return;

	Exiv2::ExifData &exifData = exifImg->exifData();
	Exiv2::IptcData &iptcData = exifImg->iptcData();
	Exiv2::XmpData &xmpData = exifImg->xmpData();

	qDebug() << "Exif------------------------------------------------------------------";

	Exiv2::ExifData::const_iterator end = exifData.end();
	for (Exiv2::ExifData::const_iterator i = exifData.begin(); i != end; ++i) {
		const char* tn = i->typeName();
		std::cout << std::setw(44) << std::setfill(' ') << std::left
			<< i->key() << " "
			<< "0x" << std::setw(4) << std::setfill('0') << std::right
			<< std::hex << i->tag() << " "
			<< std::setw(9) << std::setfill(' ') << std::left
			<< (tn ? tn : "Unknown") << " "
			<< std::dec << std::setw(3)
			<< std::setfill(' ') << std::right
			<< i->count() << "  "
			<< std::dec << i->value()
			<< "\n";
	}

	qDebug() << "IPTC------------------------------------------------------------------";

	Exiv2::IptcData::iterator endI2 = iptcData.end();
	for (Exiv2::IptcData::iterator md = iptcData.begin(); md != endI2; ++md) {
		std::cout << std::setw(44) << std::setfill(' ') << std::left
			<< md->key() << " "
			<< "0x" << std::setw(4) << std::setfill('0') << std::right
			<< std::hex << md->tag() << " "
			<< std::setw(9) << std::setfill(' ') << std::left
			<< md->typeName() << " "
			<< std::dec << std::setw(3)
			<< std::setfill(' ') << std::right
			<< md->count() << "  "
			<< std::dec << md->value()
			<< std::endl;
	}

	qDebug() << "XMP------------------------------------------------------------------";

	Exiv2::XmpData::iterator endI3 = xmpData.end();
	for (Exiv2::XmpData::iterator md = xmpData.begin(); md != endI3; ++md) {
		std::cout << std::setw(44) << std::setfill(' ') << std::left
			<< md->key() << " "
			<< "0x" << std::setw(4) << std::setfill('0') << std::right
			<< std::hex << md->tag() << " "
			<< std::setw(9) << std::setfill(' ') << std::left
			<< md->typeName() << " "
			<< std::dec << std::setw(3)
			<< std::setfill(' ') << std::right
			<< md->count() << "  "
			<< std::dec << md->value()
			<< std::endl;
	}
}

float DkMetaData::getRating() {

	readMetaData();
	if (!mdata)
		return -1.0f;

	float exifRating = -1;
	float xmpRating = -1;
	float fRating = 0;


	Exiv2::ExifData &exifData = exifImg->exifData();		//Exif.Image.Rating  - short
															//Exif.Image.RatingPercent - short
	Exiv2::XmpData &xmpData = exifImg->xmpData();			//Xmp.xmp.Rating - text
															//Xmp.MicrosoftPhoto.Rating -text


	//get Rating of Exif Tag
	if (!exifData.empty()) {
		Exiv2::ExifKey key = Exiv2::ExifKey("Exif.Image.Rating");
		Exiv2::ExifData::iterator pos = exifData.findKey(key);

		if (pos != exifData.end() && pos->count() != 0) {
			Exiv2::Value::AutoPtr v = pos->getValue();
			exifRating = v->toFloat();
		}
	}

	//get Rating of Xmp Tag
	if (!xmpData.empty()) {
		Exiv2::XmpKey key = Exiv2::XmpKey("Xmp.xmp.Rating");
		Exiv2::XmpData::iterator pos = xmpData.findKey(key);

		//xmp Rating tag
		if (pos != xmpData.end() && pos->count() != 0) {
			Exiv2::Value::AutoPtr v = pos->getValue();
			xmpRating = v->toFloat();
		}

		//if xmpRating not found, try to find MicrosoftPhoto Rating tag
		if (xmpRating == -1) {
			key = Exiv2::XmpKey("Xmp.MicrosoftPhoto.Rating");
			pos = xmpData.findKey(key);
			if (pos != xmpData.end() && pos->count() != 0) {
				Exiv2::Value::AutoPtr v = pos->getValue();
				xmpRating = v->toFloat();
			}
		}
	}

	if (xmpRating == -1.0f && exifRating != -1.0f)
		fRating = exifRating;
	else if (xmpRating != -1.0f && exifRating == -1.0f)
		fRating = xmpRating;
	else
		fRating = exifRating;

	return fRating;
}

void DkMetaData::setRating(int r) {
	
	readMetaData();	
	if (!mdata)
		return;

	unsigned short percentRating = 0;
	std::string sRating, sRatingPercent;

	if (r == 5)  { percentRating = 99; sRating = "5"; sRatingPercent = "99";}
	else if (r==4) { percentRating = 75; sRating = "4"; sRatingPercent = "75";}
	else if (r==3) { percentRating = 50; sRating = "3"; sRatingPercent = "50";}
	else if (r==2) { percentRating = 25; sRating = "2"; sRatingPercent = "25";}
	else if (r==1) {percentRating = 1; sRating = "1"; sRatingPercent = "1";}
	else {r=0;}

	Exiv2::ExifData &exifData = exifImg->exifData();		//Exif.Image.Rating  - short
															//Exif.Image.RatingPercent - short
	Exiv2::XmpData &xmpData = exifImg->xmpData();			//Xmp.xmp.Rating - text
															//Xmp.MicrosoftPhoto.Rating -text

	if (r>0) {
		exifData["Exif.Image.Rating"] = uint16_t(r);
		exifData["Exif.Image.RatingPercent"] = uint16_t(r);
		//xmpData["Xmp.xmp.Rating"] = Exiv2::xmpText(sRating);
	
		Exiv2::Value::AutoPtr v = Exiv2::Value::create(Exiv2::xmpText);
		v->read(sRating);
		xmpData.add(Exiv2::XmpKey("Xmp.xmp.Rating"), v.get());
		v->read(sRatingPercent);
		xmpData.add(Exiv2::XmpKey("Xmp.MicrosoftPhoto.Rating"), v.get());
	} else {

		Exiv2::ExifKey key = Exiv2::ExifKey("Exif.Image.Rating");
		Exiv2::ExifData::iterator pos = exifData.findKey(key);
		if (pos != exifData.end()) exifData.erase(pos);

		key = Exiv2::ExifKey("Exif.Image.RatingPercent");
		pos = exifData.findKey(key);
		if (pos != exifData.end()) exifData.erase(pos);

		Exiv2::XmpKey key2 = Exiv2::XmpKey("Xmp.xmp.Rating");
		Exiv2::XmpData::iterator pos2 = xmpData.findKey(key2);
		if (pos2 != xmpData.end()) xmpData.erase(pos2);

		key2 = Exiv2::XmpKey("Xmp.MicrosoftPhoto.Rating");
		pos2 = xmpData.findKey(key2);
		if (pos2 != xmpData.end()) xmpData.erase(pos2);
	}

	exifImg->setExifData(exifData);
	exifImg->setXmpData(xmpData);
	exifImg->writeMetadata();

}


void DkMetaData::saveMetaDataToFile(QFileInfo fileN, int orientation) {

	readMetaData();	
	if (!mdata)
		return;

	Exiv2::ExifData &exifData = exifImg->exifData();
	Exiv2::XmpData &xmpData = exifImg->xmpData();
	Exiv2::IptcData &iptcData = exifImg->iptcData();

	Exiv2::Image::AutoPtr exifImgN;
	
	try {

		exifImgN = Exiv2::ImageFactory::open(fileN.absoluteFilePath().toStdString());

	} catch (...) {

		qDebug() << "could not open image for exif data";
		return;
	}

	if (exifImgN.get() == 0) {
		qDebug() << "image could not be opened for exif data extraction";
		return;
	}

	exifImgN->readMetadata();

	exifData["Exif.Image.Orientation"] = uint16_t(orientation);

	exifImgN->setExifData(exifData);
	exifImgN->setXmpData(xmpData);
	exifImgN->setIptcData(iptcData);

	exifImgN->writeMetadata();

}

bool DkMetaData::isTiff() {
	//Exiv2::ImageType::tiff has the same key as nef, ...
	//int type;
	//type = Exiv2::ImageFactory::getType(file.absoluteFilePath().toStdString());
	//return (type==Exiv2::ImageType::tiff);
	QString newSuffix = file.suffix();

	return newSuffix.contains(QRegExp("(tif|tiff)", Qt::CaseInsensitive));
}

bool DkMetaData::isJpg() {

	QString newSuffix = file.suffix();

	return newSuffix.contains(QRegExp("(jpg|jpeg)", Qt::CaseInsensitive));
}

bool DkMetaData::isRaw() {

	QString newSuffix = file.suffix();

	return newSuffix.contains(QRegExp("(nef|crw|cr2|arw)", Qt::CaseInsensitive));
}

void DkMetaData::readMetaData() {

	if (!mdata) {
	
		try {

			exifImg = Exiv2::ImageFactory::open(file.absoluteFilePath().toStdString());

		} catch (...) {
			mdata = false;
			qDebug() << "could not open image for exif data";
			return;
		}

		if (exifImg.get() == 0) {
			qDebug() << "image could not be opened for exif data extraction";
			mdata = false;
			return;
		}

		try {
			exifImg->readMetadata();

			if (!exifImg->good()) {
				qDebug() << "metadata could not be read";
				mdata = false;
				return;
			}

		}catch (...) {
			mdata = false;
			return;
		}


		mdata = true;

	}
}

void DkMetaData::reloadImg() {

	try {

		exifImg = Exiv2::ImageFactory::open(file.absoluteFilePath().toStdString());

	} catch (...) {
		mdata = false;
		qDebug() << "could not open image for exif data";
		return;
	}

	if (exifImg.get() == 0) {
		qDebug() << "image could not be opened for exif data extraction";
		mdata = false;
		return;
	}

	exifImg->readMetadata();

	if (!exifImg->good()) {
		qDebug() << "metadata could not be read";
		mdata = false;
		return;
	}

	mdata = true;
}