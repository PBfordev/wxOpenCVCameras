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
#include <wx/numdlg.h>
#include <wx/thread.h>
#include <wx/utils.h>
#include <wx/wrapsizer.h>
#include <wx/wupdlock.h>

#include <opencv2/videoio/registry.hpp>

#include "cameragridframe.h"
#include "camerapanel.h"
#include "camerathread.h"
#include "convertmattowxbmp.h"
#include "onecameraframe.h"

// based on wxCHECK_VERSION
#define CHECK_OPENCV_VERSION(major,minor,revision) \
    (CV_VERSION_MAJOR >  (major) || \
    (CV_VERSION_MAJOR == (major) && CV_VERSION_MINOR >  (minor)) || \
    (CV_VERSION_MAJOR == (major) && CV_VERSION_MINOR == (minor) && CV_VERSION_REVISION >= (revision)))


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

CameraGridFrame::CameraGridFrame(wxWindow* parent) : wxFrame(parent, wxID_ANY, "wxOpenCVCameras")
{
    wxMenuBar* menuBar = new wxMenuBar();

    wxMenu* addOrRemovecameraMenu = new wxMenu;

    addOrRemovecameraMenu->Append(ID_CAMERA_ADD_CUSTOM,   "&Add custom...");
    addOrRemovecameraMenu->AppendSeparator();
    addOrRemovecameraMenu->Append(ID_CAMERA_ADD_DEFAULT_WEBCAM, "Add Default &Webcam");
    addOrRemovecameraMenu->AppendSeparator();
    addOrRemovecameraMenu->Append(wxID_FILE1, "Add Pendulum");
    addOrRemovecameraMenu->Append(wxID_FILE2, "Add Bunny 1");
    addOrRemovecameraMenu->Append(wxID_FILE3, "Add Bunny 2");
    addOrRemovecameraMenu->Append(wxID_FILE4, "Add Apple Stream");
    addOrRemovecameraMenu->Append(wxID_FILE5, "Add BBC World");
    addOrRemovecameraMenu->Append(wxID_FILE6, "Add ABC Live");
    addOrRemovecameraMenu->Append(ID_CAMERA_ADD_ALL_IP_ABOVE, "Add All IP Cameras Above\tCtrl+A");
    addOrRemovecameraMenu->AppendSeparator();
    addOrRemovecameraMenu->Append(ID_CAMERA_ADD_DOLBYVISIONHLS, "Add DolbyVision HLS (4k at 60fps)");
    addOrRemovecameraMenu->AppendSeparator();
    addOrRemovecameraMenu->Append(ID_CAMERA_REMOVE, "Remove...");
    addOrRemovecameraMenu->Append(ID_CAMERA_REMOVE_ALL, "&Remove All\tCtrl+R");
    menuBar->Append(addOrRemovecameraMenu, "&Add/Remove Camera");


    wxMenu* defaultCameraSettingsMenu = new wxMenu;
    wxMenu* threadSleepMenu = new wxMenu;

    defaultCameraSettingsMenu->Append(ID_CAMERA_SET_DEFAULTS_BACKEND, "Set Default &Backend...");
    threadSleepMenu->AppendRadioItem(ID_CAMERA_SET_DEFAULTS_THREAD_SLEEP_FROM_FPS, "Based on Camera FPS");
    threadSleepMenu->AppendRadioItem(ID_CAMERA_SET_DEFAULTS_THREAD_SLEEP_NONE, "No Sleep");
    threadSleepMenu->AppendRadioItem(ID_CAMERA_SET_DEFAULTS_THREAD_SLEEP_CUSTOM, "Custom");
    threadSleepMenu->Append(ID_CAMERA_SET_DEFAULTS_THREAD_SLEEP_CUSTOM_SET, "Set Custom Duration...");
    defaultCameraSettingsMenu->AppendSubMenu(threadSleepMenu, "CameraThread Sleep Duration");
    defaultCameraSettingsMenu->Append(ID_CAMERA_SET_DEFAULTS_RESOLUTION, "Resolution...");
    defaultCameraSettingsMenu->Append(ID_CAMERA_SET_DEFAULTS_FPS, "FPS...");
    defaultCameraSettingsMenu->AppendCheckItem(ID_CAMERA_SET_DEFAULTS_USE_MJPEG_FOURCC, "Use MJPEG FourCC");
    defaultCameraSettingsMenu->AppendSeparator();
    defaultCameraSettingsMenu->Append(ID_CAMERA_SET_DEFAULTS_RESET, "&Reset");

    menuBar->Append(defaultCameraSettingsMenu, "&Defaults for New Cameras");

    SetMenuBar(menuBar);

    CreateStatusBar(2);

    SetClientSize(900, 700);

    SetSizer(new wxWrapSizer(wxHORIZONTAL));

    Bind(wxEVT_MENU, &CameraGridFrame::OnAddCamera, this, ID_CAMERA_ADD_CUSTOM);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { AddCamera("0"); }, ID_CAMERA_ADD_DEFAULT_WEBCAM);

    Bind(wxEVT_MENU, [this](wxCommandEvent&) { AddCamera(knownCameraAdresses[0]); }, wxID_FILE1);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { AddCamera(knownCameraAdresses[1]); }, wxID_FILE2);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { AddCamera(knownCameraAdresses[2]); }, wxID_FILE3);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { AddCamera(knownCameraAdresses[3]); }, wxID_FILE4);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { AddCamera(knownCameraAdresses[4]); }, wxID_FILE5);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { AddCamera(knownCameraAdresses[5]); }, wxID_FILE6);
    Bind(wxEVT_MENU, &CameraGridFrame::OnAddAllIPCamerasAbove, this, ID_CAMERA_ADD_ALL_IP_ABOVE);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { AddCamera("http://d3rlna7iyyu8wu.cloudfront.net/DolbyVision_Atmos/profile5_HLS/master.m3u8"); }, ID_CAMERA_ADD_DOLBYVISIONHLS);

    Bind(wxEVT_MENU, &CameraGridFrame::OnRemoveCamera, this, ID_CAMERA_REMOVE);
    Bind(wxEVT_MENU, &CameraGridFrame::OnRemoveAllCameras, this, ID_CAMERA_REMOVE_ALL);

    Bind(wxEVT_MENU, &CameraGridFrame::OnSetCameraDefaultBackend, this, ID_CAMERA_SET_DEFAULTS_BACKEND);
    Bind(wxEVT_MENU, &CameraGridFrame::OnSetCameraDefaultThreadSleepFromFPS, this, ID_CAMERA_SET_DEFAULTS_THREAD_SLEEP_FROM_FPS);
    Bind(wxEVT_MENU, &CameraGridFrame::OnSetCameraDefaultThreadSleepNone, this, ID_CAMERA_SET_DEFAULTS_THREAD_SLEEP_NONE);
    Bind(wxEVT_MENU, &CameraGridFrame::OnSetCameraDefaultThreadSleepCustom, this, ID_CAMERA_SET_DEFAULTS_THREAD_SLEEP_CUSTOM);
    Bind(wxEVT_MENU, &CameraGridFrame::OnSetCameraDefaultThreadSleepCustomSetDuration, this, ID_CAMERA_SET_DEFAULTS_THREAD_SLEEP_CUSTOM_SET);
    Bind(wxEVT_MENU, &CameraGridFrame::OnSetCameraDefaultResolution, this, ID_CAMERA_SET_DEFAULTS_RESOLUTION);
    Bind(wxEVT_MENU, &CameraGridFrame::OnSetCameraDefaultFPS, this, ID_CAMERA_SET_DEFAULTS_FPS);
    Bind(wxEVT_MENU, &CameraGridFrame::OnSetCameraDefaultUseMJPGFourCC, this, ID_CAMERA_SET_DEFAULTS_USE_MJPEG_FOURCC);

    Bind(wxEVT_MENU, &CameraGridFrame::OnCameraDefaultsReset, this, ID_CAMERA_SET_DEFAULTS_RESET);


    m_processNewCameraFrameDataTimer.Start(m_processNewCameraFrameDataInterval);
    m_processNewCameraFrameDataTimer.Bind(wxEVT_TIMER, &CameraGridFrame::OnProcessNewCameraFrameData, this);

    Bind(EVT_CAMERA_CAPTURE_STARTED, &CameraGridFrame::OnCameraCaptureStarted, this);
    Bind(EVT_CAMERA_COMMAND_RESULT, &CameraGridFrame::OnCameraCommandResult, this);
    Bind(EVT_CAMERA_ERROR_OPEN, &CameraGridFrame::OnCameraErrorOpen, this);
    Bind(EVT_CAMERA_ERROR_EMPTY, &CameraGridFrame::OnCameraErrorEmpty, this);
    Bind(EVT_CAMERA_ERROR_EXCEPTION, &CameraGridFrame::OnCameraErrorException, this);

    m_updateInfoTimer.Bind(wxEVT_TIMER, &CameraGridFrame::OnUpdateInfo, this);
    m_updateInfoTimer.Start(1000); // once a second

    wxLog::AddTraceMask(TRACE_WXOPENCVCAMERAS);

    CallAfter([] { wxMessageBox("When a camera thumbnail shows it is receiving, you can:\n (1) Double click it to show the full frame.\n (2) Right click it to communicate with the camera."); } );
}

CameraGridFrame::~CameraGridFrame()
{
    RemoveAllCameras();
}

void CameraGridFrame::OnAddCamera(wxCommandEvent&)
{
    static wxString address;

    address = wxGetTextFromUser("Enter either a camera index as unsigned long or as an URL including protocol, address, port etc.",
                                "Camera", address, this);

    if ( !address.empty() )
        AddCamera(address);
}

void CameraGridFrame::OnAddAllIPCamerasAbove(wxCommandEvent&)
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

    wxArrayString cameras;
    wxArrayInt    camerasToRemove;

    for ( auto& camera : m_cameras )
        cameras.push_back(camera.second.thread->GetCameraName());

    if ( wxGetSelectedChoices(camerasToRemove, "Remove camera(s)",
                              "Select camera(s) to remove", cameras, this) == - 1
        )
    {
        return;
    }

    for ( const auto& cr : camerasToRemove )
        RemoveCamera(cameras[cr]);
}

void CameraGridFrame::OnRemoveAllCameras(wxCommandEvent&)
{
    RemoveAllCameras();
}

void CameraGridFrame::OnSetCameraDefaultBackend(wxCommandEvent&)
{
    using namespace cv;

    const std::vector<VideoCaptureAPIs> vcCameraAPIs = videoio_registry::getCameraBackends();
    const std::vector<VideoCaptureAPIs> vcStreamAPIs = videoio_registry::getStreamBackends();
    wxArrayString APINames;
    std::vector<VideoCaptureAPIs> APIIds;
    int initialSelection = -1;
    int selectedIndex = -1;

    APINames.push_back("<Default>");
    APIIds.push_back(CAP_ANY);

    for ( const auto& api : vcCameraAPIs )
    {
        APINames.push_back(videoio_registry::getBackendName(api));
        APIIds.push_back(api);
    }

    for ( const auto& api : vcStreamAPIs )
    {
        const wxString name(videoio_registry::getBackendName(api));

        if ( APINames.Index(name) != wxNOT_FOUND )
            continue;

        APINames.push_back(name);
        APIIds.push_back(api);
    }

    for ( size_t i = 0; i < APIIds.size(); ++i )
    {
        if ( APIIds[i] == m_defaultCameraBackend )
        {
            initialSelection = i;
            break;
        }
    }

    selectedIndex = wxGetSingleChoiceIndex("Available Backends\n(Camera and Stream)", "Select default VideoCapture backend", APINames, initialSelection, this);

    if ( selectedIndex == -1 )
        return;

    m_defaultCameraBackend = APIIds[selectedIndex];
}


void CameraGridFrame::OnSetCameraDefaultThreadSleepFromFPS(wxCommandEvent&)
{
    m_defaultCameraThreadSleepDuration = CameraSetupData::SleepFromFPS;
}

void CameraGridFrame::OnSetCameraDefaultThreadSleepNone(wxCommandEvent&)
{
    m_defaultCameraThreadSleepDuration = CameraSetupData::SleepNone;
}

void CameraGridFrame::OnSetCameraDefaultThreadSleepCustom(wxCommandEvent&)
{
    m_defaultCameraThreadSleepDuration = m_defaultCameraThreadSleepDurationInMs;
}

void CameraGridFrame::OnSetCameraDefaultThreadSleepCustomSetDuration(wxCommandEvent&)
{
    long duration = wxGetNumberFromUser("Sleep duration in ms", "Number between 5 and 1000",
                                        "Select default CameraThread sleep duration",
                                        m_defaultCameraThreadSleepDurationInMs,
                                        5, 1000, this);

    if ( duration == -1 )
        return;

    m_defaultCameraThreadSleepDurationInMs = duration;
}

void CameraGridFrame::OnSetCameraDefaultResolution(wxCommandEvent&)
{
    static const wxSize resolutions[] =
      { { 320,  240},
        { 640,  480},
        { 800,  600},
        {1024,  576},
        {1280,  720},
        {1920, 1080},
        {2048, 1080},
        {2560, 1440},
        {3840, 2160} };

    int           resolutionIndex = -1;
    wxArrayString resolutionStrings;

    resolutionStrings.push_back("<Default>");
    for ( const auto& r : resolutions )
        resolutionStrings.push_back(wxString::Format("%d x %d", r.GetWidth(), r.GetHeight()));

    if ( m_defaultCameraResolution.GetWidth() == 0 )
    {
        resolutionIndex = 0;
    }
    else
    {
        for ( size_t i = 0; i < WXSIZEOF(resolutions); ++i )
        {
            if ( resolutions[i].GetWidth() == m_defaultCameraResolution.GetWidth()
                 && resolutions[i].GetHeight() == m_defaultCameraResolution.GetHeight() )
            {
                resolutionIndex = i + 1;
                break;
            }
        }
    }

    resolutionIndex = wxGetSingleChoiceIndex("Width x Height", "Select resolution", resolutionStrings, resolutionIndex, this);
    if ( resolutionIndex == -1 )
        return;

    if ( resolutionIndex == 0 )
        m_defaultCameraResolution = wxSize();
    else
        m_defaultCameraResolution = resolutions[resolutionIndex-1];
}

void CameraGridFrame::OnSetCameraDefaultFPS(wxCommandEvent&)
{
    long FPS = wxGetNumberFromUser("Camera FPS (0 = default)", "Number between 0 and 1000",
                                   "Select default camera FPS",
                                    m_defaultCameraFPS,
                                    0, 1000, this);

    if ( FPS == -1 )
        return;

    m_defaultCameraFPS = FPS;
}

void CameraGridFrame::OnSetCameraDefaultUseMJPGFourCC(wxCommandEvent& evt)
{
    m_defaultUseMJPGFourCC = evt.IsChecked();
}

void CameraGridFrame::OnCameraDefaultsReset(wxCommandEvent&)
{
    wxMenuBar* menuBar = GetMenuBar();

    m_defaultCameraBackend = cv::CAP_ANY;
    m_defaultCameraThreadSleepDuration = CameraSetupData::SleepFromFPS;
    m_defaultCameraThreadSleepDurationInMs = 25;
    menuBar->FindItem(ID_CAMERA_SET_DEFAULTS_THREAD_SLEEP_FROM_FPS)->Check();
    m_defaultCameraResolution = wxSize();
    m_defaultCameraFPS = 0;
    m_defaultUseMJPGFourCC = false;
    menuBar->FindItem(ID_CAMERA_SET_DEFAULTS_USE_MJPEG_FOURCC)->Check(false);
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

wxString GetCVPropName(cv::VideoCaptureProperties prop)
{
    using namespace cv;

    wxString name;

    switch ( prop )
    {
        case CAP_PROP_POS_MSEC: name = "POS_MSEC"; break;
        case CAP_PROP_POS_FRAMES: name = "POS_FRAMES"; break;
        case CAP_PROP_POS_AVI_RATIO: name = "POS_AVI_RATIO"; break;
        case CAP_PROP_FRAME_WIDTH: name = "FRAME_WIDTH"; break;
        case CAP_PROP_FRAME_HEIGHT: name = "FRAME_HEIGHT"; break;
        case CAP_PROP_FPS: name = "FPS"; break;
        case CAP_PROP_FOURCC: name = "FOURCC"; break;
        case CAP_PROP_FRAME_COUNT: name = "FRAME_COUNT"; break;
        case CAP_PROP_FORMAT: name = "FORMAT"; break;
        case CAP_PROP_MODE: name = "MODE"; break;
        case CAP_PROP_BRIGHTNESS: name = "BRIGHTNESS"; break;
        case CAP_PROP_CONTRAST: name = "CONTRAST"; break;
        case CAP_PROP_SATURATION: name = "SATURATION"; break;
        case CAP_PROP_HUE: name = "HUE"; break;
        case CAP_PROP_GAIN: name = "GAIN"; break;
        case CAP_PROP_EXPOSURE: name = "EXPOSURE"; break;
        case CAP_PROP_CONVERT_RGB: name = "CONVERT_RGB"; break;
        case CAP_PROP_WHITE_BALANCE_BLUE_U: name = "WHITE_BALANCE_BLUE_U"; break;
        case CAP_PROP_RECTIFICATION: name = "RECTIFICATION"; break;
        case CAP_PROP_MONOCHROME: name = "MONOCHROME"; break;
        case CAP_PROP_SHARPNESS: name = "SHARPNESS"; break;
        case CAP_PROP_AUTO_EXPOSURE: name = "AUTO_EXPOSURE"; break;
        case CAP_PROP_GAMMA: name = "GAMMA"; break;
        case CAP_PROP_TEMPERATURE: name = "TEMPERATURE"; break;
        case CAP_PROP_TRIGGER: name = "TRIGGER"; break;
        case CAP_PROP_TRIGGER_DELAY: name = "TRIGGER_DELAY"; break;
        case CAP_PROP_WHITE_BALANCE_RED_V: name = "WHITE_BALANCE_RED_V"; break;
        case CAP_PROP_ZOOM: name = "ZOOM"; break;
        case CAP_PROP_FOCUS: name = "FOCUS"; break;
        case CAP_PROP_GUID: name = "GUID"; break;
        case CAP_PROP_ISO_SPEED: name = "ISO_SPEED"; break;
        case CAP_PROP_BACKLIGHT: name = "BACKLIGHT"; break;
        case CAP_PROP_PAN: name = "PAN"; break;
        case CAP_PROP_TILT: name = "TILT"; break;
        case CAP_PROP_ROLL: name = "ROLL"; break;
        case CAP_PROP_IRIS: name = "IRIS"; break;
        case CAP_PROP_SETTINGS: name = "SETTINGS"; break;
        case CAP_PROP_BUFFERSIZE: name = "BUFFERSIZE"; break;
        case CAP_PROP_AUTOFOCUS: name = "AUTOFOCUS"; break;
        case CAP_PROP_SAR_NUM: name = "SAR_NUM"; break;
        case CAP_PROP_SAR_DEN: name = "SAR_DEN"; break;
        case CAP_PROP_BACKEND: name = "BACKEND"; break;
        case CAP_PROP_CHANNEL: name = "CHANNEL"; break;
        case CAP_PROP_AUTO_WB: name = "AUTO_WB"; break;
        case CAP_PROP_WB_TEMPERATURE: name = "WB_TEMPERATURE"; break;
        case CAP_PROP_CODEC_PIXEL_FORMAT: name = "CODEC_PIXEL_FORMAT"; break;
#if CHECK_OPENCV_VERSION(4,3,0)
        case CAP_PROP_BITRATE: name = "BITRATE"; break;
#endif
#if CHECK_OPENCV_VERSION(4,5,0)
        case CAP_PROP_ORIENTATION_META: name = "ORIENTATION_META"; break;
        case CAP_PROP_ORIENTATION_AUTO: name = "ORIENTATION_AUTO"; break;
#endif
#if CHECK_OPENCV_VERSION(4,5,2)
        case CAP_PROP_HW_ACCELERATION: name = "HW_ACCELERATION"; break;
        case CAP_PROP_HW_DEVICE: name = "HW_DEVICE"; break;
#endif
#if CHECK_OPENCV_VERSION(4,5,3)
        case CAP_PROP_HW_ACCELERATION_USE_OPENCL: name = "HW_ACCELERATION_USE_OPENCL"; break;
#endif
#if CHECK_OPENCV_VERSION(4,5,4)
        case CAP_PROP_OPEN_TIMEOUT_MSEC: name = "OPEN_TIMEOUT_MSEC"; break;
        case CAP_PROP_READ_TIMEOUT_MSEC: name = "READ_TIMEOUT_MSEC"; break;
        case CAP_PROP_STREAM_OPEN_TIME_USEC: name = "STREAM_OPEN_TIME_USEC"; break;
#endif
#if CHECK_OPENCV_VERSION(4,5,5)
        case CAP_PROP_VIDEO_TOTAL_CHANNELS: name = "VIDEO_TOTAL_CHANNELS"; break;
        case CAP_PROP_VIDEO_STREAM: name = "VIDEO_STREAM"; break;
        case CAP_PROP_AUDIO_STREAM: name = "AUDIO_STREAM"; break;
        case CAP_PROP_AUDIO_POS: name = "AUDIO_POS"; break;
        case CAP_PROP_AUDIO_SHIFT_NSEC: name = "AUDIO_SHIFT_NSEC"; break;
        case CAP_PROP_AUDIO_DATA_DEPTH: name = "AUDIO_DATA_DEPTH"; break;
        case CAP_PROP_AUDIO_SAMPLES_PER_SECOND: name = "AUDIO_SAMPLES_PER_SECOND"; break;
        case CAP_PROP_AUDIO_BASE_INDEX: name = "AUDIO_BASE_INDEX"; break;
        case CAP_PROP_AUDIO_TOTAL_CHANNELS: name = "AUDIO_TOTAL_CHANNELS"; break;
        case CAP_PROP_AUDIO_TOTAL_STREAMS: name = "AUDIO_TOTAL_STREAMS"; break;
        case CAP_PROP_AUDIO_SYNCHRONIZE: name = "AUDIO_SYNCHRONIZE"; break;
        case CAP_PROP_LRF_HAS_KEY_FRAME: name = "LRF_HAS_KEY_FRAME"; break;
        case CAP_PROP_CODEC_EXTRADATA_INDEX: name = "CODEC_EXTRADATA_INDEX"; break;
#endif
    }

    return name;
}

void CameraGridFrame::OnCameraContextMenu(wxContextMenuEvent& evt)
{
    CameraPanel* cameraPanel = dynamic_cast<CameraPanel*>(evt.GetEventObject());

    if ( !cameraPanel )
        cameraPanel = dynamic_cast<CameraPanel*>(wxFindWindowAtPoint(wxGetMousePosition()));

    wxCHECK_RET(cameraPanel, "in CameraGridFrame::OnCameraContextMenu() but could not determine the thumbnail panel");

    auto  it = m_cameras.find(cameraPanel->GetCameraName());

    if ( it == m_cameras.end() || !it->second.thread->IsCapturing() )
        return;


    wxMenu menu;
    int    id = wxID_NONE;

    menu.Append(ID_CAMERA_GET_INFO, "Get Camera Information");
    menu.Append(ID_CAMERA_SET_THREAD_SLEEP_DURATION, "Set Thread Sleep duration...");
    menu.Append(ID_CAMERA_GET_VCPROP, "Get VideoCapture Property...");
    menu.Append(ID_CAMERA_SET_VCPROP, "Set VideoCapture Property...");

    id = cameraPanel->GetPopupMenuSelectionFromUser(menu);
    if ( id == wxID_NONE )
        return;

    CameraCommandData commandData;

    if ( id == ID_CAMERA_GET_INFO )
    {
        commandData.command = CameraCommandData::GetCameraInfo;
        it->second.commandDatas->Post(commandData);
    }
    else if ( id == ID_CAMERA_SET_THREAD_SLEEP_DURATION )
    {
        long duration = wxGetNumberFromUser("Sleep duration in ms", "Number between 0 (no sleep) and 1000",
                                            "Select default CameraThread sleep duration",
                                            m_defaultCameraThreadSleepDurationInMs,
                                            0, 1000, this);

        if ( duration == -1 )
            return;

        commandData.command = CameraCommandData::SetThreadSleepDuration;
        commandData.parameter = duration;
        it->second.commandDatas->Post(commandData);
    }
    else if ( id == ID_CAMERA_GET_VCPROP )
    {
        const int prop = SelectCaptureProperty("Property to Get");

        if ( prop == -1 )
            return;

        CameraCommandData::VCPropCommandParameter  param;
        CameraCommandData::VCPropCommandParameters params;

        commandData.command = CameraCommandData::GetVCProp;

        param.id = prop;
        params.push_back(param);
        commandData.parameter = params;

        it->second.commandDatas->Post(commandData);
    }
    else if ( id == ID_CAMERA_SET_VCPROP )
    {
        const int prop = SelectCaptureProperty("Property to Set");

        if (prop == -1)
            return;

        // wxWidgets does not have a convenience function asking the user for a double
        wxString number = wxGetTextFromUser("Enter property value as double in C locale", "Value", "", this);

        if ( number.empty() )
            return;

        double value;

        if ( !number.ToCDouble(&value) )
        {
            wxLogError("Invalid property value.");
            return;
        }

        CameraCommandData::VCPropCommandParameter  param;
        CameraCommandData::VCPropCommandParameters params;

        commandData.command = CameraCommandData::SetVCProp;

        param.id = prop;
        param.value = value;
        params.push_back(param);
        commandData.parameter = params;

        it->second.commandDatas->Post(commandData);
    }
    else
    {
        wxFAIL_MSG("Invalid command");
    }
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
    // and last but not least on the interval and resolution of m_processNewCameraFrameDataTimer.
    SetStatusText(wxString::Format("%s frames processed by GUI in the last second",
        (m_framesProcessed - prevFramesProcessed).ToString()), 1);

    prevFramesProcessed = m_framesProcessed;
}

void CameraGridFrame::AddCamera(const wxString& address)
{
    const wxSize thumbnailSize = wxSize(320, 180);

    static int newCameraId = 0;

    CameraView      cameraView;
    wxString        cameraName = wxString::Format("CAM #%d", newCameraId++);
    CameraSetupData cameraInitData;

    cameraInitData.name          = cameraName;
    cameraInitData.address       = address;
    cameraInitData.apiPreference = m_defaultCameraBackend;
    cameraInitData.sleepDuration = m_defaultCameraThreadSleepDuration;
    cameraInitData.frameSize     = m_defaultCameraResolution;
    cameraInitData.FPS           = m_defaultCameraFPS;
    cameraInitData.useMJPGFourCC = m_defaultUseMJPGFourCC;

    cameraInitData.eventSink     = this;
    cameraInitData.frames        = &m_newCameraFrameData;
    cameraInitData.framesCS      = &m_newCameraFrameDataCS;
    cameraInitData.thumbnailSize = thumbnailSize;

    cameraInitData.commands      = new CameraCommandDatas;

    cameraView.thread = new CameraThread(cameraInitData);

    cameraView.thumbnailPanel = new CameraPanel(this, cameraName);
    cameraView.thumbnailPanel->SetMinSize(thumbnailSize);
    cameraView.thumbnailPanel->SetMaxSize(thumbnailSize);
    cameraView.thumbnailPanel->Bind(wxEVT_LEFT_DCLICK, &CameraGridFrame::OnShowOneCameraFrame, this);
    cameraView.thumbnailPanel->Bind(wxEVT_CONTEXT_MENU, &CameraGridFrame::OnCameraContextMenu, this);
    GetSizer()->Add(cameraView.thumbnailPanel, wxSizerFlags().Border());
    Layout();

    cameraView.commandDatas = cameraInitData.commands;

    m_cameras[cameraName] = cameraView;

    if ( cameraView.thread->Run() != wxTHREAD_NO_ERROR )
        wxLogError("Could not create the worker thread needed to retrieve the images from camera '%s'.", cameraName);
}

void CameraGridFrame::RemoveCamera(const wxString& cameraName)
{
    auto it = m_cameras.find(cameraName);

    wxCHECK_RET(it != m_cameras.end(), wxString::Format("Camera '%s' not found, could not be deleted.", cameraName));

    wxLogTrace(TRACE_WXOPENCVCAMERAS, "Removing camera '%s'...", cameraName);
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

    delete it->second.commandDatas;

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
    CameraFrameDataPtrs frameData;
    wxStopWatch         stopWatch;

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
        auto             it = m_cameras.find(cameraName);

        if ( it == m_cameras.end() || !it->second.thread->IsCapturing() )
            continue; // ignore yet-unprocessed frames from removed or errored cameras

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

        m_framesProcessed++;

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

    frameData.clear();
}

void CameraGridFrame::OnCameraCaptureStarted(CameraEvent& evt)
{
    wxLogTrace(TRACE_WXOPENCVCAMERAS, "Started capturing from camera '%s' (fps: %s, backend: %s)'.",
        evt.GetCameraName(),
        evt.GetInt() ? wxString::Format("%d", evt.GetInt()) : "n/a",
        evt.GetString());
}

void CameraGridFrame::OnCameraCommandResult(CameraEvent& evt)
{
    const CameraCommandData commandData = evt.GetCommandResult();

    wxString infoMessage;

    if ( commandData.command == CameraCommandData::GetCameraInfo )
    {
        CameraCommandData::CameraInfo cameraInfo;
        wxString                      s;

        commandData.parameter.GetAs(&cameraInfo);

        infoMessage.Printf("CameraInfo for camera '%s':\n", evt.GetCameraName());

        switch ( cameraInfo.threadSleepDuration )
        {
            case CameraSetupData::SleepFromFPS:
                s = "FPS-based";
                break;
            case CameraSetupData::SleepNone:
                s = "None";
                break;
            default:
                s.Printf("%ld", cameraInfo.threadSleepDuration);
        }

        infoMessage += "  Thread sleep duration: " + s + "\n";
        infoMessage += "  Frames captured: " + cameraInfo.framesCapturedCount.ToString()  + "\n";;
        infoMessage += "  Backend name: " + cameraInfo.cameraCaptureBackendName + "\n";
        infoMessage += "  Address: " + cameraInfo.cameraAddress + "\n";
    }
    else if ( commandData.command == CameraCommandData::SetThreadSleepDuration )
    {
        long duration;

        commandData.parameter.GetAs(&duration);

        infoMessage.Printf("Thread sleep duration for camera '%s' was set to %ld:\n.", evt.GetCameraName(), duration);
    }
    else if ( commandData.command == CameraCommandData::GetVCProp )
    {
        CameraCommandData::VCPropCommandParameters params;
        wxString                                   s;

        commandData.parameter.GetAs(&params);

        infoMessage.Printf("Retrieved capture properties from camera '%s':\n", evt.GetCameraName());

        for ( const auto& p : params )
        {
            if ( p.id == cv::CAP_PROP_FOURCC && p.value != 0. )
            {
                const int  fourCCInt   = static_cast<int>(p.value);
                const char fourCCStr[] = {(char)(fourCCInt  & 0XFF),
                    (char)((fourCCInt & 0XFF00) >> 8),
                    (char)((fourCCInt & 0XFF0000) >> 16),
                    (char)((fourCCInt & 0XFF000000) >> 24), 0};

                s = fourCCStr;
            }
            else
            {
                s.Printf("%G", p.value);
            }
            infoMessage += wxString::Format("  %s: %s\n",
                GetCVPropName(static_cast<cv::VideoCaptureProperties>(p.id)), s);
        }

    }
    else if ( commandData.command == CameraCommandData::SetVCProp )
    {
        CameraCommandData::VCPropCommandParameters params;

        commandData.parameter.GetAs(&params);

        infoMessage.Printf("Set capture properties for camera '%s':\n", evt.GetCameraName());

        for ( const auto& p : params )
        {
            infoMessage += wxString::Format("  %s to %G: %s\n",
                GetCVPropName(static_cast<cv::VideoCaptureProperties>(p.id)), p.value,
                p.succeeded ? "Succeeded" : "FAILED");
        }
    }
    else
    {
        wxFAIL_MSG("Invalid command");
    }

    wxLogMessage(infoMessage);
}

void CameraGridFrame::OnCameraErrorOpen(CameraEvent& evt)
{
    const wxString cameraName = evt.GetCameraName();

    ShowErrorForCamera(cameraName, wxString::Format("Could not open camera '%s'.",  cameraName));
}

void CameraGridFrame::OnCameraErrorEmpty(CameraEvent& evt)
{
    const wxString cameraName = evt.GetCameraName();

    ShowErrorForCamera(cameraName, wxString::Format("Connection to camera '%s' lost.", cameraName));
}

void CameraGridFrame::OnCameraErrorException(CameraEvent& evt)
{
    const wxString cameraName = evt.GetCameraName();

    ShowErrorForCamera(cameraName, wxString::Format("Exception in camera '%s': %s", cameraName, evt.GetString()));
}

void CameraGridFrame::ShowErrorForCamera(const wxString& cameraName, const wxString& message)
{
    CameraPanel* cameraPanel = FindThumbnailPanelForCamera(cameraName);

    if ( cameraPanel )
        cameraPanel->SetBitmap(wxBitmap(), CameraPanel::Error);

    OneCameraFrame* ocFrame = FindOneCameraFrameForCamera(cameraName);

    if ( ocFrame )
        ocFrame->SetCameraBitmap(wxBitmap(), CameraPanel::Error);

    wxLogError(message);
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

    for ( const auto child : children )
    {
        OneCameraFrame* ocFrame = dynamic_cast<OneCameraFrame*>(child);

        if ( !ocFrame )
            continue;

        if ( ocFrame->GetCameraName().IsSameAs(cameraName) )
            return ocFrame;
    }

    return nullptr;
}

int CameraGridFrame::SelectCaptureProperty(const wxString& message)
{
    wxArrayString properties;

    for ( int prop = cv::CAP_PROP_POS_MSEC; prop < cv::CV__CAP_PROP_LATEST; ++prop )
    {
        const wxString propName = GetCVPropName(static_cast<cv::VideoCaptureProperties>(prop));

        if ( !propName.empty() )
            properties.push_back(propName);
    }

    int result = wxGetSingleChoiceIndex(message, "Select OpenCV Capture Property", properties, 0, this);

    if ( result > cv::CAP_PROP_ISO_SPEED )
    {
        // hack needed due to missing cv::CAP_PROP_ with value 31 between CAP_PROP_ISO_SPEED and CAP_PROP_BACKLIGHT
        result++;
    }

    return result;
}