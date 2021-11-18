///////////////////////////////////////////////////////////////////////////////
// Name:        onecameraframe.h
// Purpose:     Displays full resolution output from a single camera
// Author:      PB
// Created:     2021-11-18
// Copyright:   (c) 2021 PB
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef ONECAMERAFRAME_H
#define ONECAMERAFRAME_H

#include <wx/wx.h>

#include "camerapanel.h"

class OneCameraFrame : public wxFrame
{
public:
    OneCameraFrame(wxWindow* parent, const wxString& cameraName);

    void SetCameraBitmap(const wxBitmap& bitmap, CameraPanel::Status status = CameraPanel::Receiving);

    wxString GetCameraName() const { return m_cameraPanel->GetCameraName(); }

private:
    bool         m_clientSizeAdjusted{false};
    CameraPanel* m_cameraPanel{nullptr};
};

#endif // #ifndef ONECAMERAFRAME_H