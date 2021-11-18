///////////////////////////////////////////////////////////////////////////////
// Name:        camerathread.h
// Purpose:     Thread and event for retrieving images from a camera with cv::VideoCapture
// Author:      PB
// Created:     2021-11-18
// Copyright:   (c) 2021 PB
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////


#ifndef CAMERATHREAD_H
#define CAMERATHREAD_H

#include <wx/wx.h>
#include <wx/thread.h>

#include <atomic>
#include <memory>
#include <vector>


// for wxLogTrace
#define TRACE_WXOPENCVCAMERAS "WXOPENCVCAMERAS"

/***********************************************************************************************

    CameraEvent: an event sent from the worker thread to the GUI thread.
                 When type is EVT_CAMERA_ERROR_EXCEPTION, the exception information
                 is available via its inherited GetString() method.

***********************************************************************************************/

class CameraEvent : public wxThreadEvent
{
public:
    // eventType is one of EVT_CAMERA_xxx types declared below
    CameraEvent(wxEventType eventType, const wxString& cameraName)
        : wxThreadEvent(eventType), m_cameraName(cameraName)
    {}

    wxString GetCameraName() const  { return m_cameraName; }

    wxEvent* Clone() const override { return new CameraEvent(*this); }
protected:
    wxString    m_cameraName;
};

// Camera capture started
wxDECLARE_EVENT(EVT_CAMERA_CAPTURE_STARTED, CameraEvent);
// Could not open OpenCV camera capture
wxDECLARE_EVENT(EVT_CAMERA_ERROR_OPEN, CameraEvent);
// Could not retrieve a frame, consider connection to the camera lost.
wxDECLARE_EVENT(EVT_CAMERA_ERROR_EMPTY, CameraEvent);
// An exception was thrown in the camera thread,
// see the event's GetString() for the exception information.
wxDECLARE_EVENT(EVT_CAMERA_ERROR_EXCEPTION, CameraEvent);



/***********************************************************************************************

    CameraFrameData: a class containing information about a captured camera frame,
                     created in a camera thread and processed in the GUI thread

***********************************************************************************************/

class CameraFrameData
{
public:
    CameraFrameData(const wxString& cameraName,
                    const wxULongLong frameNumber);
    ~CameraFrameData();

    wxString GetCameraName() const { return m_cameraName; }

    // captured camera frame
    wxBitmap*    GetFrame() { return m_frame; }

    // optional thumbnail, created when thumbnailSize passed to CameraThread is not empty
    wxBitmap*    GetThumbnail() { return m_thumbnail; }

    // frame number, starting with 0
    wxULongLong  GetFrameNumber() const { return m_frameNumber; }

    // All times are in milliseconds

    // how long it took to retrieve the frame from OpenCV
    long GetTimeToRetrieve() const { return m_timeToRetrieve; }

    // how long it took to convert the frame from cv::Mat to wxBitmap
    long GetTimeToConvert() const { return m_timeToConvert; }

    // how long it took to resize and convert the frame to thumbnail
    long GetTimeToCreateThumbnail() const { return m_timeToCreateThumbnail; }

    // when was the image captured, obtained with wxGetUTCTimeMillis()
    wxLongLong GetCapturedTime() const { return m_capturedTime ; }

    // Setters

    void SetCameraName(const wxString& cameraName) { m_cameraName = cameraName; }

    void SetFrame(wxBitmap* frame)                { m_frame = frame; }
    void SetThumbnail(wxBitmap* thumbnail)        { m_thumbnail = thumbnail; }
    void SetFrameNumber(const wxULongLong number) { m_frameNumber = number; }

    void SetTimeToRetrieve(const long t)        { m_timeToRetrieve = t; }
    void SetTimeToConvert(const long t)         { m_timeToConvert = t; }
    void SetTimeToCreateThumbnail(const long t) { m_timeToCreateThumbnail = t; }
    void SetCapturedTime(const wxLongLong t)    { m_capturedTime = t; }
private:
    wxString    m_cameraName;
    wxBitmap*   m_frame{nullptr};
    wxBitmap*   m_thumbnail{nullptr};
    wxULongLong m_frameNumber{0};
    wxLongLong  m_capturedTime{0};
    long        m_timeToRetrieve{0};
    long        m_timeToConvert{0};
    long        m_timeToCreateThumbnail{0};
};

typedef std::unique_ptr<CameraFrameData> CameraFrameDataPtr;
typedef std::vector<CameraFrameDataPtr>  CameraFrameDataVector;


/***********************************************************************************************

CameraThread: a worker wxThread for retrieving images from a camera with OpenCV
and sending them to GUI (a wxEvtHandler*) for display

***********************************************************************************************/

// forward declaration to avoid including OpenCV header
namespace cv { class VideoCapture; }


class CameraThread : public wxThread
{
public:
    CameraThread(const wxString& cameraAddress, const wxString& cameraName,
                 wxEvtHandler& eventSink,
                 CameraFrameDataVector& frames,
                 wxCriticalSection& framesCS,
                 const wxSize& thumbnailSize, long sleepTime = 30);

    wxString GetCameraAddress() const { return m_cameraAddress; }
    wxString GetCameraName() const    { return m_cameraName; }
    bool     IsCapturing() const      { return m_isCapturing; }
protected:
    wxString                          m_cameraAddress;
    wxString                          m_cameraName;
    wxEvtHandler&                     m_eventSink;
    CameraFrameDataVector&            m_frames;
    wxCriticalSection&                m_framesCS;
    long                              m_sleepTime{0};
    std::unique_ptr<cv::VideoCapture> m_cameraCapture;
    wxSize                            m_thumbnailSize;
    std::atomic_bool                  m_isCapturing{false};

    bool     InitCapture();
    ExitCode Entry() override;
};

#endif // #ifndef CAMERATHREAD_H