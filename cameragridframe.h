///////////////////////////////////////////////////////////////////////////////
// Name:        cameragridframe.cpp
// Purpose:     Displays thumbnails from multiple cameras
// Author:      PB
// Created:     2021-11-18
// Copyright:   (c) 2021 PB
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef CAMERAGRIDFRAME_H
#define CAMERAGRIDFRAME_H

#include <wx/wx.h>

#include <map>

#include "camerathread.h"

// forward declarations
class CameraPanel;
class OneCameraFrame;

class CameraGridFrame : public wxFrame
{
public:
    CameraGridFrame(wxWindow* parent = nullptr);
    ~CameraGridFrame();
private:
    enum
    {
        ID_CAMERA_ADD_CUSTOM = wxID_HIGHEST + 1,
        ID_CAMERA_ADD_DEFAULT_WEBCAM,

        ID_CAMERA_ADD_ALL_IP_ABOVE,

        ID_CAMERA_ADD_ALL_DOLBYVISIONHLS,

        ID_CAMERA_REMOVE,
        ID_CAMERA_REMOVE_ALL,

        ID_CAMERA_SET_DEFAULTS_BACKEND,
        ID_CAMERA_SET_DEFAULTS_THREAD_SLEEP_FROM_FPS,
        ID_CAMERA_SET_DEFAULTS_THREAD_SLEEP_NONE,
        ID_CAMERA_SET_DEFAULTS_THREAD_SLEEP_CUSTOM,
        ID_CAMERA_SET_DEFAULTS_THREAD_SLEEP_CUSTOM_SET,
        ID_CAMERA_SET_DEFAULTS_RESOLUTION,
        ID_CAMERA_SET_DEFAULTS_FPS,
        ID_CAMERA_SET_DEFAULTS_USE_MJPEG_FOURCC,
        ID_CAMERA_SET_DEFAULTS_RESET,

        ID_CAMERA_GET_INFO,
        ID_CAMERA_SET_THREAD_SLEEP_DURATION,
        ID_CAMERA_GET_VCPROP,
        ID_CAMERA_SET_VCPROP,
    };

    struct CameraView
    {
        CameraThread*   thread{nullptr};
        CameraPanel*    thumbnailPanel{nullptr};
		CameraCommandDatas* commands{nullptr};
    };

    // default timer interval in ms for processing new camera frame data from worker threads
    // see m_processNewCameraFrameDataInterval
    static const long ms_defaultProcessNewCameraFrameDataInterval = 30;

    // camera name to CameraView
    std::map<wxString, CameraView> m_cameras;
    long                           m_processNewCameraFrameDataInterval{ms_defaultProcessNewCameraFrameDataInterval};
    wxTimer                        m_processNewCameraFrameDataTimer;
    CameraFrameDataPtrs            m_newCameraFrameData;
    wxCriticalSection              m_newCameraFrameDataCS;

    long                           m_defaultCameraBackend{0};
    long                           m_defaultCameraThreadSleepDuration{CameraSetupData::SleepFromFPS};
    long                           m_defaultCameraThreadSleepDurationInMs{25}; // used for custom sleep duration
    wxSize                         m_defaultCameraResolution;
    int                            m_defaultCameraFPS{0};
    bool                           m_defaultUseMJPGFourCC{false};

    wxTimer                        m_updateInfoTimer;
    wxULongLong                    m_framesProcessed{0};

    void OnAddCamera(wxCommandEvent&);
    void OnAddAllIPCamerasAbove(wxCommandEvent&);
    void OnRemoveCamera(wxCommandEvent&);
    void OnRemoveAllCameras(wxCommandEvent&);

    void OnSetCameraDefaultBackend(wxCommandEvent&);
    void OnSetCameraDefaultThreadSleepFromFPS(wxCommandEvent&);
    void OnSetCameraDefaultThreadSleepNone(wxCommandEvent&);
    void OnSetCameraDefaultThreadSleepCustom(wxCommandEvent&);
    void OnSetCameraDefaultThreadSleepCustomSetDuration(wxCommandEvent&);
    void OnSetCameraDefaultResolution(wxCommandEvent&);
    void OnSetCameraDefaultFPS(wxCommandEvent&);
    void OnSetCameraDefaultUseMJPGFourCC(wxCommandEvent& evt);
    void OnCameraDefaultsReset(wxCommandEvent&);

    void OnShowOneCameraFrame(wxMouseEvent& evt);

    void OnCameraContextMenu(wxContextMenuEvent& evt);

    void OnUpdateInfo(wxTimerEvent&);

    void AddCamera(const wxString& address);
    void RemoveCamera(const wxString& cameraName);
    void RemoveAllCameras();

    void OnProcessNewCameraFrameData(wxTimerEvent&);

    void OnCameraCaptureStarted(CameraEvent& evt);

    void OnCameraCommandResult(CameraEvent& evt);

    void OnCameraErrorOpen(CameraEvent& evt);
    void OnCameraErrorEmpty(CameraEvent& evt);
    void OnCameraErrorException(CameraEvent& evt);

    void ShowErrorForCamera(const wxString& cameraName, const wxString& message);

    CameraPanel*    FindThumbnailPanelForCamera(const wxString& cameraName) const;
    OneCameraFrame* FindOneCameraFrameForCamera(const wxString& cameraName) const;

    int SelectCaptureProperty(const wxString& message);
};

#endif // #ifndef CAMERAGRIDFRAME_H