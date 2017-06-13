///*----------------------------------------------------------------------------*/
///*    Copyright (C) 2015 Alexandre Campo                                      */
///*                                                                            */
///*    This file is part of USE Tracker.                                       */
///*                                                                            */
///*    USE Tracker is free software: you can redistribute it and/or modify     */
///*    it under the terms of the GNU General Public License as published by    */
///*    the Free Software Foundation, either version 3 of the License, or       */
///*    (at your option) any later version.                                     */
///*                                                                            */
///*    USE Tracker is distributed in the hope that it will be useful,          */
///*    but WITHOUT ANY WARRANTY; without even the implied warranty of          */
///*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           */
///*    GNU General Public License for more details.                            */
///*                                                                            */
///*    You should have received a copy of the GNU General Public License       */
///*    along with USE Tracker.  If not, see <http://www.gnu.org/licenses/>.    */
///*----------------------------------------------------------------------------*/
//
//#include "CaptureAVTCamera.h"
//
//#include <chrono>
//#include <thread>
//#include <iostream>
////#include <chrono>
//
//#include <VimbaCPP/Include/VimbaCPP.h>
//#include <opencv2/core/persistence.hpp>
//
//using namespace std;
//
//
//CaptureAVTCamera::CaptureAVTCamera(int device) : Capture() {
//	if (Open(device))
//		type = AVT_CAMERA;
//}
//
//CaptureAVTCamera::CaptureAVTCamera(cv::FileNode& fn) : Capture() {
//	LoadXML(fn);
//	if (Open(device))
//		type = AVT_CAMERA;
//}
//
//CaptureAVTCamera::~CaptureAVTCamera() {
//
//}
//
//string CaptureAVTCamera::GetName() {
//	string str = "AVT Camera device ";
//	return str + std::to_string(device);
//}
//
//bool CaptureAVTCamera::Open(int device) {
//	this->device = device;
//	std::string cameraID;
//	VmbErrorType err = VmbErrorSuccess;
//
//	err = vimbaApiController.StartUp();
//	if (err == VmbErrorSuccess) {
//		AVT::VmbAPI::CameraPtrVector cameras = vimbaApiController.GetCameraList();
//		if (cameras.empty()) {
//			err = VmbErrorNotFound;
//		}
//		else {
//			err = cameras[device]->GetID(cameraID);
//		}
//	}
//	if (err == VmbErrorSuccess) {
//		log_time << "Opening camera with ID: " << cameraID << "\n";
//		err = vimbaApiController.StartContinuousImageAcquisition(cameraID);
//	}
//
//	if (err != VmbErrorSuccess) {
//		std::string strError = vimbaApiController.ErrorCodeToMessage(err);
//		log_time << "\nAn error occured:" << strError << "\n";
//		return false;
//	}
//
//	// Get the properties from the camera
//	width = vimbaApiController.GetWidth();
//	height = vimbaApiController.GetHeight();
//
//	// estimate FPS by retrieving xx frames and measuring time
//	wxLongLong t1 = wxGetUTCTimeUSec();
//	int frameCount = 0;
//	while (1) {
//		if (vimbaApiController.FrameAvailable()) {
//			unsigned char* buffer;
//			AVT::VmbAPI::FramePtr pFrame = vimbaApiController.GetFrame();
//			VmbErrorType err = SP_ACCESS( pFrame )->GetImage(buffer);
//			if (err == VmbErrorSuccess) frameCount++;
//			vimbaApiController.QueueFrame(pFrame);
//
//			if (frameCount == 20) break;
//		}
//		wxMicroSleep(10);
//	}
//	wxLongLong t2 = wxGetUTCTimeUSec();
//	double delay = (t2 - t1).ToDouble() / 1000000.0;
//	fps = 20.0 / delay;
//
//	log_time << "Camera properties\n";
//	log_time << "width = " << width << std::endl << " height = " << height << " fps = " << fps << std::endl;
//
//	playTimestep.Assign(1000000.0 / fps);
//	startTime = wxGetUTCTimeUSec();
//	pauseTime = startTime;
//	isPaused = true;
//
//	frame = cv::Mat(height, width, CV_8UC3);
//
//	return (!frame.empty());
//}
//
//void CaptureAVTCamera::Close() {
//
//}
//
//bool CaptureAVTCamera::GetNextFrame() {
//	bool gotFrame = false;
//
//	// try to get a frame during x useconds
//	while (1) {
//		if (vimbaApiController.FrameAvailable()) {
//			gotFrame = vimbaApiController.GetFrame(frame);
//		}
//
//		if (gotFrame) break;
//		else if (wxGetUTCTimeUSec() - startTime > 1000000) {
//			return false;
//		}
//
//		// did not get a frame, wait a bit before retrying
//		wxMicroSleep(100);
//		//	log_time << "Waiting for a frame... " << startTime << "\t" << wxGetUTCTimeUSec() << std::endl;
//	}
//
//	frameNumber++;
//	lastFrameTime = InternalGetTime();
//	nextFrameTime += playTimestep;
//
//	return !frame.empty();
//}
//
//void CaptureAVTCamera::Stop() {
//	isPaused = false;
//	isStopped = true;
//	statusChanged = true;
//	frameNumber = 0;
//}
//
//void CaptureAVTCamera::Pause() {
//	isPaused = true;
//	statusChanged = true;
//	pauseTime = wxGetUTCTimeUSec();
//}
//
//void CaptureAVTCamera::Play() {
//	if (isPaused) {
//		startTime += wxGetUTCTimeUSec() - pauseTime;
//		nextFrameTime = wxGetUTCTimeUSec() + playTimestep;
//		statusChanged = true;
//		isPaused = false;
//	}
//	if (isStopped) {
//		startTime = wxGetUTCTimeUSec();
//		nextFrameTime = startTime + playTimestep;
//		statusChanged = true;
//		isStopped = false;
//	}
//}
//
//bool CaptureAVTCamera::GetFrame(double time) {
//	time *= 1000000.0;
//	while (InternalGetTime() < time) this_thread::sleep_for(chrono::milliseconds(10));
//
//	if (vimbaApiController.FrameAvailable())
//		vimbaApiController.GetFrame(frame);
//	else return false;
//
//	frameNumber++;
//	lastFrameTime = InternalGetTime();
//	nextFrameTime = wxGetUTCTimeUSec() + playTimestep;
//
//	return !frame.empty();
//}
//
//long CaptureAVTCamera::GetFrameNumber() {
//	return frameNumber; // source.get(CV_CAP_PROP_POS_FRAMES);
//}
//
//long CaptureAVTCamera::GetFrameCount() {
//	return 1;
//}
//
//double CaptureAVTCamera::GetTime() {
//	if (isStopped) return 0;
//	else return lastFrameTime.ToDouble() / 1000000.0;
//}
//
//wxLongLong CaptureAVTCamera::InternalGetTime() {
//	if (isPaused) return (pauseTime - startTime);
//	else if (isStopped) return 0;
//	else return (wxGetUTCTimeUSec() - startTime);
//}
//
//
//void CaptureAVTCamera::SaveXML(FileStorage& fs) {
//	fs << "Type" << "AVTcamera";
//	fs << "Device" << device;
//
//	if (calibration.calibrated) {
//		fs << "Calibration" << "{";
//
//		calibration.SaveXML(fs);
//
//		fs << "}";
//	}
//}
//
//void CaptureAVTCamera::LoadXML(FileNode& fn) {
//	if (!fn.empty()) {
//		device = (int)fn["Device"];
//
//		FileNode calibNode = fn["Calibration"];
//		if (!calibNode.empty()) {
//			calibration.LoadXML(calibNode);
//		}
//	}
//}
//
