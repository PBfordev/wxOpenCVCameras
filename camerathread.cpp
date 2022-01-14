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
wxDEFINE_EVENT(EVT_CAMERA_COMMAND_RESULT, CameraEvent);
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

    CameraInitData

***********************************************************************************************/


bool CameraSetupData::IsOk() const
{
    return  !name.empty()
            && !address.empty()
            && defaultFPS > 0
            && eventSink
            && frames && framesCS
            && frameSize.GetWidth() >= 0 && frameSize.GetHeight() >= 0
            && commands;
}

/***********************************************************************************************

    CameraThread

***********************************************************************************************/

CameraThread::CameraThread(const CameraSetupData& cameraSetupData)
    : wxThread(wxTHREAD_JOINABLE),
      m_cameraSetupData(cameraSetupData)
{
    wxCHECK_RET(m_cameraSetupData.IsOk(), "Invalid camera initialization data");
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
        m_cameraSetupData.eventSink->QueueEvent(new CameraEvent(EVT_CAMERA_ERROR_OPEN, GetCameraName()));
        return static_cast<wxThread::ExitCode>(nullptr);
    }

    const bool createThumbnail = m_cameraSetupData.thumbnailSize.GetWidth() > 0 && m_cameraSetupData.thumbnailSize.GetHeight() > 0;

    CameraEvent* evt{nullptr};
    cv::Mat      matFrame;
    wxStopWatch  stopWatch;
    long         msPerFrame;

    m_captureStartedTime = wxGetUTCTimeMillis();
    m_isCapturing = true;

    if ( m_cameraSetupData.frameSize.GetWidth() > 0 )
        SetCameraResolution(m_cameraSetupData.frameSize);
    if ( m_cameraSetupData.useMJPGFourCC )
        SetCameraUseMJPEG();
    if ( m_cameraSetupData.FPS > 0 )
        SetCameraFPS(m_cameraSetupData.FPS);

    m_cameraSetupData.FPS = m_cameraCapture->get(static_cast<int>(cv::CAP_PROP_FPS));

    evt = new CameraEvent(EVT_CAMERA_CAPTURE_STARTED, GetCameraName());
    evt->SetString(wxString(m_cameraCapture->getBackendName()));
    evt->SetInt(m_cameraSetupData.FPS);
    m_cameraSetupData.eventSink->QueueEvent(evt);


    while ( !TestDestroy() )
    {
        try
        {
            CameraFrameDataPtr frameData(new CameraFrameData(GetCameraName(), m_framesCapturedCount++));
            wxLongLong         frameCaptureStartedTime;
            CameraCommandData  commandData;

            frameCaptureStartedTime = wxGetUTCTimeMillis();

            if ( m_cameraSetupData.commands->ReceiveTimeout(0, commandData) == wxMSGQUEUE_NO_ERROR )
                ProcessCameraCommand(commandData);

            if ( m_cameraSetupData.FPS > 0 )
                msPerFrame = 1000 / m_cameraSetupData.FPS;
            else
                msPerFrame = 1000 / m_cameraSetupData.defaultFPS;

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
                    cv::resize(matFrame, matThumbnail, cv::Size(m_cameraSetupData.thumbnailSize.GetWidth(), m_cameraSetupData.thumbnailSize.GetHeight()));
                    frameData->SetThumbnail(new wxBitmap(m_cameraSetupData.thumbnailSize, 24));
                    ConvertMatBitmapTowxBitmap(matThumbnail, *frameData->GetThumbnail());
                    frameData->SetTimeToCreateThumbnail(stopWatch.Time());
                }

                {
                    wxCriticalSectionLocker locker(*m_cameraSetupData.framesCS);

                    m_cameraSetupData.frames->push_back(std::move(frameData));
                }

                if ( m_cameraSetupData.sleepDuration == CameraSetupData::SleepFromFPS )
                {
                    const wxLongLong timeSinceFrameCaptureStarted = wxGetUTCTimeMillis() - frameCaptureStartedTime;
                    const long       timeToSleep = msPerFrame - timeSinceFrameCaptureStarted.GetLo();

                    // exact time slept depends among else on the resolution of the system clock
                    // for example, for MSW see Remarks in the ::Sleep() documentation at https://docs.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-sleep
                    if ( timeToSleep > 0 )
                        Sleep(timeToSleep);
                }
                else if ( m_cameraSetupData.sleepDuration == CameraSetupData::SleepNone )
                {
                    continue;
                }
                else if ( m_cameraSetupData.sleepDuration > 0 )
                {
                    Sleep(m_cameraSetupData.sleepDuration);
                }
                else
                {
                    wxLogDebug("Invalid sleep duration %d", m_cameraSetupData.sleepDuration);
                }
            }
            else // connection to camera lost
            {
                m_isCapturing = false;
                m_cameraSetupData.eventSink->QueueEvent(new CameraEvent(EVT_CAMERA_ERROR_EMPTY, GetCameraName()));
                break;
            }
        }
        catch ( const std::exception& e )
        {
            m_isCapturing = false;

            evt = new CameraEvent(EVT_CAMERA_ERROR_EXCEPTION, GetCameraName());
            evt->SetString(e.what());
            m_cameraSetupData.eventSink->QueueEvent(evt);

            break;
        }
        catch ( ... )
        {
            m_isCapturing = false;

            evt = new CameraEvent(EVT_CAMERA_ERROR_EXCEPTION, GetCameraName());
            evt->SetString("Unknown exception");
            m_cameraSetupData.eventSink->QueueEvent(evt);

            break;
        }
    }

    wxLogTrace(TRACE_WXOPENCVCAMERAS, "Exiting CameraThread for camera '%s'...", GetCameraName());
    return static_cast<wxThread::ExitCode>(nullptr);
}


bool CameraThread::InitCapture()
{
    unsigned long cameraIndex = 0;

    if ( m_cameraSetupData.address.ToCULong(&cameraIndex) )
        m_cameraCapture.reset(new cv::VideoCapture(cameraIndex, m_cameraSetupData.apiPreference));
    else
        m_cameraCapture.reset(new cv::VideoCapture(m_cameraSetupData.address.ToStdString(), m_cameraSetupData.apiPreference));

    return m_cameraCapture->isOpened();
}

void CameraThread::SetCameraResolution(const wxSize& resolution)
{
    if ( m_cameraCapture->set(cv::CAP_PROP_FRAME_WIDTH, resolution.GetWidth()) )
        wxLogTrace(TRACE_WXOPENCVCAMERAS, "Set frame width to %d for camera '%s'", resolution.GetWidth(), GetCameraName());
    else
        wxLogTrace(TRACE_WXOPENCVCAMERAS, "Could not set frame width to %d for camera '%s'", resolution.GetWidth(), GetCameraName());

    if ( m_cameraCapture->set(cv::CAP_PROP_FRAME_HEIGHT, resolution.GetHeight()) )
        wxLogTrace(TRACE_WXOPENCVCAMERAS, "Set frame height to %d for camera '%s'", resolution.GetHeight(), GetCameraName());
    else
        wxLogTrace(TRACE_WXOPENCVCAMERAS, "Could not set frame height to %d for camera '%s'", resolution.GetHeight(), GetCameraName());
}

void CameraThread::SetCameraUseMJPEG()
{
    if ( m_cameraCapture->set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G')) )
        wxLogTrace(TRACE_WXOPENCVCAMERAS, "Set FourCC to 'MJPG' for camera '%s'", GetCameraName());
    else
        wxLogTrace(TRACE_WXOPENCVCAMERAS, "Could not set FourCC to 'MJPG' for camera '%s'", GetCameraName());
}

void CameraThread::SetCameraFPS(const int FPS)
{
    if ( m_cameraCapture->set(cv::CAP_PROP_FPS, FPS) )
        wxLogTrace(TRACE_WXOPENCVCAMERAS, "Set FPS to %d for camera '%s'", FPS, GetCameraName());
    else
        wxLogTrace(TRACE_WXOPENCVCAMERAS, "Could not set FPS to %d for camera '%s'", FPS, GetCameraName());
}


void CameraThread::ProcessCameraCommand(const CameraCommandData& commandData)
{
    CameraEvent*      evt = new CameraEvent(EVT_CAMERA_COMMAND_RESULT, GetCameraName());
    CameraCommandData evtCommandData;

    evtCommandData.command = commandData.command;

    if ( commandData.command == CameraCommandData::GetCameraInfo )
    {
        CameraCommandData::CameraInfo cameraInfo;

        cameraInfo.threadSleepDuration      = m_cameraSetupData.sleepDuration;
        cameraInfo.captureStartedTime       = m_captureStartedTime;
        cameraInfo.framesCapturedCount      = m_framesCapturedCount;
        cameraInfo.cameraCaptureBackendName = m_cameraCapture->getBackendName();
        cameraInfo.cameraAddress            = m_cameraSetupData.address;

        evtCommandData.parameter = cameraInfo;
    }
    else if ( commandData.command == CameraCommandData::SetThreadSleepDuration )
    {
        m_cameraSetupData.sleepDuration = commandData.parameter.As<long>();

        evtCommandData.parameter = m_cameraSetupData.sleepDuration;
    }
    else if ( commandData.command == CameraCommandData::GetVCProp )
    {
        const CameraCommandData::VCPropCommandParameters params = commandData.parameter.As<CameraCommandData::VCPropCommandParameters>();
        CameraCommandData::VCPropCommandParameters       evtParams;

        for ( const auto& p: params )
        {
            CameraCommandData::VCPropCommandParameter evtParam;

            evtParam.id = p.id;
            evtParam.value = m_cameraCapture->get(p.id);
            evtParams.push_back(evtParam);
        }
        evtCommandData.parameter = evtParams;
    }
    else if ( commandData.command == CameraCommandData::SetVCProp )
    {
        const CameraCommandData::VCPropCommandParameters params = commandData.parameter.As<CameraCommandData::VCPropCommandParameters>();
        CameraCommandData::VCPropCommandParameters       evtParams;

        for ( const auto& p: params )
        {
            CameraCommandData::VCPropCommandParameter evtParam;

            evtParam.id = p.id;
            evtParam.value = p.value;
            evtParam.succeeded = m_cameraCapture->set(p.id, p.value);
            evtParams.push_back(evtParam);
        }
        evtCommandData.parameter = evtParams;
    }
    else
    {
        delete evt;
        wxFAIL_MSG("Invalid command");
        return;
    }

    evt->SetPayload(evtCommandData);
    m_cameraSetupData.eventSink->QueueEvent(evt);
}