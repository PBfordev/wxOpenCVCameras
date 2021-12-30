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
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    Bind(wxEVT_PAINT, &CameraPanel::OnPaint, this);
}

void CameraPanel::SetBitmap(const wxBitmap& bitmap, Status status)
{
    m_bitmap = bitmap;
    m_status = status;

    Refresh(); Update();
}

// On MSW, displaying 4k bitmaps from 60 fps camera with
// wx(Auto)BufferedPaintDC in some scenarios meant the application
// after while started for some reason lagging very badly,
// even unable to process the camera frames. This did not happen
// when using wxPaintDC instead. However, drawing the same way with
// wxPaintDC instead of wx(Auto)BufferedPaintDC meant the panels were flashing.
// In the end, the old-fashioned way with wxMemoryDC
// is used, even if it means that drawing cannot be timed (easily).

void CameraPanel::OnPaint(wxPaintEvent&)
{
    wxDC* paintDC{nullptr};

#if CAMERAPANEL_USE_AUTOBUFFEREDPAINTDC
    wxAutoBufferedPaintDC dc(this);
    wxStopWatch           stopWatch;

    paintDC = &dc;
    stopWatch.Start();
#else
    const wxSize clientSize(GetClientSize());

    wxPaintDC dc(this);

    if ( clientSize.GetWidth() < 1 || clientSize.GetHeight() < 1 )
        return;

    wxBitmap   memDCBitmap(clientSize);
    wxMemoryDC memDC(&dc);

    memDC.SelectObject(memDCBitmap);
    paintDC = &memDC;
#endif

    wxString    statusString;
    wxColour    statusColor(*wxBLUE);

    paintDC->SetBackground(*wxBLACK_BRUSH);
    paintDC->Clear();

    if ( m_bitmap.IsOk() )
        paintDC->DrawBitmap(m_bitmap, 0, 0, false);

    switch ( m_status )
    {
        case Connecting:
            statusString = "Connecting";
            statusColor  = *wxBLUE;
            break;
        case Receiving:
            statusString = "Receiving";
            statusColor  = *wxGREEN;
            break;
        case Error:
            statusString = "ERROR";
            statusColor  = *wxRED;
            break;
    }

    wxDCTextColourChanger tcChanger(*paintDC, statusColor);
    wxString              infoText(wxString::Format("%s: %s", m_cameraName, statusString));

#if CAMERAPANEL_USE_AUTOBUFFEREDPAINTDC
    if ( m_drawPaintTime && m_status == Receiving )
        infoText.Printf("%s\nFrame painted in %ld ms", infoText, stopWatch.Time());
#endif

    paintDC->DrawText(infoText, 5, 5);

#if !CAMERAPANEL_USE_AUTOBUFFEREDPAINTDC
    dc.Blit(wxPoint(0, 0), clientSize, &memDC, wxPoint(0, 0));
#endif
}