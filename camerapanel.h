///////////////////////////////////////////////////////////////////////////////
// Name:        camerapanel.h
// Purpose:     Displays a bitmap and camera status
// Author:      PB
// Created:     2021-11-18
// Copyright:   (c) 2021 PB
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////


#ifndef CAMERAPANEL_H
#define CAMERAPANEL_H

#include <wx/wx.h>

class CameraPanel : public wxPanel
{
public:
    enum Status { Connecting, Receiving, Error };

    CameraPanel(wxWindow* parent, const wxString& cameraName, Status status = Connecting);

    void SetBitmap(const wxBitmap& bitmap, Status status = Receiving);

    wxString GetCameraName() const { return m_cameraName; }
    Status   GetStatus() const     { return m_status; }
private:
    wxBitmap m_bitmap;
    wxString m_cameraName;
    Status   m_status{Connecting};

    void OnPaint(wxPaintEvent&);
};

#endif // #ifndef CAMERAPANEL_H