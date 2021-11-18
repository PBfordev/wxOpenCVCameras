///////////////////////////////////////////////////////////////////////////////
// Name:        camerapp.cpp
// Purpose:     Application class, just creates and shows the main frame
// Author:      PB
// Created:     2021-11-18
// Copyright:   (c) 2021 PB
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#include <wx/wx.h>

#include "cameragridframe.h"

class wxOpenCVCamerasApp : public wxApp
{
public:
    bool OnInit() override
    {
        (new CameraGridFrame)->Show();
        return true;
    }
}; wxIMPLEMENT_APP(wxOpenCVCamerasApp);