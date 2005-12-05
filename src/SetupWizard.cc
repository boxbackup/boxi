/***************************************************************************
 *            SetupWizardPanel.cc
 *
 *  Tue Mar  1 00:26:30 2005
 *  Copyright  2005  Chris Wilson
 *  anjuta@qwirx.com
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

#include <wx/html/htmlwin.h>
#include <wx/artprov.h>
#include <wx/image.h>

#include "SetupWizardPanel.h"
#include "ParamPanel.h"

class SetupWizardPage : public wxWizardPageSimple
{
	public:
	SetupWizardPage(ClientConfig *config, wxWizard* parent = NULL,
		wxString Text)
 	: wxWizardPageSimple(parent)
	{
		mpConfig = config;

		mpSizer = new wxBoxSizer(wxVERTICAL);
		SetSizer(mpSizer);
		wxHtmlWindow* pIntroText = new wxHtmlWindow(this, wxID_ANY, 
			wxDefaultPosition, wxDefaultSize, 
			wxHW_SCROLLBAR_NEVER | wxHW_NO_SELECTION);
		pIntroText->SetStandardFonts(10);
		pIntroText->SetPage(Text);
		mpSizer->Add(pIntroText, 1, wxGROW | wxALL, 0);
	}
 
	protected:
	ClientConfig* mpConfig;
	wxBoxSizer*   mpSizer;
};

class SetupWizardIntroPage : public SetupWizardPage
{
	public:
	SetupWizardIntroPage(ClientConfig *config,
		wxWizard* parent = NULL)
 	: SetupWizardPage(config, parent, wxT(
		"<html><body><h1>Boxi Setup Wizard</h1>"
		"<p>This wizard can help you configure Boxi for a new server, "
		"or change your settings, as simply as possible.</p>"
		"<p>Please click on the <b>Next</b> button below to continue, "
		"or <b>Cancel</b> if you don't want to reconfigure Boxi.</p>"
		"</body></html>"))
	{ }
};

class SetupWizardPrivateKeyPage : public SetupWizardPage
{
	private:
	wxStaticBitmap* mpConfigStateBitmap;
	wxStaticText*   mpConfigStateLabel;

	public:
	SetupWizardPrivateKeyPage(ClientConfig *config,
		wxWizard* parent = NULL)
 	: SetupWizardPage(config, parent, wxT(
		"<html><body><h1>Private Key</h1>"
		"<p>You need a private key to prove your identity to the server. "
		"The private key is stored in a file on your hard disk.</p>"
		"<p>If you already have a private key, you should continue to "
		"use it unless it has been compromised (stolen) or the "
		"server administrator tells you to generate a new one.</p>"
		"<p>If you want to use an existing key, select the "
		"<b>Existing Private Key</b> option below, and tell Boxi "
		"where to find the key file.</p>"
		"<p>Otherwise, select the <b>New Private Key</b> option, and tell Boxi "
		"where the new key file should be stored.</p>"
		"</body></html>"))
	{
		wxRadioButton* pExistingRadio = new wxRadioButton(
			this, wxID_ANY, wxT("Existing Private Key"), 
			wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
		mpSizer->Add(pExistingRadio, 0, wxGROW | wxTOP, 8);

		wxRadioButton* pNewRadio = new wxRadioButton(
			this, wxID_ANY, wxT("New Private Key"), 
			wxDefaultPosition, wxDefaultSize);
		mpSizer->Add(pNewRadio, 0, wxGROW | wxTOP, 8);
		
		wxSizer* pLocationSizer = new wxBoxSizer(wxHORIZONTAL);
		mpSizer->Add(pLocationSizer, 0, wxGROW | wxTOP, 8);
		
		wxStaticText* pLabel = new wxStaticText(this, wxID_ANY,
			wxT("Key file location:"));
		pLocationSizer->Add(pLabel, 0, wxALIGN_CENTER_VERTICAL, 0);
		
		BoundStringCtrl* pLocationText = new BoundStringCtrl(
			this, wxID_ANY, mpConfig->KeysFile);
		pLocationSizer->Add(pLocationText, 1, 
			wxALIGN_CENTER_VERTICAL | wxLEFT, 8);
			
		FileSelButton* pFileSelButton = new FileSelButton(
			this, wxID_ANY, mpConfig->KeysFile, pLocationText, 
			wxT("Encryption key files (*-FileEncKeys.raw)|*-FileEncKeys.raw"), 
			wxT("Open Private Key File"));
		pLocationSizer->Add(pFileSelButton, 0, wxGROW | wxLEFT, 8);
	}
};

SetupWizard::SetupWizard(ClientConfig *config,
	wxWindow* parent, wxWindowID id)
	: wxWizard(parent, id, wxT("Boxi Setup Wizard"), 
		wxNullBitmap, wxDefaultPosition, 
		wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
	mpConfig = config;

	GetPageAreaSizer()->SetMinSize(500, 400);
	
	wxWizardPageSimple* pIntroPage = 
		new SetupWizardIntroPage(config, this);
	wxWizardPageSimple* pPrivateKeyPage = 
		new SetupWizardPrivateKeyPage(config, this);
	
	wxWizardPageSimple::Chain(pIntroPage, pPrivateKeyPage);
	
	RunWizard(pIntroPage);
}
