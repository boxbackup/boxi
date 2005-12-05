/***************************************************************************
 *            ParamPanel.cc
 *
 *  Sun May 15 15:18:40 2005
 *  Copyright  2005  Chris Wilson
 *  Email <boxi_ParamPanel.cc@qwirx.com>
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

#include <wx/artprov.h>
#include <wx/filename.h>
#include <wx/image.h>

#include "ParamPanel.h"
#include "main.h"

void BoundStringCtrl::Reload() 
{
	const std::string * pValue = mrStringProp.Get();
	if (pValue) 
	{
		wxString valueString(pValue->c_str(), wxConvLibc);
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
	wxCharBuffer buf = tempString.mb_str(wxConvLibc);
	if (tempString.Length() == 0) {
		mrStringProp.Clear();
	} else {
		mrStringProp.Set(buf.data());
	}
}

BEGIN_EVENT_TABLE(BoundIntCtrl, wxTextCtrl)
EVT_KILL_FOCUS(BoundIntCtrl::OnFocusLost)
END_EVENT_TABLE()
		
void BoundIntCtrl::Reload()
{
	const int * ValuePtr = mrIntProp.Get();
	if (ValuePtr)
	{
		wxString ValueString;
		ValueString.Printf(
			wxString(mFormat.c_str(), wxConvLibc), 
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

	wxCharBuffer buf = tempString.mb_str(wxConvLibc);
	
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

void BoundBoolCtrl::Reload()
{
	const bool * value = mrBoolProp.Get();
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
	std::string oldValue;
	mrProperty.GetInto(oldValue);
	
	wxFileName file(wxString(oldValue.c_str(), wxConvLibc));
	wxFileName dir(file);
	dir.SetName(wxT(""));
	dir.SetExt(wxT(""));
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

	wxCharBuffer buf = path.mb_str(wxConvLibc);
	mrProperty.Set(buf.data());
	mpStringCtrl->Reload();
}

BEGIN_EVENT_TABLE(DirSelButton, wxBitmapButton)
EVT_BUTTON(wxID_ANY, DirSelButton::OnClick)
END_EVENT_TABLE()

void DirSelButton::OnClick(wxCommandEvent& event)
{
	std::string oldValue;
	mrProperty.GetInto(oldValue);
	
	wxFileName dir(wxString(oldValue.c_str(), wxConvLibc));
	dir.MakeAbsolute();
	
	wxString newDir = wxDirSelector(
		wxT("Set Property"), dir.GetFullPath(), 0, 
		wxDefaultPosition, this);
	
	if (newDir.empty()) return;
	wxCharBuffer buf = newDir.mb_str(wxConvLibc);
	mrProperty.Set(buf.data());
	mpStringCtrl->Reload();
}

ParamPanel::ParamPanel(
	wxWindow* parent, wxWindowID id,
	const wxPoint& pos, const wxSize& size,
	long style, const wxString& name)
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
	const wxChar* pFileSpec)
{
	mpSizer->Add(
		new wxStaticText(this, -1, wxString(pLabel, wxConvLibc), 
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
		wxString spec(pFileSpec);
		FileSelButton* pButton = new FileSelButton(this, 
				-1, rProp, pCtrl, spec);
		pMiniSizer->Add(pButton, 0, wxGROW);
	}
	else if (DirSel)
	{
		DirSelButton* pButton = new DirSelButton(this, 
				-1, Bitmap, rProp, pCtrl);
		pMiniSizer->Add(pButton, 0, wxGROW);
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
