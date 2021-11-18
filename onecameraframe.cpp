///////////////////////////////////////////////////////////////////////////////
// Name:        onecameraframe.cpp
// Purpose:     Displays full resolution output from a single camera
// Author:      PB
// Created:     2021-11-18
// Copyright:   (c) 2021 PB
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////


#include "onecameraframe.h"

OneCameraFrame::OneCameraFrame(wxWindow* parent, const wxString& cameraName)
	: wxFrame(parent, wxID_ANY, cameraName)
{
	m_cameraPanel = new CameraPanel(this, cameraName);
	m_cameraPanel->SetMinSize(wxSize(640, 400));
	m_cameraPanel->SetMaxSize(wxSize(640, 400));
}

void OneCameraFrame::SetCameraBitmap(const wxBitmap& bitmap, CameraPanel::Status status)
{
	m_cameraPanel->SetBitmap(bitmap, status);

	if ( bitmap.IsOk() && !m_clientSizeAdjusted )
	{
		m_cameraPanel->SetMinSize(bitmap.GetSize());
		m_cameraPanel->SetMaxSize(bitmap.GetSize());
		SetClientSize(bitmap.GetSize());
		m_clientSizeAdjusted = true;
	}
}

