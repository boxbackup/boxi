/***************************************************************************
 *            ParamPanel.cc
 *
 *  Sun May 15 15:18:40 2005
 *  Copyright 2005-2006 Chris Wilson
 *  Email <chris-boxisource@qwirx.com>
 ****************************************************************************/

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "SandBox.h"

#include <wx/artprov.h>
#include <wx/filename.h>
#include <wx/image.h>

#include "ParamPanel.h"
#include "main.h"

BEGIN_EVENT_TABLE(BoundStringCtrl, wxTextCtrl)
	EVT_TEXT(wxID_ANY, BoundStringCtrl::OnTextChanged)
END_EVENT_TABLE()

void BoundStringCtrl::Reload() 
{
	const std::string * pValue = mrStringProp.GetPointer();
	if (pValue) 
	{
		wxString valueString(pValue->c_str(), wxConvBoxi);
		SetValue(valueString);
		// work around Windows bug
		mrStringProp.Set(pValue->c_str());
	}
	else
	{
		SetValue(wxT(""));
		mrStringProp.Clear();
	}
}

void BoundStringCtrl::OnChange()
{
	wxString tempString = GetValue();
	wxCharBuffer buf = tempString.mb_str(wxConvBoxi);
	if (tempString.Length() == 0) {
		mrStringProp.Clear();
	} else {
		mrStringProp.Set(buf.data());
	}
}

BEGIN_EVENT_TABLE(IntCtrl, wxTextCtrl)
	EVT_KILL_FOCUS(IntCtrl::OnFocusLost)
	EVT_TEXT(wxID_ANY, IntCtrl::OnTextChanged)
END_EVENT_TABLE()
		
void IntCtrl::Reload()
{
	int value = mValue;
	wxString ValueString;
	ValueString.Printf(
		wxString(mFormat.c_str(), wxConvBoxi), 
		mValue);
	SetValue(ValueString);
	// work around Windows bug
	mValue = value;
}

void IntCtrl::OnChange()
{
	mIsValid = FALSE;
	
	wxString tempString = GetValue();
	if (tempString.Length() == 0) 
	{
		return;
	}
	
	unsigned int tempValue;
	char *endptr;

	wxCharBuffer buf = tempString.mb_str(wxConvBoxi);
	
	if (tempString.StartsWith(wxT("0x"))) 
	{
		tempValue = strtol(buf.data() + 2, &endptr, 16);
	} 
	else 
	{
		tempValue = strtol(buf.data(), &endptr, 10);
	}
	
	if (*endptr != '\0') {
		return;
	}

	mValue = tempValue;
	mIsValid = TRUE;
}

BEGIN_EVENT_TABLE(BoundIntCtrl, wxTextCtrl)
	EVT_KILL_FOCUS(BoundIntCtrl::OnFocusLost)
	EVT_TEXT(wxID_ANY, BoundIntCtrl::OnTextChanged)
END_EVENT_TABLE()
		
void BoundIntCtrl::Reload()
{
	const int* ValuePtr = mrIntProp.GetPointer();
	if (ValuePtr)
	{
		wxString ValueString;
		ValueString.Printf(wxString(mFormat.c_str(), wxConvBoxi), 
			*ValuePtr);
		int ValueInt = *ValuePtr;
		SetValue(ValueString);
		// work around Windows bug
		mrIntProp.Set(ValueInt);
	}
	else
	{
		SetValue(wxT(""));
		mrIntProp.Clear();
	}
}

void BoundIntCtrl::OnChange()
{
	wxString tempString = GetValue();
	if (tempString.Length() == 0) {
		mrIntProp.Clear();
		return;
	}
	
	unsigned int tempValue;
	char *endptr;

	wxCharBuffer buf = tempString.mb_str(wxConvBoxi);
	
	if (tempString.StartsWith(wxT("0x"))) {
		tempValue = strtol(buf.data() + 2, &endptr, 16);
	} else {
		tempValue = strtol(buf.data(), &endptr, 10);
	}
	
	if (*endptr != '\0') {
		return;
	}

	mrIntProp.Set(tempValue);
}

BEGIN_EVENT_TABLE(BoundHexCtrl, wxTextCtrl)
	EVT_KILL_FOCUS(BoundHexCtrl::OnFocusLost)
	EVT_TEXT(wxID_ANY, BoundHexCtrl::OnTextChanged)
END_EVENT_TABLE()
		
void BoundHexCtrl::Reload()
{
	if (!mIsValid)
	{
		// don't replace displayed, invalid value with out-of-date
		// value from underlying property
		return;
	}

	const int* ValuePtr = mrIntProp.GetPointer();

	if (ValuePtr)
	{
		wxString ValueString;
		ValueString.Printf(wxString(mFormat.c_str(), wxConvBoxi), 
			*ValuePtr);
		SetValue(ValueString);

		// work around Windows bug
		int ValueInt = *ValuePtr;
		mrIntProp.Set(ValueInt);
	}
	else
	{
		SetValue(wxT(""));
		mrIntProp.Clear();
	}
}

void BoundHexCtrl::OnChange()
{
	wxString tempString = GetValue();
	if (tempString.Length() == 0)
	{
		mrIntProp.Clear();
		mIsValid = true;
		return;
	}

	unsigned int tempValue;
	char *endptr;

	wxCharBuffer buf = tempString.mb_str(wxConvBoxi);

	if (!isxdigit(buf.data()[0]))
	{
		mrIntProp.Clear();
		mIsValid = false;
		return;
	}

	tempValue = strtoul(buf.data(), &endptr, 16);
	
	if (*endptr != '\0')
	{
		mrIntProp.Clear();
		mIsValid = false;
		return;
	}

	mrIntProp.Set(tempValue);
	mIsValid = true;
}

BEGIN_EVENT_TABLE(BoundBoolCtrl, wxCheckBox)
	EVT_CHECKBOX(wxID_ANY, BoundBoolCtrl::OnCheckboxClicked)
END_EVENT_TABLE()

void BoundBoolCtrl::Reload()
{
	const bool * value = mrBoolProp.GetPointer();
	if (value) 
	{
		SetValue(*value);
		// work around Windows bug
		mrBoolProp.Set(*value);
	}
	else
	{
		SetValue(FALSE);
		mrBoolProp.Clear();
	}
}

void BoundBoolCtrl::OnChange()
{
	mrBoolProp.Set(IsChecked());
}

BEGIN_EVENT_TABLE(FileSelButton, wxBitmapButton)
	EVT_BUTTON(wxID_ANY, FileSelButton::OnClick)
END_EVENT_TABLE()

void FileSelButton::OnClick(wxCommandEvent& event)
{
	wxString fileName = mpTextCtrl->GetValue();
	wxFileName file(fileName);
	
	wxFileName dir(file.GetPath());
	dir.MakeAbsolute();
	
	int flags = 0;
	if (mFileMustExist)
	{
		flags = wxOPEN | wxFILE_MUST_EXIST;
	}
	else
	{
		flags = wxSAVE | wxOVERWRITE_PROMPT;
	}
	
	wxString path = wxFileSelector(mFileSelectDialogTitle, 
		dir.GetFullPath(), file.GetFullName(), wxT(""),
		mFileSpec, flags, this);

	if (path.empty()) return;

	mpTextCtrl->SetValue(path);
}

BEGIN_EVENT_TABLE(DirSelButton, wxBitmapButton)
	EVT_BUTTON(wxID_ANY, DirSelButton::OnClick)
END_EVENT_TABLE()

void DirSelButton::OnClick(wxCommandEvent& event)
{
	wxString dirName = mpTextCtrl->GetValue();
	wxFileName dir(dirName);
	dir.MakeAbsolute();
	
	wxString newDir = wxDirSelector(
		wxT("Set Property"), dir.GetFullPath(), 0, 
		wxDefaultPosition, this);
	
	if (newDir.empty()) return;
	mpTextCtrl->SetValue(newDir);
}

ParamPanel::ParamPanel
(
	wxWindow* parent, 
	wxWindowID id,
	const wxPoint& pos, 
	const wxSize& size,
	long style, 
	const wxString& name
)
: wxPanel(parent, id, pos, size, style, name)
{
	wxBoxSizer *pBasicBox = new wxBoxSizer(wxVERTICAL);
	SetSizer(pBasicBox, true);
	
	mpSizer = new wxGridSizer(2, 4, 4);
	pBasicBox->Add(mpSizer, 0, wxGROW | wxALL, 8);
	
	row = 0;
}

BoundStringCtrl* ParamPanel::AddParam(const wxChar * pLabel, 
	StringProperty& rProp, int ID, bool FileSel, bool DirSel, 
	const wxChar* pFileSpec, const wxChar* pFileExtDefault)
{
	mpSizer->Add(
		new wxStaticText(this, -1, wxString(pLabel, wxConvBoxi), 
			wxDefaultPosition),
		1, wxALIGN_CENTER_VERTICAL);

	wxBoxSizer *pMiniSizer = new wxBoxSizer( wxHORIZONTAL );
	mpSizer->Add(pMiniSizer, 1, wxGROW);
	
	BoundStringCtrl* pCtrl = new BoundStringCtrl(this, ID, rProp);
	pMiniSizer->Add(pCtrl, 1, wxGROW);

	wxBitmap Bitmap = wxArtProvider::GetBitmap(wxART_FILE_OPEN, 
		wxART_CMN_DIALOG, wxSize(16, 16));

	if (FileSel) 
	{
		FileSelButton* pButton = new FileSelButton(this, -1, pCtrl, 
			pFileSpec, pFileExtDefault);
		pMiniSizer->Add(pButton, 0, wxGROW | wxLEFT, 4);
	}
	else if (DirSel)
	{
		DirSelButton* pButton = new DirSelButton(this, -1, pCtrl);
		pMiniSizer->Add(pButton, 0, wxGROW | wxLEFT, 4);
	}
	
	return pCtrl;
}

BoundIntCtrl* ParamPanel::AddParam(const wxChar * pLabel, IntProperty& rProp, 
	const char * pFormat, int ID)
{
	BoundIntCtrl* pCtrl = new BoundIntCtrl(this, ID, rProp, pFormat);
	::AddParam(this, pLabel, pCtrl, true, mpSizer);
	return pCtrl;
}

BoundBoolCtrl* ParamPanel::AddParam(const wxChar * pLabel, BoolProperty& rProp, 
	int ID)
{
	BoundBoolCtrl* pCtrl = new BoundBoolCtrl(this, ID, rProp);
	::AddParam(this, pLabel, pCtrl, true, mpSizer);
	return pCtrl;
}
