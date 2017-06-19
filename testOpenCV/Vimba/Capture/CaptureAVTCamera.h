/*----------------------------------------------------------------------------*/
/*    Copyright (C) 2015 Alexandre Campo                                      */
/*                                                                            */
/*    This file is part of USE Tracker.                                       */
/*                                                                            */
/*    USE Tracker is free software: you can redistribute it and/or modify     */
/*    it under the terms of the GNU General Public License as published by    */
/*    the Free Software Foundation, either version 3 of the License, or       */
/*    (at your option) any later version.                                     */
/*                                                                            */
/*    USE Tracker is distributed in the hope that it will be useful,          */
/*    but WITHOUT ANY WARRANTY; without even the implied warranty of          */
/*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           */
/*    GNU General Public License for more details.                            */
/*                                                                            */
/*    You should have received a copy of the GNU General Public License       */
/*    along with USE Tracker.  If not, see <http://www.gnu.org/licenses/>.    */
/*----------------------------------------------------------------------------*/

#pragma once

#include "Capture.h"
#include "vimba/ApiController.h"

class AVT::VmbAPI::ApiController;

struct CaptureAVTCamera : Capture {
    int device;
    AVT::VmbAPI::ApiController vimbaApiController;

    unsigned int frameNumber = 0;
    unsigned long long startTime = 0;
    unsigned long long pauseTime = 0;
    unsigned long long lastFrameTime = 0;
    unsigned long long nextFrameTime = 0;
    unsigned long long playTimestep = 0;

    CaptureAVTCamera(int device);
    CaptureAVTCamera(cv::FileNode& fn);
    ~CaptureAVTCamera();

    static inline long long int get_now_us() {
        return std::chrono::high_resolution_clock::now().time_since_epoch() / std::chrono::microseconds(1);
    }

    bool Open(int device);
    void Close();

    void Pause();
    void Play();
    void Stop();

    bool GetNextFrame();
    bool GetFrame(double time);
    unsigned long long GetNextFrameSystemTime();
    unsigned long long InternalGetTime();
    double GetTime();

    long GetFrameNumber();
    long GetFrameCount();

    void LoadXML(cv::FileNode& fn);
    void SaveXML(cv::FileStorage& fs);

    std::string GetName();
};
