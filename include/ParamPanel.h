/***************************************************************************
 *            ParamPanel.h
 *
 *  Sun May 15 15:19:21 2005
 *  Copyright  2005  Chris Wilson
 *  Email <boxi_ParamPanel.h@qwirx.com>
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

#ifndef _PARAMPANEL_H
#define _PARAMPANEL_H

#include <wx/wx.h>

#include "ClientConfig.h"

class BoundCtrl 
{
	private:
	wxString mName;

	public:
	BoundCtrl(Property& rProp) 
	: mName(rProp.GetKeyName().c_str(), wxConvLibc) { }
	virtual ~BoundCtrl() { }

	virtual void Reload()   = 0;
	virtual void OnChange() = 0;
	virtual const wxString& GetName() { return mName; }
};

class BoundStringCtrl : public wxTextCtrl, public BoundCtrl {
	private:
	StringProperty& mrStringProp;	
	
	public:
	BoundStringCtrl(
		wxWindow* parent, 
		wxWindowID id,
		StringProperty& rStringProp)
	: wxTextCtrl(parent, id, wxT("")),
	  BoundCtrl(rStringProp),
	  mrStringProp(rStringProp)
	{
		Reload();
	}
	void Reload();
	void OnChange();
};

class BoundIntCtrl : public wxTextCtrl, public BoundCtrl {
	private:
	IntProperty& mrIntProp;
	wxString mFormat;
	
	public:
	BoundIntCtrl(wxWindow* parent, 
		wxWindowID id,
		IntProperty& rIntProp, const char * pFormat)
	: wxTextCtrl(parent, id, wxT("")),
	  BoundCtrl(rIntProp),
	  mrIntProp(rIntProp),
	  mFormat(pFormat, wxConvLibc)
	{
		Reload();
	}
	void Reload();
	void OnChange();
	void OnFocusLost(wxFocusEvent& event) {
		Reload();
		event.Skip();
	}

	DECLARE_EVENT_TABLE()	
};

class BoundBoolCtrl : public wxCheckBox, public BoundCtrl {
	private:
	BoolProperty& mrBoolProp;
	
	public:
	BoundBoolCtrl(
		wxWindow* parent, 
		wxWindowID id,
		BoolProperty& rBoolProp)
	: wxCheckBox(parent, id, wxT("Enabled")),
	  BoundCtrl(rBoolProp),
	  mrBoolProp(rBoolProp)
	{ 
		Reload(); 
	}
	void Reload();
	void OnChange();
};	

class FileSelButton : public wxBitmapButton 
{
	private:
	StringProperty& mrProperty;
	BoundStringCtrl* mpStringCtrl;
	wxString mFileSpec;
	
	public:
	FileSelButton(wxWindow* pParent, wxWindowID id, 
		const wxBitmap& rBitmap, 
		StringProperty& rProp, 
		BoundStringCtrl* pStringCtrl,
		wxString& rFileSpec)
	: wxBitmapButton(pParent, id, rBitmap),
	  mrProperty(rProp),
	  mFileSpec(rFileSpec)
	{
		mpStringCtrl = pStringCtrl;
	}
	
	void OnClick(wxCommandEvent& event);
	
	DECLARE_EVENT_TABLE()
};

class DirSelButton : public wxBitmapButton 
{
	private:
	StringProperty& mrProperty;
	BoundStringCtrl* mpStringCtrl;
	
	public:
	DirSelButton(wxWindow* pParent, wxWindowID id, const wxBitmap& rBitmap, 
		StringProperty& rProp, BoundStringCtrl* pStringCtrl)
	: wxBitmapButton(pParent, id, rBitmap),
	  mrProperty(rProp)
	{
		mpStringCtrl = pStringCtrl;
	}
	
	void OnClick(wxCommandEvent& event);
	
	DECLARE_EVENT_TABLE()
};

class ParamPanel : public wxPanel {
	public:
	ParamPanel(
		wxWindow* parent, wxWindowID id = -1,
		const wxPoint& pos = wxDefaultPosition, 
		const wxSize& size = wxDefaultSize,
		long style = wxTAB_TRAVERSAL, 
		const wxString& name = wxT("ParamPanel"));
	
	BoundStringCtrl* AddParam(const wxChar * pLabel, StringProperty& rProp, 
		int ID,	bool FileSel, bool DirSel, const wxChar* rFileSpec);
	BoundIntCtrl*    AddParam(const wxChar * pLabel, IntProperty&    rProp, 
		const char *format, int ID);
	BoundBoolCtrl*   AddParam(const wxChar * pLabel, BoolProperty&   rProp, 
		int ID);
	
	private:
	wxGridSizer *mpSizer;
	int row;
};

class DirCtrl : public wxTextCtrl {
	public:
	DirCtrl(
		wxWindow* parent, 
		wxWindowID id)
	: wxTextCtrl(parent, id, wxT(""))
	{
		Reload();
	}
	void Reload();
	void OnChange();
};

#endif /* ! _PARAMPANEL_H */
