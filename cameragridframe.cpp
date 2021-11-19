///////////////////////////////////////////////////////////////////////////////
// Name:        cameragridframe.cpp
// Purpose:     Displays thumbnails from multiple cameras
// Author:      PB
// Created:     2021-11-18
// Copyright:   (c) 2021 PB
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#include <wx/wx.h>
#include <wx/choicdlg.h>
#include <wx/thread.h>
#include <wx/wupdlock.h>
#include <wx/utils.h>
#include <wx/wrapsizer.h>

#include "cameragridframe.h"
#include "camerapanel.h"
#include "camerathread.h"
#include "convertmattowxbmp.h"
#include "onecameraframe.h"

// some/most are time-limited
const char* const knownCameraAdresses[] =
{
    "http://pendelcam.kip.uni-heidelberg.de/mjpg/video.mjpg",
    "https://www.rmp-streaming.com/media/big-buck-bunny-360p.mp4",
    "rtsp://wowzaec2demo.streamlock.net/vod/mp4:BigBuckBunny_115k.mp4",
    "http://qthttp.apple.com.edgesuite.net/1010qwoeiuryfg/sl.m3u8",
    "http://103.199.161.254/Content/bbcworld/Live/Channel(BBCworld)/index.m3u8",
    "https://c.mjh.nz/101002210221/",
};

CameraGridFrame::CameraGridFrame(wxWindow* parent) : wxFrame(parent, wxID_ANY, "Double click camera thumbnail to show full resolution display")
{
    wxMenu*    cameraMenu = new wxMenu;
    wxMenuBar* menuBar = new wxMenuBar();

    cameraMenu->Append(wxID_NEW,   "&Add...");
    cameraMenu->Append(wxID_FILE1, "Add Pendulum");
    cameraMenu->Append(wxID_FILE2, "Add Bunny 1");
    cameraMenu->Append(wxID_FILE3, "Add Bunny 2");
    cameraMenu->Append(wxID_FILE4, "Add Apple Stream");
    cameraMenu->Append(wxID_FILE5, "Add BBC World");
    cameraMenu->Append(wxID_FILE6, "Add ABC Live");
    cameraMenu->Append(ID_CAMERA_ADD_ALL_KNOWN, "Add All &Known Cameras");
    cameraMenu->AppendSeparator();
    cameraMenu->Append(ID_CAMERA_REMOVE, "&Remove...");
    cameraMenu->Append(ID_CAMERA_REMOVE_ALL, "Remove A&ll");

    menuBar->Append(cameraMenu, "&Camera");
    SetMenuBar(menuBar);

    CreateStatusBar(2);

    SetClientSize(900, 700);

    SetSizer(new wxWrapSizer(wxHORIZONTAL));

    Bind(wxEVT_MENU, &CameraGridFrame::OnAddCamera, this, wxID_NEW);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { AddCamera(knownCameraAdresses[0]); }, wxID_FILE1);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { AddCamera(knownCameraAdresses[1]); }, wxID_FILE2);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { AddCamera(knownCameraAdresses[2]); }, wxID_FILE3);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { AddCamera(knownCameraAdresses[3]); }, wxID_FILE4);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { AddCamera(knownCameraAdresses[4]); }, wxID_FILE5);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { AddCamera(knownCameraAdresses[5]); }, wxID_FILE6);
    Bind(wxEVT_MENU, &CameraGridFrame::OnAddAllKnownCameras, this, ID_CAMERA_ADD_ALL_KNOWN);

    Bind(wxEVT_MENU, &CameraGridFrame::OnRemoveCamera, this, ID_CAMERA_REMOVE);
    Bind(wxEVT_MENU, &CameraGridFrame::OnRemoveAllCameras, this, ID_CAMERA_REMOVE_ALL);

    m_processNewCameraFrameDataTimer.Start(m_processNewCameraFrameDataInterval);
    m_processNewCameraFrameDataTimer.Bind(wxEVT_TIMER, &CameraGridFrame::OnProcessNewCameraFrameData, this);

    Bind(EVT_CAMERA_CAPTURE_STARTED, &CameraGridFrame::OnCameraCaptureStarted, this);
    Bind(EVT_CAMERA_ERROR_OPEN, &CameraGridFrame::OnCameraErrorOpen, this);
    Bind(EVT_CAMERA_ERROR_EMPTY, &CameraGridFrame::OnCameraErrorEmpty, this);
    Bind(EVT_CAMERA_ERROR_EXCEPTION, &CameraGridFrame::OnCameraErrorException, this);

    m_updateInfoTimer.Bind(wxEVT_TIMER, &CameraGridFrame::OnUpdateInfo, this);
    m_updateInfoTimer.Start(1000); // once a second

    wxLog::AddTraceMask(TRACE_WXOPENCVCAMERAS);
}

CameraGridFrame::~CameraGridFrame()
{
    RemoveAllCameras();
}

void CameraGridFrame::OnAddCamera(wxCommandEvent&)
{
    static wxString address;

    address = wxGetTextFromUser("Enter either a camera index as unsigned long or an URL including protocol, address, port etc.",
                                "Camera", address, this);

    if ( !address.empty() )
        AddCamera(address);
}

void CameraGridFrame::OnAddAllKnownCameras(wxCommandEvent&)
{
    wxWindowUpdateLocker locker;

    for ( const auto& address : knownCameraAdresses )
        AddCamera(address);

    Layout();
}

void CameraGridFrame::OnRemoveCamera(wxCommandEvent&)
{
    if ( m_cameras.empty() )
    {
        wxLogMessage("No cameras to remove.");
        return;
    }

    wxArrayString cameraNames;
    wxString      cameraNameToDelete;

    for ( auto& camera : m_cameras )
        cameraNames.push_back(camera.second.thread->GetCameraName());

    cameraNameToDelete = wxGetSingleChoice("Remove camera", "Select camera to remove", cameraNames, this);
    if ( cameraNameToDelete.empty() )
        return;

    RemoveCamera(cameraNameToDelete);
}

void CameraGridFrame::OnRemoveAllCameras(wxCommandEvent&)
{
    RemoveAllCameras();
}

// if a camera thumbnail is doubleclicked, show the camera output
// in a full resolution in its own frame
void CameraGridFrame::OnShowOneCameraFrame(wxMouseEvent& evt)
{
    evt.Skip();

    CameraPanel* cameraPanel = dynamic_cast<CameraPanel*>(evt.GetEventObject());

    wxCHECK_RET(cameraPanel, "in CameraGridFrame::OnShowOneCameraFrame() but event object is not CameraPanel");

    const wxString cameraName = cameraPanel->GetCameraName();

    OneCameraFrame* ocFrame = FindOneCameraFrameForCamera(cameraName);

    if ( ocFrame )
    {
        ocFrame->Raise();
        return;
    }

    if ( cameraPanel->GetStatus() == CameraPanel::Error )
    {
        wxLogMessage("Camera '%s' is in error state.", cameraName);
        return;
    }

    ocFrame = new OneCameraFrame(this, cameraPanel->GetCameraName());
    ocFrame->Show();
}

void CameraGridFrame::OnUpdateInfo(wxTimerEvent&)
{
    static wxULongLong prevFramesProcessed{0};

    size_t camerasCapturing{0};

    for ( const auto& c : m_cameras )
    {
        if ( c.second.thread->IsCapturing() )
            camerasCapturing++;
    }

    SetStatusText(wxString::Format("%zu cameras (%zu capturing)",
        m_cameras.size(), camerasCapturing), 0);

    // This number is not indicative of the maximum possible performance.
    // It depends on how many cameras are there, on their fps and time to sleep in the thread
    // and also on the interval and resolution of m_processNewCameraFrameDataTimer.
    SetStatusText(wxString::Format("%s frames processed by GUI in the last second",
        (m_framesProcessed - prevFramesProcessed).ToString()), 1);

    prevFramesProcessed = m_framesProcessed;
}

void CameraGridFrame::AddCamera(const wxString& address)
{
    const wxSize thumbnailSize = wxSize(320, 180);

    static int newCameraId = 0;

    CameraView cameraView;
    wxString   cameraName = wxString::Format("CAM #%d", newCameraId++);

    cameraView.thread = new CameraThread(address, cameraName, *this, m_newCameraFrameData, m_newCameraFrameDataCS, thumbnailSize);
    cameraView.thumbnailPanel = new CameraPanel(this, cameraName);
    cameraView.thumbnailPanel->SetMinSize(thumbnailSize);
    cameraView.thumbnailPanel->SetMaxSize(thumbnailSize);
    cameraView.thumbnailPanel->Bind(wxEVT_LEFT_DCLICK, &CameraGridFrame::OnShowOneCameraFrame, this);
    GetSizer()->Add(cameraView.thumbnailPanel, wxSizerFlags().Border());
    Layout();

    m_cameras[cameraName] = cameraView;

    if ( cameraView.thread->Run() != wxTHREAD_NO_ERROR )
        wxLogError("Could not create the worker thread needed to retrieve the images from camera '%s'.", cameraName);
}

void CameraGridFrame::RemoveCamera(const wxString& cameraName)
{
    auto it = m_cameras.find(cameraName);

    wxCHECK_RET(it != m_cameras.end(), wxString::Format("Camera '%s' not found, could not be deleted.", cameraName));

    wxLogTrace(TRACE_WXOPENCVCAMERAS, "Removing camera '%s'....", cameraName);
    it->second.thread->Delete();
    delete it->second.thread;

    GetSizer()->Detach(it->second.thumbnailPanel);
    it->second.thumbnailPanel->Destroy();

    OneCameraFrame* ocFrame = FindOneCameraFrameForCamera(cameraName);

    if ( ocFrame )
    {
        ocFrame->Destroy();
        wxLogTrace(TRACE_WXOPENCVCAMERAS, "Closed OneCameraFrame for camera '%s'.", cameraName);
    }

    m_cameras.erase(it);
    Layout();
    wxLogTrace(TRACE_WXOPENCVCAMERAS, "Removed camera '%s'.", cameraName);
}

void CameraGridFrame::RemoveAllCameras()
{
    wxWindowUpdateLocker locker;

    while ( !m_cameras.empty() )
        RemoveCamera(wxString(m_cameras.begin()->first));

    m_cameras.clear();
    Layout();
}

void CameraGridFrame::OnProcessNewCameraFrameData(wxTimerEvent&)
{
    CameraFrameDataVector frameData;
    wxStopWatch           stopWatch;

    stopWatch.Start();
    {
        wxCriticalSectionLocker locker(m_newCameraFrameDataCS);

        if ( m_newCameraFrameData.empty() )
            return;

        frameData = std::move(m_newCameraFrameData);
    }

    for ( const auto& fd : frameData )
    {
        const wxString   cameraName = fd->GetCameraName();
        CameraPanel*     cameraThumbnailPanel = FindThumbnailPanelForCamera(cameraName);
        const wxBitmap*  cameraFrame = fd->GetFrame();
        const wxBitmap*  cameraFrameThumbnail = fd->GetThumbnail();
        // capturedToProcessTime obviously depends on timer interval and resolution
        const wxLongLong capturedToProcessTime = wxGetUTCTimeMillis() - fd->GetCapturedTime();

        if ( !cameraFrame || !cameraFrame->IsOk() )
        {
            wxLogTrace(TRACE_WXOPENCVCAMERAS, "Frame with a null or invalid frame (camera '%s', frame #%s)!",
                cameraName, fd->GetFrameNumber().ToString());
            continue;
        }

        if ( cameraThumbnailPanel )
        {
            if ( cameraFrameThumbnail && cameraFrameThumbnail->IsOk() )
                cameraThumbnailPanel->SetBitmap(*cameraFrameThumbnail);
            else
                cameraThumbnailPanel->SetBitmap(wxBitmap(), CameraPanel::Error);
        }

        OneCameraFrame* ocFrame = FindOneCameraFrameForCamera(cameraName);

        if ( ocFrame )
            ocFrame->SetCameraBitmap(*cameraFrame);

#if 0
        wxLogTrace(TRACE_WXOPENCVCAMERAS, "Frame from camera '%s' for frame #%s with resolution %dx%d took %ld ms from capture to process"
            " (OpenCV times: retrieve %ld ms, convert %ld ms, thumbnail %s ms).",
            cameraName,
            fd->GetFrameNumber().ToString(),
            cameraFrame->GetWidth(), cameraFrame->GetHeight(),
            capturedToProcessTime.ToLong(),
            fd->GetTimeToRetrieve(), fd->GetTimeToConvert(),
            cameraFrameThumbnail ? wxString::Format("%ld", fd->GetTimeToCreateThumbnail()) : "n/a");
#endif
    }

    wxLogTrace(TRACE_WXOPENCVCAMERAS, "Processed %zu new camera frames in %ld ms.", frameData.size(), stopWatch.Time());

    m_framesProcessed += frameData.size();
    frameData.clear();
}

void CameraGridFrame::OnCameraCaptureStarted(CameraEvent& evt)
{
    wxLogTrace(TRACE_WXOPENCVCAMERAS, "Started capturing from camera '%s' (%s fps, address '%s')'.",
        evt.GetCameraName(),
        evt.GetInt() ? wxString::Format("%d", evt.GetInt()) : "n/a",
        evt.GetString());
}

void CameraGridFrame::OnCameraErrorOpen(CameraEvent& evt)
{
    const wxString cameraName = evt.GetCameraName();

    ShowErrorForCamera(cameraName);
    wxLogError("Could not open camera '%s'.",  cameraName);
}

void CameraGridFrame::OnCameraErrorEmpty(CameraEvent& evt)
{
    const wxString cameraName = evt.GetCameraName();

    ShowErrorForCamera(cameraName);
    wxLogError("Connection to camera '%s' lost.", cameraName);
}

void CameraGridFrame::OnCameraErrorException(CameraEvent& evt)
{
    const wxString cameraName = evt.GetCameraName();

    ShowErrorForCamera(cameraName);
    wxLogError("Exception in camera '%s': %s", cameraName, evt.GetString());
}

void CameraGridFrame::ShowErrorForCamera(const wxString& cameraName)
{
    CameraPanel* cameraPanel = FindThumbnailPanelForCamera(cameraName);

    if ( cameraPanel )
        cameraPanel->SetBitmap(wxBitmap(), CameraPanel::Error);

    OneCameraFrame* ocFrame = FindOneCameraFrameForCamera(cameraName);

    if ( ocFrame )
        ocFrame->SetCameraBitmap(wxBitmap(), CameraPanel::Error);
}

CameraPanel* CameraGridFrame::FindThumbnailPanelForCamera(const wxString& cameraName) const
{
    auto it = m_cameras.find(cameraName);

    if ( it == m_cameras.end() )
        return nullptr;

    return it->second.thumbnailPanel;
}

OneCameraFrame* CameraGridFrame::FindOneCameraFrameForCamera(const wxString& cameraName) const
{
    const auto& children = GetChildren();

    for ( auto child : children )
    {
        OneCameraFrame* ocFrame = dynamic_cast<OneCameraFrame*>(child);

        if ( !ocFrame )
            continue;

        if ( ocFrame->GetCameraName().IsSameAs(cameraName) )
            return ocFrame;
    }

    return nullptr;
}