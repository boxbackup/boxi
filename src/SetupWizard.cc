/***************************************************************************
 *            SetupWizard.cc
 *
 *  Created 2005-12-24 01:05
 *  Copyright  2005  Chris Wilson <boxi_source@qwirx.com>
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
 *
 *  Includes (small) portions of OpenSSL by Eric Young (eay@cryptsoft.com) and
 *  Tim Hudson (tjh@cryptsoft.com), under their source redistribution license.
 */

#include <wx/html/htmlwin.h>
#include <wx/progdlg.h>

#include <openssl/conf.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>

#include "SetupWizard.h"
#include "ParamPanel.h"

class SetupWizardPage : public wxWizardPageSimple
{
	public:
	SetupWizardPage(ClientConfig *config, wxWizard* parent = NULL,
		wxString Text = wxT("No page text defined"))
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

static int numChecked = 0;

static void MySslProgressCallback(int p, int n, void* cbinfo)
{
	wxProgressDialog* pProgress = (wxProgressDialog *)cbinfo;
	wxString msg;
	msg.Printf(wxT("Generating your new private key (2048 bits).\n"
			"This may take a minute.\n\n(%d potential primes checked)"),
			++numChecked);
	pProgress->Update(0, msg);
	// wxSafeYield();
	/*
	char c = '?';
	
	if (p == 0) c = '.';
	if (p == 1) c = '+';
	if (p == 2) c = '*';
	if (p == 3) c = '\n';
	putc(c, stdout);
	fflush(stdout);
	*/
}

class SetupWizardPrivateKeyPage : public SetupWizardPage
{
	private:
	wxRadioButton* mpExistingRadio;
	wxRadioButton* mpNewRadio;
	wxTextCtrl* mpLocationText;
	
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
		mpExistingRadio = new wxRadioButton(
			this, wxID_ANY, wxT("Existing Private Key"), 
			wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
		mpSizer->Add(mpExistingRadio, 0, wxGROW | wxTOP, 8);

		mpNewRadio = new wxRadioButton(
			this, wxID_ANY, wxT("New Private Key"), 
			wxDefaultPosition, wxDefaultSize);
		mpSizer->Add(mpNewRadio, 0, wxGROW | wxTOP, 8);
		
		wxSizer* pLocationSizer = new wxBoxSizer(wxHORIZONTAL);
		mpSizer->Add(pLocationSizer, 0, wxGROW | wxTOP, 8);
		
		wxStaticText* pLabel = new wxStaticText(this, wxID_ANY,
			wxT("Key file location:"));
		pLocationSizer->Add(pLabel, 0, wxALIGN_CENTER_VERTICAL, 0);
		
		wxString location;
		mpConfig->PrivateKeyFile.GetInto(location);
		mpLocationText = new wxTextCtrl(this, wxID_ANY, location);
		pLocationSizer->Add(mpLocationText, 1, 
			wxALIGN_CENTER_VERTICAL | wxLEFT, 8);
			
		FileSelButton* pFileSelButton = new FileSelButton(
			this, wxID_ANY, mpConfig->KeysFile, mpLocationText, 
			wxT("Private key files (*-key.pem)|*-key.pem"), 
			wxT("Open Private Key File"));
		pLocationSizer->Add(pFileSelButton, 0, wxGROW | wxLEFT, 8);
		
		Connect(wxID_ANY,wxEVT_WIZARD_PAGE_CHANGING,
                wxWizardEventHandler(
					SetupWizardPrivateKeyPage::OnWizardPageChanging));
	}
	
	void OnWizardPageChanging(wxWizardEvent& event)
	{
		bool isOk = FALSE;
		
		if (mpExistingRadio->GetValue())
		{
			isOk = CheckExistingKey();
		}
		else if (mpNewRadio->GetValue())
		{
			isOk = CreateNewKey();
		}
		
		if (!isOk) event.Veto();
	}
	
	bool CheckExistingKey()
	{	
		if (!wxFileName::FileExists(mpLocationText->GetValue()))
		{
			wxMessageBox(wxT("The specified private key file "
				"could not be found"), wxT("Boxi Error"),
					  wxICON_ERROR | wxOK, this);
			return FALSE;
		}
		
		wxString opensslConfigPath(X509_get_default_cert_area(), 
			wxConvLibc);
		wxString opensslConfigFileName(wxT("openssl.cnf"));
		wxFileName opensslConfigFile(opensslConfigPath, 
			opensslConfigFileName);
			
		CONF* pConfig = NCONF_new(NULL);
		long errline;
		bool isOk = FALSE;
		
		if (!NCONF_load(pConfig, 
			opensslConfigFile.GetFullPath().mb_str(wxConvLibc), &errline))
		{
			wxString msg;
			msg.Printf(wxT("Failed to initialise OpenSSL. Perhaps the "
				"configuration file could not be found or read? (%s)"),
				opensslConfigFile.GetFullPath().c_str());
			ShowSslError(msg);			
		}
		else
		{
			isOk = CheckExistingKeyWithOpenSslConfigured(pConfig);
		}
		
		NCONF_free(pConfig);
		return isOk;
	}
	
	void ShowSslError(const wxString& msgBase)
	{
		unsigned long err = ERR_get_error();
		wxString sslErrorMsg(ERR_error_string(err, NULL), wxConvLibc);
		
		wxString msg = msgBase;
		
		const char* reason = ERR_reason_error_string(err);
		if (reason != NULL)
		{
			msg.Append(wxT(": "));
			msg.Append(wxString(reason, wxConvLibc));
		}
		
		msg.Append(wxT("\n\nThe full error code is: "));
		msg.Append(wxString(ERR_error_string(err, NULL), wxConvLibc));
		
		wxMessageBox(msg, wxT("Boxi Error"), wxICON_ERROR | wxOK, this);
	}		
	
	bool CheckExistingKeyWithOpenSslConfigured(CONF* pConfig)
	{
		OPENSSL_load_builtin_modules();
		
		if (CONF_modules_load(pConfig, NULL, 0) <= 0)
		{
			ShowSslError(wxT("Failed to configure OpenSSL"));
			return FALSE;
		}

		BIO* pKeyBio = BIO_new(BIO_s_file());
		
		if (pKeyBio == NULL)
		{
			ShowSslError(wxT("Failed to create an OpenSSL BIO"));
			return FALSE;
		}
		
		bool isOk = CheckExistingKeyWithBio(pKeyBio);
		BIO_free(pKeyBio);
		
		return isOk;
	}
	
	bool CheckExistingKeyWithBio(BIO* pKeyBio)
	{
		wxString keyFile = mpLocationText->GetValue();
		
		if (BIO_read_filename(pKeyBio, keyFile.mb_str(wxConvLibc).data()) <= 0)
		{
			ShowSslError(wxT("Failed to read key file"));
			return FALSE;
		}
		
		char *pName = NULL, *pHeader = NULL;
		unsigned char *pData = NULL;
		long len;
		
		for (;;)
		{
			if (!PEM_read_bio(pKeyBio, &pName, &pHeader, &pData, &len)) 
			{
				if (ERR_GET_REASON(ERR_peek_error()) == PEM_R_NO_START_LINE)
				{
					ERR_add_error_data(2, "Expecting: ", PEM_STRING_EVP_PKEY);
					ShowSslError(wxT("Failed to read key file"));
					return FALSE;
				}
			}
			
			if (strcmp(pName, PEM_STRING_RSA) == 0)
				break;
			
			OPENSSL_free(pName);
			OPENSSL_free(pHeader);
			OPENSSL_free(pData);
		}
		
		bool isOk = CheckExistingKeyWithData(pName, pHeader, pData);
		
		OPENSSL_free(pName);
		OPENSSL_free(pHeader);
		OPENSSL_free(pData);
		
		return isOk;
	}
	
	bool CheckExistingKeyWithData(char* pName, char* pHeader,
		unsigned char* pData)
	{
		EVP_CIPHER_INFO cipher;
		
		if (!PEM_get_EVP_CIPHER_INFO(pHeader, &cipher))
		{
			ShowSslError(wxT("Failed to decipher key file"));
			return FALSE;
		}
		
		if (cipher.cipher != NULL)
		{
			wxMessageBox(wxT("This private key is protected with a passphrase. "
				"It cannot be used with Box Backup."), wxT("Boxi Error"), 
				wxICON_ERROR | wxOK, this);
			return FALSE;
		}
		
		return TRUE;
	}

	bool CreateNewKey()
	{	
		wxFileName keyFileName(mpLocationText->GetValue());

		if (wxFileName::FileExists(keyFileName.GetFullPath()))
		{
			int result = wxMessageBox(wxT("The specified private key file "
				"already exists! If you overwrite an existing key, "
				"you may lose access to the server and your backups. "
				"Are you REALLY SURE you want to overwrite it?\n\n"
				"This operation CANNOT be undone!"), 
				wxT("Boxi Warning"), wxICON_WARNING | wxYES_NO, this);
			
			if (result != wxYES)
				return FALSE;

			if (! wxFile::Access(keyFileName.GetFullPath(), wxFile::write))
			{
				wxMessageBox(wxT("The specified private key file is read-only "
					"and cannot be overwritten."), wxT("Boxi Error"), 
					wxICON_ERROR | wxOK, this);
				return FALSE;
			}
		}
		else if (!wxFileName::DirExists(keyFileName.GetPath()))
		{
			wxMessageBox(wxT("The directory where you want to create the "
				"private key file does not exist, so the file cannot be "
				"created."), wxT("Boxi Error"), wxICON_ERROR | wxOK, this);
			return FALSE;
		}
		else // does not exist yet
		{
			if (! wxFile::Access(keyFileName.GetPath(), wxFile::write))
			{
				wxMessageBox(wxT("The directory where you want to create the "
					"private key file is read-only, so the file cannot be "
					"created."), wxT("Boxi Error"), wxICON_ERROR | wxOK, this);
				return FALSE;
			}
		}
		
		BIGNUM* pBigNum = BN_new();
		if (!pBigNum)
		{
			ShowSslError(wxT("Failed to create an OpenSSL BN object"));
			return FALSE;
		}
		
		RSA* pRSA = RSA_new();
		if (!pRSA)
		{
			ShowSslError(wxT("Failed to create an OpenSSL RSA object"));
			BN_free(pBigNum);
			return FALSE;
		}
		
		bool isOk = CreateNewKeyWithBigNumRSA(pBigNum, pRSA);
		
		RSA_free(pRSA);
		BN_free(pBigNum);
		
		return isOk;
	}
	
	bool CreateNewKeyWithBigNumRSA(BIGNUM* pBigNum, RSA* pRSA)
	{
		BIO* pKeyOutBio = BIO_new(BIO_s_file());
		if (!pKeyOutBio)
		{
			ShowSslError(wxT("Failed to create an OpenSSL BIO object "
				"for key output"));
			return FALSE;
		}
		
		bool isOk = CreateNewKeyWithBio(pBigNum, pRSA, pKeyOutBio);
		
		BIO_free_all(pKeyOutBio);
		
		return isOk;
	}
	
	bool CreateNewKeyWithBio(BIGNUM* pBigNum, RSA* pRSA, BIO* pKeyOutBio)
	{
		char* filename = strdup(
			mpLocationText->GetValue().mb_str(wxConvLibc).data());		
		int result = BIO_write_filename(pKeyOutBio, filename);
		free(filename);
		
		if (result <= 0)
		{
			ShowSslError(wxT("Failed to open key file using an OpenSSL BIO"));
			return FALSE;
		}

		if (!RAND_status())
		{
			int result = wxMessageBox(wxT("Your system is not configured "
				"with a random number generator. It may be possible for "
				"an attacker to guess your keys and gain access to your data. "
				"Do you want to continue anyway?"), 
				wxT("Boxi Warning"), wxICON_WARNING | wxYES_NO, this);
			
			if (result != wxYES)
				return FALSE;
		}
		
		if (!BN_set_word(pBigNum, RSA_F4))
		{
			ShowSslError(wxT("Failed to set key type to RSA F4"));
			return FALSE;
		}

		/*		
		BN_GENCB sslProgressCallbackInfo;
		BN_GENCB_set(&sslProgressCallbackInfo, MySslProgressCallback, NULL);
		*/
		
		wxProgressDialog progress(wxT("Boxi: Generating Private Key"),
			wxT("Generating your new private key (2048 bits).\n"
			"This may take a minute.\n\n(0 potential primes checked)"), 2, this, 
			wxPD_APP_MODAL | wxPD_ELAPSED_TIME);
		numChecked = 0;
		
		RSA* pRSA2 = RSA_generate_key(2048, 0x10001, MySslProgressCallback, 
			&progress);
		if (!pRSA2)
		{
			ShowSslError(wxT("Failed to generate an RSA key"));
			return FALSE;
		}
		
		progress.Hide();
		
		if (!PEM_write_bio_RSAPrivateKey(pKeyOutBio, pRSA2, NULL, NULL, 0,
                NULL, NULL))
		{
			ShowSslError(wxT("Failed to write the key to the specified file"));
			return FALSE;
		}			
		
		return TRUE;
	}
};

/*
BEGIN_EVENT_TABLE(wxPanel, SetupWizardPrivateKeyPage)
    EVT_WIZARD_PAGE_CHANGING(wxID_ANY, 
		SetupWizardPrivateKeyPage::OnWizardPageChanging)
END_EVENT_TABLE()
*/

SetupWizard::SetupWizard(ClientConfig *config,
	wxWindow* parent, wxWindowID id)
	: wxWizard(parent, id, wxT("Boxi Setup Wizard"), 
		wxNullBitmap, wxDefaultPosition, 
		wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
	mpConfig = config;

	CRYPTO_malloc_init();
	OpenSSL_add_all_algorithms();
	ERR_load_crypto_strings();
	SSL_load_error_strings();

	GetPageAreaSizer()->SetMinSize(500, 400);
	
	wxWizardPageSimple* pIntroPage = 
		new SetupWizardIntroPage(config, this);
	wxWizardPageSimple* pPrivateKeyPage = 
		new SetupWizardPrivateKeyPage(config, this);
	
	wxWizardPageSimple::Chain(pIntroPage, pPrivateKeyPage);
	
	RunWizard(pIntroPage);
}
