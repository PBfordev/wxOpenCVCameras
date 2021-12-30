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
#include <wx/any.h>
#include <wx/msgqueue.h>
#include <wx/thread.h>

#include <atomic>
#include <memory>
#include <vector>


// for wxLogTrace
#define TRACE_WXOPENCVCAMERAS "WXOPENCVCAMERAS"

/***********************************************************************************************

    CameraCommandData: a struct used by the main thread to communicate with CameraThread.

***********************************************************************************************/

struct CameraCommandData
{
    // see cv::VideoCapture get() and set() methods
    struct VCPropCommandParameter
    {
        int    id{0};
        double value{0.};
        // only for SetVCProp, result of cv::VideoCapture::set()
        bool   succeeded{false};
    };
    // for setting/getting multiple properties at once
	typedef std::vector<VCPropCommandParameter> VCPropCommandParameters;

    struct CameraInfo
    {
        long        threadSleepDuration{0};
        wxLongLong  captureStartedTime{0};
        wxULongLong framesCapturedCount{0};
        wxString    cameraCaptureBackendName;
        wxString    cameraAddress;
    };

    enum Commands
    {
        // parameter is CameraInfo
        GetCameraInfo = 0,

        // parameter is long, see CameraThread::m_sleepDuration
        SetThreadSleepDuration,

        // parameter is VCPropCommandParameters
        GetVCProp,
        SetVCProp,
    };

    Commands command;
    wxAny    parameter;
};

typedef wxMessageQueue<CameraCommandData> CameraCommandDatas;

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

    // only for EVT_CAMERA_COMMAND_RESULT
    CameraCommandData GetCommandResult() const { return GetPayload<CameraCommandData>(); }

    wxEvent* Clone() const override { return new CameraEvent(*this); }
protected:
    wxString    m_cameraName;
};

// Camera capture started
// VideoCapture's backend can be retrieved via event's GetString(),
// camera fps can be retrieved via event's GetInt(), if it returns non-zero
wxDECLARE_EVENT(EVT_CAMERA_CAPTURE_STARTED, CameraEvent);
// Result of the CameraCommand::GetXXX command sent to camera, use GetCommandResult()
wxDECLARE_EVENT(EVT_CAMERA_COMMAND_RESULT, CameraEvent);
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
typedef std::vector<CameraFrameDataPtr>  CameraFrameDataPtrs;


/***********************************************************************************************

    CameraSetupData: a struct containing camera setup

***********************************************************************************************/

struct CameraSetupData
{
    // how long the camera thread sleeps after grabbing the frame
    enum
    {
        SleepFromFPS = -1, // = (1000 / camera FPS ) - time taken to process the frame
        SleepNone    =  0  // no sleep in the thread
    };

    wxString             name;
    wxString             address;
    int                  apiPreference{0}; // = cv::CAP_ANY
    long                 sleepDuration{SleepFromFPS}; // either one of Sleep* or time in milliseconds
    int                  FPS{0}; // if 0 do not attempt to set
    int                  defaultFPS{25}; // when the camera FPS cannot be retrieved
    bool                 useMJPGFourCC{false};

	// where to send EVT_CAMERA_xxx events;
    wxEvtHandler*        eventSink{nullptr};
    // new frames captured from camera, to be processed by the GUI thread
	CameraFrameDataPtrs* frames{nullptr};
    wxCriticalSection*   framesCS{nullptr};
    wxSize               frameSize; // if width or height is 0, not set
    wxSize               thumbnailSize;

	// commands send from the GUI thread to camera thread
	CameraCommandDatas*		 commands{nullptr};

    bool IsOk() const;
};



/***********************************************************************************************

    CameraThread: a worker wxThread for retrieving images from a camera with OpenCV
                  and sending them to GUI (a wxEvtHandler*) for display

***********************************************************************************************/

// forward declaration to avoid including OpenCV header
namespace cv { class VideoCapture; }


class CameraThread : public wxThread
{
public:
    CameraThread(const CameraSetupData& cameraSetupData);

    wxString GetCameraAddress() const { return m_cameraSetupData.address; }
    wxString GetCameraName() const    { return m_cameraSetupData.name; }
    bool     IsCapturing() const      { return m_isCapturing; }
protected:
    CameraSetupData                    m_cameraSetupData;

    std::unique_ptr<cv::VideoCapture> m_cameraCapture;
    std::atomic_bool                  m_isCapturing{false};
    wxLongLong                        m_captureStartedTime; // when was capture opened, obtained with wxGetUTCTimeMillis()
    wxULongLong                       m_framesCapturedCount{0};

    ExitCode Entry() override;

    bool InitCapture();
    void SetCameraResolution(const wxSize& resolution);
    void SetCameraUseMJPEG();
    void SetCameraFPS(const int FPS);

	void ProcessCameraCommand(const CameraCommandData& commandData);
};

#endif // #ifndef CAMERATHREAD_H