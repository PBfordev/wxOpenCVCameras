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
        ID_ADD_ALL_KNOWN_CAMERAS = wxID_HIGHEST + 1
    };

    struct CameraView
    {
        CameraThread* thread{nullptr};
        CameraPanel*  thumbnailPanel{nullptr};
    };

    // timer interval for processing new camera frame data from worker threads
    static const long ms_defaultProcessNewCameraFrameDataInterval = 30;

    // camera name to CameraView
    std::map<wxString, CameraView> m_cameras;
    long                           m_processNewCameraFrameDataInterval{ms_defaultProcessNewCameraFrameDataInterval};
    wxTimer                        m_processNewCameraFrameDataTimer;
    CameraFrameDataVector          m_newCameraFrameData;
    wxCriticalSection              m_newCameraFrameDataCS;

    void OnAddCamera(wxCommandEvent&);
    void OnAddAllKnownCameras(wxCommandEvent&);
    void OnRemoveCamera(wxCommandEvent&);
    void OnRemoveAllCameras(wxCommandEvent&);

    void OnShowOneCameraFrame(wxMouseEvent& evt);

    void AddCamera(const wxString& address);
    void RemoveCamera(const wxString& cameraName);
    void RemoveAllCameras();

    void OnProcessNewCameraFrameData(wxTimerEvent&);

    void OnCameraCaptureStarted(CameraEvent& evt);
    void OnCameraErrorOpen(CameraEvent& evt);
    void OnCameraErrorEmpty(CameraEvent& evt);
    void OnCameraErrorException(CameraEvent& evt);

    CameraPanel*    FindThumbnailPanelForCamera(const wxString& cameraName) const;
    OneCameraFrame* FindOneCameraFrameForCamera(const wxString& cameraName) const;
};

#endif // #ifndef CAMERAGRIDFRAME_H