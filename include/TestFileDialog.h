/////////////////////////////////////////////////////////////////////////////
// Name:        TestFileDialog.h
// Purpose:     Test hook enabled version of wxFileDialog
// Author:      Chris Wilson (originally Robert Roebling)
// Modified by:
// Created:     2008-08-20
// Copyright:   (c) Robert Roebling, Chris Wilson
// Old-RCS-ID:  $Id: filedlg.h 44027 2006-12-21 19:26:48Z VZ $
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _TESTFILEDIALOG_H
#define _TESTFILEDIALOG_H

#include "wx/filedlg.h"

class TestFileDialog: public wxFileDialog
{
public:
	TestFileDialog(wxWindow *parent,
		const wxString& message = wxFileSelectorPromptStr,
		const wxString& defaultDir = wxEmptyString,
		const wxString& defaultFile = wxEmptyString,
		const wxString& wildCard = wxFileSelectorDefaultWildcardStr,
		#if wxCHECK_VERSION(2,8,0)
			long style = wxFD_DEFAULT_STYLE
		#else
			long style = wxOPEN
		#endif
		/*
		const wxPoint& pos = wxDefaultPosition,
		const wxSize& sz = wxDefaultSize,
		const wxString& name = wxFileDialogNameStr
		*/
		)
	: wxFileDialog(parent, message, defaultDir, defaultFile, wildCard,
		style /*pos, sz, name*/)
	{ }

	virtual void SetPath(const wxString& path)
	{
		m_path = path;
		this->wxFileDialog::SetPath(path);
	}
	
	virtual void SetDirectory(const wxString& dir)
	{
		m_dir = dir;
		this->wxFileDialog::SetDirectory(dir);
	}
	
	virtual void SetFilename(const wxString& name)
	{
		m_fileName = name;
		this->wxFileDialog::SetFilename(name);
	}
	
	virtual wxString GetPath() const
	{
		if (wxGetApp().IsTestMode())
		{
			return m_path;
		}
		else
		{
			return this->wxFileDialog::GetPath();
		}
	}

	// virtual void GetPaths(wxArrayString& paths) const { paths.Empty(); paths.Add(m_path); }
	
	virtual wxString GetDirectory() const
	{
		if (wxGetApp().IsTestMode())
		{
			return m_dir;
		}
		else
		{
			return this->wxFileDialog::GetDirectory();
		}
	}

	virtual wxString GetFilename() const
	{
		if (wxGetApp().IsTestMode())
		{
			return m_fileName;
		}
		else
		{
			return this->wxFileDialog::GetFilename();
		}
	}

	// DECLARE_DYNAMIC_CLASS(TestFileDialog)
	DECLARE_NO_COPY_CLASS(TestFileDialog)
};

#endif // _TESTFILEDIALOG_H
