///////////////////////////////////////////////////////////////////////////////
// Name:        camerapanel.cpp
// Purpose:     Displays a bitmap and camera status
// Author:      PB
// Created:     2021-11-18
// Copyright:   (c) 2021 PB
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////


#include <wx/wx.h>
#include <wx/dcbuffer.h>

#include "camerapanel.h"

CameraPanel::CameraPanel(wxWindow* parent, const wxString& cameraName,
                        bool drawPaintTime, Status status)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxFULL_REPAINT_ON_RESIZE | wxBORDER_RAISED),
      m_cameraName(cameraName), m_drawPaintTime(drawPaintTime), m_status(status)
{
    SetBackgroundColour(*wxBLACK);
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    Bind(wxEVT_PAINT, &CameraPanel::OnPaint, this);
}

void CameraPanel::SetBitmap(const wxBitmap& bitmap, Status status)
{
    m_bitmap = bitmap;
    m_status = status;

    Refresh(); Update();
}

void CameraPanel::OnPaint(wxPaintEvent&)
{
    wxAutoBufferedPaintDC dc(this);
    wxString              statusString;
    wxColour              statusColor(*wxBLUE);
    wxStopWatch           stopWatch;

    stopWatch.Start();
    dc.Clear();

    if ( m_bitmap.IsOk() )
        dc.DrawBitmap(m_bitmap, 0, 0, false);

    switch ( m_status )
    {
        case Connecting:
            statusString = "Connecting";
            statusColor = *wxBLUE;
            break;
        case Receiving:
            statusString = "Receiving";
            statusColor = *wxGREEN;
            break;
        case Error:
            statusString = "ERROR";
            statusColor = *wxRED;
            break;
    }

    wxDCTextColourChanger tcChanger(dc, statusColor);
    wxString              infoText(wxString::Format("%s: %s", m_cameraName, statusString));

    if ( m_drawPaintTime )
        infoText.Printf("%s\nFrame painted in %ld ms", infoText, stopWatch.Time());

    dc.DrawText(infoText, 5, 5);
}