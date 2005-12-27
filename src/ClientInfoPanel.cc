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
	
	wxScrolledWindow *pVisibleArea = new wxScrolledWindow(this, wxID_ANY,
		wxDefaultPosition, wxDefaultSize, wxVSCROLL);
	pTabSizer->Add(pVisibleArea, 1, wxGROW | wxALL, 4);
	
	wxBoxSizer *pScrollablePanelSizer = new wxBoxSizer( wxVERTICAL );
	pVisibleArea->SetSizer(pScrollablePanelSizer);
	pVisibleArea->SetScrollRate(0, 1);
	
	wxBoxSizer* pStateSizer = new wxBoxSizer( wxHORIZONTAL );
	pScrollablePanelSizer->Add(pStateSizer, 0, wxGROW | wxALL, 4);

	mpConfigStateBitmap = new TickCrossIcon(pVisibleArea);
	pStateSizer->Add(mpConfigStateBitmap, 0, wxGROW | wxALL, 4);

	mpConfigStateLabel = new wxStaticText(pVisibleArea, -1, wxT("Unknown state"));
	pStateSizer->Add(mpConfigStateLabel, 1, wxGROW | wxALL, 4);
	
	wxNotebook *pClientPropsNotebook = new wxNotebook(pVisibleArea, -1);
	pScrollablePanelSizer->Add(pClientPropsNotebook, 1, 
		wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 8);
	
	ParamPanel *pBasicPanel = new ParamPanel(pClientPropsNotebook);
	pClientPropsNotebook->AddPage(pBasicPanel, wxT("Basic"));
	
	ParamPanel *pAdvancedPanel = new ParamPanel(pClientPropsNotebook);
	pClientPropsNotebook->AddPage(pAdvancedPanel, wxT("Advanced"));

	mpStoreHostnameCtrl = pBasicPanel->AddParam(
		wxT("Store Host:"), pConfig->StoreHostname, 
		ID_ClientProp_StoreHostname, 
		FALSE, FALSE, NULL);

	mpAccountNumberCtrl = pBasicPanel->AddParam(
		wxT("Account Number:"), pConfig->AccountNumber, 
		"0x%x", ID_ClientProp_AccountNumber);
		
	mpKeysFileCtrl = pBasicPanel->AddParam(
		wxT("Keys File:"), pConfig->KeysFile, 
		ID_ClientProp_KeysFile, TRUE, FALSE, 
		wxT("Encryption key files (*-FileEncKeys.raw)|"
			"*-FileEncKeys.raw"));
		
	mpCertificateFileCtrl = pBasicPanel->AddParam(
		wxT("Certificate File:"), pConfig->CertificateFile, 
		ID_ClientProp_CertificateFile, TRUE, FALSE,
		wxT("Certificate files (*-cert.pem)|*-cert.pem"));

	mpPrivateKeyFileCtrl = pBasicPanel->AddParam(
		wxT("Private Key File:"), pConfig->PrivateKeyFile, 
		ID_ClientProp_PrivateKeyFile, TRUE, FALSE,
		wxT("Private key files (*-key.pem)|*-key.pem"));

	mpTrustedCAsFileCtrl = pBasicPanel->AddParam(
		wxT("Trusted CAs File:"), pConfig->TrustedCAsFile, 
		ID_ClientProp_TrustedCAsFile, TRUE, FALSE,
		wxT("Server CA certificate (serverCA.pem)|serverCA.pem"));

	mpDataDirectoryCtrl = pBasicPanel->AddParam(
		wxT("Data Directory:"), pConfig->DataDirectory, 
		ID_ClientProp_DataDirectory, FALSE, TRUE, NULL);

	mpCommandSocketCtrl = pBasicPanel->AddParam(
		wxT("Command Socket:"), pConfig->CommandSocket, 
		ID_ClientProp_CommandSocket, TRUE, FALSE, 
		wxT("Command socket (bbackupd.sock)|bbackupd.sock"));

	mpExtendedLoggingCtrl = pBasicPanel->AddParam(
		wxT("Extended Logging:"), pConfig->ExtendedLogging, 
		ID_ClientProp_ExtendedLogging);

	mpUpdateStoreIntervalCtrl = pAdvancedPanel->AddParam(
		wxT("Update Store Interval:"), pConfig->UpdateStoreInterval, 
		"%d", ID_ClientProp_UpdateStoreInterval);
		
	mpMinimumFileAgeCtrl = pAdvancedPanel->AddParam(
		wxT("Minimum File Age:"), pConfig->MinimumFileAge, 
		"%d", ID_ClientProp_MinimumFileAge);

	mpMaxUploadWaitCtrl = pAdvancedPanel->AddParam(
		wxT("Maximum Upload Wait:"), 
		pConfig->MaxUploadWait, "%d", ID_ClientProp_MaxUploadWait);

	mpFileTrackingSizeThresholdCtrl = pAdvancedPanel->AddParam(
		wxT("File Tracking Size Threshold:"), 
		pConfig->FileTrackingSizeThreshold, 
		"%d", ID_ClientProp_FileTrackingSizeThreshold);
		
	mpDiffingUploadSizeThresholdCtrl = pAdvancedPanel->AddParam(
		wxT("Diffing Upload Size Threshold:"), 
		pConfig->DiffingUploadSizeThreshold, 
		"%d", ID_ClientProp_DiffingUploadSizeThreshold);

	mpMaximumDiffingTimeCtrl = pAdvancedPanel->AddParam(
		wxT("Maximum Diffing Time:"), pConfig->MaximumDiffingTime, 
		"%d", ID_ClientProp_MaximumDiffingTime);

	mpNotifyScriptCtrl = pAdvancedPanel->AddParam(
		wxT("Notify Script:"), pConfig->NotifyScript, 
		ID_ClientProp_NotifyScript, TRUE, FALSE,
		wxT("Default script (NotifySysadmin.sh)|NotifySysadmin.sh|"
			"All files (*)|*"));
	
	mpSyncAllowScriptCtrl = pAdvancedPanel->AddParam(
		wxT("Sync Allow Script:"), pConfig->SyncAllowScript, 
		ID_ClientProp_SyncAllowScript, TRUE, FALSE,
		wxT("All files (*)|*"));

	mpPidFileCtrl = pAdvancedPanel->AddParam(wxT("Process ID File:"), 
		pConfig->PidFile, ID_ClientProp_PidFile, TRUE, FALSE,
		wxString(wxT("Client PID files (bbackupd.pid)|bbackupd.pid")));
	
	wxSizer* pActionCtrlSizer = new wxBoxSizer(wxHORIZONTAL);
	pTabSizer->Add(pActionCtrlSizer, 0, 
		wxALIGN_RIGHT | wxLEFT | wxRIGHT | wxBOTTOM, 8);

	wxButton* pCloseButton = new wxButton(this, 
		wxID_CANCEL, wxT("Close"));
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
		msg = wxT("Configuration looks OK");
	}
	else 
	{
		mpConfigStateBitmap->SetTicked(FALSE);
	}
	mpConfigStateLabel->SetLabel(msg);
}
