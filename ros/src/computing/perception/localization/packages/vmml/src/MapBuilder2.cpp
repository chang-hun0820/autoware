/*
 * MapBuilder2.cpp
 *
 *  Created on: Jul 26, 2018
 *      Author: sujiwo
 */

#include <exception>
#include <thread>
#include "MapBuilder2.h"
#include "optimizer.h"
#include "ImageDatabase.h"
#include "Viewer.h"
#include "utilities.h"



using namespace std;
using namespace Eigen;


MapBuilder2::MapBuilder2() :
	currentAnchor(0)
{
	cMap = new VMap();
//	imageView = new Viewer;
}


MapBuilder2::~MapBuilder2()
{
//	delete(imageView);
}


void
MapBuilder2::initialize (const InputFrame &f1, const InputFrame &f2)
{
	kfid k1 = cMap->createKeyFrame(f1.image, f1.position, f1.orientation, f1.cameraId, NULL, f1.setKfId);
	kfid k2 = cMap->createKeyFrame(f2.image, f2.position, f2.orientation, f2.cameraId, NULL, f2.setKfId);
	cMap->estimateStructure(k1, k2);
	currentAnchor = k2;
	initialized = true;
}


void
MapBuilder2::track (const InputFrame &f)
{
	if (initialized==false)
		throw runtime_error("Map not initialized");

	kfid fId = cMap->createKeyFrame(f.image, f.position, f.orientation, f.cameraId, NULL, f.setKfId);
	cMap->estimateAndTrack(currentAnchor, fId);
	currentAnchor = fId;
}


void
MapBuilder2::track2(const InputFrame &f)
{
	if (initialized==false) {
		auto normcdf = cdf(f.image);
		if (normcdf[127] < 0.25)
			return;

		if (frame0.image.empty()) {
			frame0 = f;
			return;
		}

		else {
			double runTrans, runRot;
			f.getPose().displacement(frame0.getPose(), runTrans, runRot);
			if (runTrans>=translationThrs or runRot>=rotationThrs) {
				initialize(frame0, f);
				// XXX: Callback here
				initialized = true;
				return;
			}
		}
	}
}


void
MapBuilder2::build ()
{
	thread ba([this] {
				cout << "Bundling...";
				bundle_adjustment(cMap);
				cout << "Done\n";
	});

	thread db([this] {
				cout << "Rebuilding Image DB... ";
				cout.flush();
				cMap->getImageDB()->rebuildAll();
				cout << "Done\n";
	});

	ba.join();
	db.join();
	return;
}


void
MapBuilder2::runFromDataset(GenericDataset *ds)
{
	sourceDataset = ds;
	if (initialized != false)
		throw runtime_error("Map has been running; aborted");

	initialized = true;

	this->build();
}
