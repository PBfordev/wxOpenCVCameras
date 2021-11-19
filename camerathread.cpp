///////////////////////////////////////////////////////////////////////////////
// Name:        camerathread.cpp
// Purpose:     Thread and event for retrieving images from a camera with cv::VideoCapture
// Author:      PB
// Created:     2021-11-18
// Copyright:   (c) 2021 PB
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#include <memory>

#include <opencv2/opencv.hpp>

#include "camerathread.h"
#include "convertmattowxbmp.h"


/***********************************************************************************************

    CameraEvent

***********************************************************************************************/

// see the header for description
wxDEFINE_EVENT(EVT_CAMERA_CAPTURE_STARTED, CameraEvent);
wxDEFINE_EVENT(EVT_CAMERA_ERROR_OPEN, CameraEvent);
wxDEFINE_EVENT(EVT_CAMERA_ERROR_EMPTY, CameraEvent);
wxDEFINE_EVENT(EVT_CAMERA_ERROR_EXCEPTION, CameraEvent);


/***********************************************************************************************

    CameraFrameData

***********************************************************************************************/

CameraFrameData::CameraFrameData(const wxString& cameraName,
                                 const wxULongLong frameNumber)
    : m_cameraName(cameraName), m_frameNumber(frameNumber)
{}

CameraFrameData::~CameraFrameData()
{
    if ( m_frame )
        delete m_frame;
    if ( m_thumbnail )
        delete m_thumbnail;
}

/***********************************************************************************************

    CameraThread

***********************************************************************************************/

CameraThread::CameraThread(const wxString& cameraAddress, const wxString& cameraName,
                           wxEvtHandler& eventSink,
                           CameraFrameDataVector& frames,
                           wxCriticalSection& framesCS,
                           const wxSize& targetFrameSize, long sleepTime)
    : wxThread(wxTHREAD_JOINABLE),
      m_cameraAddress(cameraAddress), m_cameraName(cameraName), m_eventSink(eventSink),
      m_frames(frames), m_framesCS(framesCS),
      m_thumbnailSize(targetFrameSize), m_sleepTime(sleepTime)
{}

bool CameraThread::InitCapture()
{
    unsigned long cameraIndex = 0;

    if ( m_cameraAddress.ToCULong(&cameraIndex) )
        m_cameraCapture.reset(new cv::VideoCapture(cameraIndex));
    else
        m_cameraCapture.reset(new cv::VideoCapture(m_cameraAddress.ToStdString()));

    return m_cameraCapture->isOpened();
}

wxThread::ExitCode CameraThread::Entry()
{
#if wxCHECK_VERSION(3, 1, 6)
    SetName(wxString::Format("CameraThread %s", GetCameraName()));
#endif

    wxLogTrace(TRACE_WXOPENCVCAMERAS, "Entered CameraThread for camera '%s', attempting to start capture...", GetCameraName());

    if ( !InitCapture() )
    {
        wxLogTrace(TRACE_WXOPENCVCAMERAS, "Failed to start capture for camera '%s'", GetCameraName());
        m_eventSink.QueueEvent(new CameraEvent(EVT_CAMERA_ERROR_OPEN, GetCameraName()));
        return static_cast<wxThread::ExitCode>(nullptr);
    }
    else
    {
        CameraEvent* evt = new CameraEvent(EVT_CAMERA_CAPTURE_STARTED, GetCameraName());

        evt->SetString(m_cameraAddress);
        evt->SetInt(m_cameraCapture->get(static_cast<int>(cv::CAP_PROP_FPS)));
        m_eventSink.QueueEvent(evt);
    }

    const bool createThumbnail = m_thumbnailSize.GetWidth() > 0 && m_thumbnailSize.GetHeight() > 0;

    cv::Mat     matFrame;
    wxULongLong frameNumber{0};
    wxStopWatch stopWatch;

    m_isCapturing = true;

    while ( !TestDestroy() )
    {
        try
        {
            CameraFrameDataPtr frameData(new CameraFrameData(GetCameraName(), frameNumber++));

            stopWatch.Start();
            (*m_cameraCapture) >> matFrame;
            frameData->SetTimeToRetrieve(stopWatch.Time());
            frameData->SetCapturedTime(wxGetUTCTimeMillis());

            if ( !matFrame.empty() )
            {
                stopWatch.Start();
                frameData->SetFrame(new wxBitmap(matFrame.cols, matFrame.rows, 24));
                ConvertMatBitmapTowxBitmap(matFrame, *frameData->GetFrame());
                frameData->SetTimeToConvert(stopWatch.Time());

                if ( createThumbnail )
                {
                    cv::Mat matThumbnail;

                    stopWatch.Start();
                    cv::resize(matFrame, matThumbnail, cv::Size(m_thumbnailSize.GetWidth(), m_thumbnailSize.GetHeight()));
                    frameData->SetThumbnail(new wxBitmap(m_thumbnailSize, 24));
                    ConvertMatBitmapTowxBitmap(matThumbnail, *frameData->GetThumbnail());
                    frameData->SetTimeToCreateThumbnail(stopWatch.Time());
                }

                {
                    wxCriticalSectionLocker locker(m_framesCS);

                    m_frames.push_back(std::move(frameData));
                }

                // In a real code, the duration to sleep would normally
                // be computed based on the camera framerate, time taken
                // to process the image, and system clock tick resolution.
                if ( m_sleepTime > 0 )
                    Sleep(m_sleepTime);
                else
                    Yield();
            }
            else // connection to camera lost
            {
                m_isCapturing = false;
                m_eventSink.QueueEvent(new CameraEvent(EVT_CAMERA_ERROR_EMPTY, GetCameraName()));
                break;
            }
        }
        catch ( const std::exception& e )
        {
            m_isCapturing = false;

            CameraEvent* evt = new CameraEvent(EVT_CAMERA_ERROR_EXCEPTION, GetCameraName());

            evt->SetString(e.what());
            m_eventSink.QueueEvent(evt);
            break;
        }
        catch ( ... )
        {
            m_isCapturing = false;

            CameraEvent* evt = new CameraEvent(EVT_CAMERA_ERROR_EXCEPTION, GetCameraName());

            evt->SetString("Unknown exception");
            m_eventSink.QueueEvent(evt);
            break;
        }
    }

    wxLogTrace(TRACE_WXOPENCVCAMERAS, "Exiting CameraThread for camera '%s'...", GetCameraName());
    return static_cast<wxThread::ExitCode>(nullptr);
}