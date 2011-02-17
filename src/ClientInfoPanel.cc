/***************************************************************************
 *            ClientInfoPanel.cc
 *
 *  Tue Mar  1 00:16:12 2005
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

#include "SandBox.h"

#include <wx/wx.h>
#include <wx/artprov.h>
#include <wx/filename.h>
#include <wx/image.h>
#include <wx/mstream.h>
#include <wx/notebook.h>

#include "ClientInfoPanel.h"
#include "StaticImage.h"
#include "ParamPanel.h"

BEGIN_EVENT_TABLE(ClientInfoPanel, wxPanel)
	EVT_BUTTON(wxID_CANCEL, ClientInfoPanel::OnClickCloseButton)
END_EVENT_TABLE()

void LoadBitmap(const struct StaticImage& rImageData, wxBitmap& rDest)
{
	wxMemoryInputStream byteStream(rImageData.data, rImageData.size);
	wxImage img(byteStream, wxBITMAP_TYPE_PNG);
	rDest = wxBitmap(img);
}

class WrappingLabel : public wxStaticText
{
	public:
	WrappingLabel(wxWindow* pParent, wxWindowID id, const wxString& rLabel)
	: wxStaticText(pParent, id, rLabel) { }
	virtual void SetLabel(const wxString& rLabel)
	{
		this->wxStaticText::SetLabel(rLabel);
		wxWindow* pTarget = GetParent();
		while (!pTarget->IsTopLevel())
		{
			pTarget = pTarget->GetParent();
		}
		pTarget->Layout();
	}
};

ClientInfoPanel::ClientInfoPanel(ClientConfig *pConfig,
	wxWindow* parent, wxWindowID id,
	const wxPoint& pos, const wxSize& size,
	long style, const wxString& name)
	: wxPanel(parent, id, pos, size, style, name)
{
	mpConfig = pConfig;
	mpConfig->AddListener(this);
	
	wxBoxSizer *pTabSizer = new wxBoxSizer( wxVERTICAL );
	SetSizer(pTabSizer);
	
	wxBoxSizer* pStateSizer = new wxBoxSizer( wxHORIZONTAL );
	pTabSizer->Add(pStateSizer, 0, wxGROW | wxALL, 4);

	mpConfigStateBitmap = new TickCrossIcon(this);
	pStateSizer->Add(mpConfigStateBitmap, 0, wxALIGN_CENTER_VERTICAL | wxALL, 4);

	mpConfigStateLabel = new WrappingLabel(this, -1, _("Unknown state"));
	pStateSizer->Add(mpConfigStateLabel, 1, wxALIGN_CENTER_VERTICAL | wxALL, 4);
	
	wxScrolledWindow *pVisibleArea = new wxScrolledWindow(this, wxID_ANY,
		wxDefaultPosition, wxDefaultSize, wxVSCROLL);
	pTabSizer->Add(pVisibleArea, 1, wxGROW | wxALL, 4);
	
	wxBoxSizer *pScrollablePanelSizer = new wxBoxSizer( wxVERTICAL );
	pVisibleArea->SetSizer(pScrollablePanelSizer);
	pVisibleArea->SetScrollRate(0, 1);
	
	wxNotebook *pClientPropsNotebook = new wxNotebook(pVisibleArea, -1);
	pScrollablePanelSizer->Add(pClientPropsNotebook, 1, 
		wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 8);
	
	ParamPanel *pBasicPanel = new ParamPanel(pClientPropsNotebook);
	pClientPropsNotebook->AddPage(pBasicPanel, _("Basic"));
	
	ParamPanel *pAdvancedPanel = new ParamPanel(pClientPropsNotebook);
	pClientPropsNotebook->AddPage(pAdvancedPanel, _("Advanced"));

	mpStoreHostnameCtrl = pBasicPanel->AddParam(
		_("Store Host:"), pConfig->StoreHostname, 
		wxID_ANY, FALSE, FALSE);

	mpAccountNumberCtrl = pBasicPanel->AddParam(
		_("Account Number:"), pConfig->AccountNumber, 
		"0x%x", wxID_ANY);
		
	mpKeysFileCtrl = pBasicPanel->AddParam(
		_("Keys File:"), pConfig->KeysFile, 
		wxID_ANY, TRUE, FALSE, 
		_("Encryption key files (*-FileEncKeys.raw)|"
			"*-FileEncKeys.raw"), _("-FileEncKeys.raw"));
		
	mpCertificateFileCtrl = pBasicPanel->AddParam(
		_("Certificate File:"), pConfig->CertificateFile, 
		wxID_ANY, TRUE, FALSE,
		_("Certificate files (*-cert.pem)|*-cert.pem"),
		_("-cert.pem"));

	mpPrivateKeyFileCtrl = pBasicPanel->AddParam(
		_("Private Key File:"), pConfig->PrivateKeyFile, 
		wxID_ANY, TRUE, FALSE,
		_("Private key files (*-key.pem)|*-key.pem"),
		_("-key.pem"));

	mpTrustedCAsFileCtrl = pBasicPanel->AddParam(
		_("Trusted CAs File:"), pConfig->TrustedCAsFile, 
		wxID_ANY, TRUE, FALSE,
		_("Server CA certificate (serverCA.pem)|serverCA.pem"),
		_("serverCA.pem"));

	mpDataDirectoryCtrl = pBasicPanel->AddParam(
		_("Data Directory:"), pConfig->DataDirectory, 
		wxID_ANY, FALSE, TRUE);

	mpCommandSocketCtrl = pBasicPanel->AddParam(
		_("Command Socket:"), pConfig->CommandSocket, 
		wxID_ANY, TRUE, FALSE, 
		_("Command socket (bbackupd.sock)|bbackupd.sock"),
		_("bbackupd.sock"));

	mpExtendedLoggingCtrl = pBasicPanel->AddParam(
		_("Extended Logging:"), pConfig->ExtendedLogging, 
		wxID_ANY);

	mpUpdateStoreIntervalCtrl = pAdvancedPanel->AddParam(
		_("Update Store Interval:"), pConfig->UpdateStoreInterval, 
		"%d", wxID_ANY);
		
	mpMinimumFileAgeCtrl = pAdvancedPanel->AddParam(
		_("Minimum File Age:"), pConfig->MinimumFileAge, 
		"%d", wxID_ANY);

	mpMaxUploadWaitCtrl = pAdvancedPanel->AddParam(
		_("Maximum Upload Wait:"), 
		pConfig->MaxUploadWait, "%d", wxID_ANY);

	mpFileTrackingSizeThresholdCtrl = pAdvancedPanel->AddParam(
		_("File Tracking Size Threshold:"), 
		pConfig->FileTrackingSizeThreshold, 
		"%d", wxID_ANY);
		
	mpDiffingUploadSizeThresholdCtrl = pAdvancedPanel->AddParam(
		_("Diffing Upload Size Threshold:"), 
		pConfig->DiffingUploadSizeThreshold, 
		"%d", wxID_ANY);

	mpMaximumDiffingTimeCtrl = pAdvancedPanel->AddParam(
		_("Maximum Diffing Time:"), pConfig->MaximumDiffingTime, 
		"%d", wxID_ANY);

	mpKeepAliveTimeCtrl = pAdvancedPanel->AddParam(
		_("Keep-Alive Time:"), pConfig->KeepAliveTime, 
		"%d", wxID_ANY);

	mpNotifyScriptCtrl = pAdvancedPanel->AddParam(
		_("Notify Script:"), pConfig->NotifyScript, 
		wxID_ANY, TRUE, FALSE,
		_("Default script (NotifySysadmin.sh)|NotifySysadmin.sh|"
			"All files (*)|*"));
	
	mpSyncAllowScriptCtrl = pAdvancedPanel->AddParam(
		_("Sync Allow Script:"), pConfig->SyncAllowScript, 
		wxID_ANY, TRUE, FALSE,
		_("All files (*)|*"));

	mpPidFileCtrl = pAdvancedPanel->AddParam(_("Process ID File:"), 
		pConfig->PidFile, wxID_ANY, TRUE, FALSE,
		_("Client PID files (bbackupd.pid)|bbackupd.pid"),
		_("bbackupd.pid"));
	
	wxSizer* pActionCtrlSizer = new wxBoxSizer(wxHORIZONTAL);
	pTabSizer->Add(pActionCtrlSizer, 0, 
		wxALIGN_RIGHT | wxLEFT | wxRIGHT | wxBOTTOM, 8);

	wxButton* pCloseButton = new wxButton(this, 
		wxID_CANCEL, _("Close"));
	pActionCtrlSizer->Add(pCloseButton, 0, wxGROW | wxLEFT, 8);

	// pScrollablePanelSizer->SetSizeHints(this);
	
	UpdateConfigState();
}

void ClientInfoPanel::OnClickCloseButton(wxCommandEvent& rEvent)
{
	Hide();
}

void ClientInfoPanel::Reload() 
{
	mpStoreHostnameCtrl             ->Reload();
	mpAccountNumberCtrl             ->Reload();
	mpKeysFileCtrl                  ->Reload();
	mpCertificateFileCtrl           ->Reload();
	mpPrivateKeyFileCtrl            ->Reload();
	mpTrustedCAsFileCtrl            ->Reload();
	mpDataDirectoryCtrl             ->Reload();
	mpNotifyScriptCtrl              ->Reload();
	mpUpdateStoreIntervalCtrl       ->Reload();
	mpMinimumFileAgeCtrl            ->Reload();
	mpMaxUploadWaitCtrl             ->Reload();
	mpFileTrackingSizeThresholdCtrl ->Reload();
	mpDiffingUploadSizeThresholdCtrl->Reload();
	mpMaximumDiffingTimeCtrl        ->Reload();
	mpKeepAliveTimeCtrl             ->Reload();
	mpExtendedLoggingCtrl           ->Reload();
	mpSyncAllowScriptCtrl           ->Reload();
	mpCommandSocketCtrl             ->Reload();
	mpPidFileCtrl                   ->Reload();
}

void ClientInfoPanel::NotifyChange()
{
	UpdateConfigState();
}

void ClientInfoPanel::UpdateConfigState()
{
	wxString msg;
	if (mpConfig->Check(msg))
	{
		mpConfigStateBitmap->SetTicked(TRUE);
		msg = _("Configuration looks OK");
	}
	else 
	{
		mpConfigStateBitmap->SetTicked(FALSE);
	}
	mpConfigStateLabel->SetLabel(msg);
}
