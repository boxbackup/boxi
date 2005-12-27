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
#include <wx/socket.h>

#include <openssl/conf.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include "SetupWizard.h"
#include "ParamPanel.h"

class SetupWizardPage : public wxWizardPageSimple
{
	private:
	wxHtmlWindow* mpText;
	
	public:
	SetupWizardPage(ClientConfig *pConfig, wxWizard* pParent = NULL,
		const wxString& rText = wxT("No page text defined"))
 	: wxWizardPageSimple(pParent)
	{
		mpConfig = pConfig;

		mpSizer = new wxBoxSizer(wxVERTICAL);
		SetSizer(mpSizer);
		mpText = new wxHtmlWindow(this, wxID_ANY, 
			wxDefaultPosition, wxDefaultSize, 
			wxHW_SCROLLBAR_AUTO | wxHW_NO_SELECTION);
		mpText->SetStandardFonts(10);
		mpText->SetPage(rText);
		mpSizer->Add(mpText, 1, wxGROW | wxALL, 0);
	}
 
	protected:
	ClientConfig* mpConfig;
	wxBoxSizer*   mpSizer;
	
	void SetText(const wxString& rNewText)
	{
		mpText->SetPage(rNewText);
	}

	void ShowError(const wxString& rErrorMsg)
	{
		wxMessageBox(rErrorMsg, wxT("Boxi Error"), wxICON_ERROR | wxOK, this);
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
		
		ShowError(msg);
	}
	
	wxString opensslConfigFileName;
	
	CONF* ConfigureSsl()
	{
		wxString opensslConfigPath(X509_get_default_cert_area(), 
			wxConvLibc);
		opensslConfigFileName = wxT("openssl.cnf");
		wxFileName opensslConfigFile(opensslConfigPath, 
			opensslConfigFileName);
			
		CONF* pConfig = NCONF_new(NULL);
		if (!pConfig)
		{
			ShowSslError(wxT("Failed to create an "
				"OpenSSL configuration object"));
			return NULL;
		}
		
		long errline;

		if (!NCONF_load(pConfig, 
			opensslConfigFile.GetFullPath().mb_str(wxConvLibc), &errline))
		{
			wxString msg;
			msg.Printf(wxT("Failed to initialise OpenSSL. Perhaps the "
				"configuration file could not be found or read? (%s)"),
				opensslConfigFile.GetFullPath().c_str());
			ShowSslError(msg);
			NCONF_free(pConfig);
			return NULL;
		}

		OPENSSL_load_builtin_modules();
		
		if (CONF_modules_load(pConfig, NULL, 0) <= 0)
		{
			ShowSslError(wxT("Failed to configure OpenSSL"));
			NCONF_free(pConfig);
			return NULL;
		}

		return pConfig;
	}
	
	void FreeSsl(CONF* pConfig)
	{
		NCONF_free(pConfig);
	}
	
	bool GetCommonName(X509_NAME* pX509SubjectName, wxString* pTarget)
	{
		int commonNameNid = OBJ_txt2nid("commonName");
		if (commonNameNid == NID_undef)
		{
			ShowSslError(wxT("Failed to find NID of "
				"X.509 CommonName attribute"));
			return FALSE;
		}
		
		char commonNameBuf[256];

		X509_NAME_get_text_by_NID(pX509SubjectName, commonNameNid, 
			commonNameBuf, sizeof(commonNameBuf)-1);
		commonNameBuf[sizeof(commonNameBuf)-1] = 0;

		// X509_NAME_free(pX509SubjectName);
		
		*pTarget = wxString(commonNameBuf, wxConvLibc);
		return TRUE;
	}
	
	EVP_PKEY* LoadKey(wxString& rKeyFileName)
	{
		BIO *pKeyBio = BIO_new(BIO_s_file());
		if (!pKeyBio)
		{
			ShowSslError(wxT("Failed to create an OpenSSL BIO "
				"to read the private key"));
			return NULL;
		}
		
		if (BIO_read_filename(pKeyBio, 
			rKeyFileName.mb_str(wxConvLibc).data()) <= 0)
		{
			wxString msg;
			msg.Printf(wxT("Failed to read the private key file (%s)"),
				rKeyFileName.c_str());
			ShowSslError(msg);
			BIO_free(pKeyBio);
			return NULL;
		}
		
		EVP_PKEY* pKey = PEM_read_bio_PrivateKey(pKeyBio, NULL, NULL, NULL);
		BIO_free(pKeyBio);
		
		if (!pKey)
		{
			wxString msg;
			msg.Printf(wxT("Failed to read the private key (%s)"),
				rKeyFileName.c_str());
			ShowSslError(msg);
			return NULL;
		}
		
		return pKey;
	}
	
	void FreeKey(EVP_PKEY* pKey)
	{
		EVP_PKEY_free(pKey);
	}
};

class IntroPage : public SetupWizardPage
{
	public:
	IntroPage(ClientConfig *config,
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

class AccountPage : public SetupWizardPage
{
	private:
	wxTextCtrl* mpServerName;
	IntCtrl*    mpAccountNumber;
	
	public:
	AccountPage(ClientConfig *config,
		wxWizard* parent = NULL)
 	: SetupWizardPage(config, parent, wxT(
		"<html><body><h1>Account Details</h1>"
		"<p>To use a particular server, you must contact the server's operator "
		"and ask them for an account. If they accept, they will give you a "
		"server name and account number. Please enter them here.</p>"
		"</body></html>"))
	{
		wxGridSizer* pLocationSizer = new wxGridSizer(2, 8, 8);
		mpSizer->Add(pLocationSizer, 0, wxGROW | wxTOP, 8);

		wxStaticText* pLabel = new wxStaticText(this, wxID_ANY,
			wxT("Server name:"));
		pLocationSizer->Add(pLabel, 0, wxALIGN_CENTER_VERTICAL, 0);
		
		wxString serverName;
		mpConfig->StoreHostname.GetInto(serverName);
		mpServerName = new wxTextCtrl(this, wxID_ANY, serverName);
		pLocationSizer->Add(mpServerName, 1, 
			wxALIGN_CENTER_VERTICAL | wxGROW, 8);
		
		pLabel = new wxStaticText(this, wxID_ANY, wxT("Account number:"));
		pLocationSizer->Add(pLabel, 0, wxALIGN_CENTER_VERTICAL, 0);
		
		int account = 0;
		mpConfig->AccountNumber.GetInto(account);
		mpAccountNumber = new IntCtrl(this, wxID_ANY, account, "%d");
		pLocationSizer->Add(mpAccountNumber, 1, 
			wxALIGN_CENTER_VERTICAL | wxGROW, 8);
		
		Connect(wxID_ANY,wxEVT_WIZARD_PAGE_CHANGING,
                wxWizardEventHandler(
					AccountPage::OnWizardPageChanging));
	}

	private:
	
	void OnWizardPageChanging(wxWizardEvent& event)
	{
		// always allow moving backwards
		if (!event.GetDirection())
			return;
		
		if (CheckAccountDetails())
		{
			mpConfig->StoreHostname.Set(mpServerName->GetValue());
			mpConfig->AccountNumber.Set(mpAccountNumber->GetValueInt());
		}
		else
		{
			event.Veto();
		}
	}
	
	bool CheckAccountDetails()
	{
		if (! mpAccountNumber->IsValid())
		{
			ShowError(wxT("The account number is empty or not valid"));
			return FALSE;
		}
		
		if (mpAccountNumber->GetValueInt() <= 0)
		{
			ShowError(wxT("The account number is zero or negative"));
			return FALSE;
		}
		
		if (mpAccountNumber->GetValueInt() > 0x7FFFFFFF)
		{
			ShowError(wxT("The account number is too large"));
			return FALSE;
		}
		
		wxIPV4address addr;
		if (!addr.Hostname(mpServerName->GetValue()))
		{
			ShowError(wxT("The server name is not valid"));
			return FALSE;
		}
		
		return TRUE;
	}
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
}

class FileSavingPage : public SetupWizardPage
{
	private:
	wxRadioButton* mpExistingRadio;
	wxRadioButton* mpNewRadio;
	wxTextCtrl* mpLocationText;
	StringProperty& mrProperty;
	
	public:
	FileSavingPage(ClientConfig *pConfig, wxWizard* pParent = NULL, 
		const wxString& rDescriptionText, const wxString& rFileDesc,
		const wxString& rFileNamePattern, StringProperty& rProperty)
 	: SetupWizardPage(pConfig, pParent, rDescriptionText),
	  mrProperty(rProperty)
	{
		wxString label;
		label.Printf(wxT("Existing %s"), rFileDesc.c_str());
		mpExistingRadio = new wxRadioButton(this, wxID_ANY, label,
			wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
		mpSizer->Add(mpExistingRadio, 0, wxGROW | wxTOP, 8);

		label.Printf(wxT("New %s"), rFileDesc.c_str());
		mpNewRadio = new wxRadioButton(this, wxID_ANY, label, 
			wxDefaultPosition, wxDefaultSize);
		mpSizer->Add(mpNewRadio, 0, wxGROW | wxTOP, 8);
		
		wxSizer* pLocationSizer = new wxBoxSizer(wxHORIZONTAL);
		mpSizer->Add(pLocationSizer, 0, wxGROW | wxTOP, 8);
		
		label.Printf(wxT("%s file location:"), rFileDesc.c_str());
		wxStaticText* pLabel = new wxStaticText(this, wxID_ANY, label);
		pLocationSizer->Add(pLabel, 0, wxALIGN_CENTER_VERTICAL, 0);
		
		rProperty.GetInto(mFileNameString);
		mpLocationText = new wxTextCtrl(this, wxID_ANY, mFileNameString);
		pLocationSizer->Add(mpLocationText, 1, 
			wxALIGN_CENTER_VERTICAL | wxLEFT, 8);
		
		wxString fileSpec;
		fileSpec.Printf(wxT("%s (%s)|%s"), rFileDesc.c_str(), 
			rFileNamePattern.c_str(), rFileNamePattern.c_str());
		
		wxString openDialogTitle;
		openDialogTitle.Printf(wxT("Open %s File"), rFileDesc.c_str());
		
		FileSelButton* pFileSelButton = new FileSelButton(
			this, wxID_ANY, mpLocationText, fileSpec, openDialogTitle);
		pLocationSizer->Add(pFileSelButton, 0, wxGROW | wxLEFT, 8);
		
		Connect(wxID_ANY,wxEVT_WIZARD_PAGE_CHANGING,
                wxWizardEventHandler(
					FileSavingPage::OnWizardPageChanging));
	}
	
	const wxString&   GetFileNameString() { return mFileNameString; }
	const wxFileName& GetFileName()       { return mFileName; }
	void SetFileName(const wxString& rFileName)
	{
		mFileNameString = rFileName;
		mFileName = wxFileName(rFileName);
		mpLocationText->SetValue(rFileName);
	}
	
	private:
	wxString   mFileNameString;
	wxFileName mFileName;
	
	void OnWizardPageChanging(wxWizardEvent& event)
	{
		// always allow moving backwards
		if (!event.GetDirection())
			return;

		bool isOk = FALSE;
		
		mFileNameString = mpLocationText->GetValue();
		
		if (mFileNameString.IsEmpty())
		{
			ShowError(wxT("You must specify a location for the file!"));
		}
		else
		{
			mFileName = wxFileName(mFileNameString);
			
			if (mpExistingRadio->GetValue())
			{
				isOk = CheckExistingFile();
			}
			else if (mpNewRadio->GetValue())
			{
				isOk = CreateNewFile();
			}
		}
		
		if (isOk) 
		{
			mrProperty.Set(mFileNameString);
		}
		else
		{
			event.Veto();
		}
	}
	
	bool CheckExistingFile()
	{	
		if (!wxFileName::FileExists(mFileNameString))
		{
			ShowError(wxT("The specified file was not found."));
			return FALSE;
		}
	
		if (!wxFile::Access(mFileNameString, wxFile::read))
		{
			ShowError(wxT("The specified file is not readable."));
			return FALSE;
		}
		
		return CheckExistingFileImpl();
	}
	
	virtual bool CheckExistingFileImpl() = 0;
		
	bool CreateNewFile()
	{	
		if (wxFileName::FileExists(mFileNameString))
		{
			int result = wxMessageBox(wxT("The specified file "
				"already exists!\n"
				"\n"
				"If you overwrite an important file, "
				"you may lose access to the server and your backups. "
				"Are you REALLY SURE you want to overwrite it?\n"
				"\n"
				"This operation CANNOT be undone!"), 
				wxT("Boxi Warning"), wxICON_WARNING | wxYES_NO, this);
			
			if (result != wxYES)
				return FALSE;

			if (! wxFile::Access(mFileNameString, wxFile::write))
			{
				ShowError(wxT("The specified file is not writable."));
				return FALSE;
			}
		}
		else if (!wxFileName::DirExists(mFileName.GetPath()))
		{
			ShowError(wxT("The directory where you want to create the "
				"file does not exist, so the file cannot be created."));
			return FALSE;
		}
		else // does not exist yet
		{
			if (! wxFile::Access(mFileName.GetPath(), wxFile::write))
			{
				ShowError(wxT("The directory where you want to create the "
					"file is read-only, so the file cannot be created."));
				return FALSE;
			}
		}
		
		return CreateNewFileImpl();
	}
	
	virtual bool CreateNewFileImpl() = 0;
};

class PrivateKeyPage : public FileSavingPage
{
	public:
	PrivateKeyPage(ClientConfig *pConfig, wxWizard* pParent)
 	: FileSavingPage(pConfig, pParent, wxT(
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
		"</body></html>"),
		wxT("Private Key"), wxT("*-key.pem"), pConfig->PrivateKeyFile)
	{ }
	
	bool CheckExistingFileImpl()
	{	
		CONF* pConfig = ConfigureSsl();
		if (!pConfig)
		{
			return FALSE;
		}
		
		bool isOk = CheckExistingKeyWithOpenSslConfigured(pConfig);
		
		FreeSsl(pConfig);
		
		return isOk;
	}
	
	bool CheckExistingKeyWithOpenSslConfigured(CONF* pConfig)
	{
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
		if (BIO_read_filename(pKeyBio, 
			GetFileNameString().mb_str(wxConvLibc).data()) <= 0)
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
			ShowError(wxT("This private key is protected with a passphrase. "
				"It cannot be used with Box Backup."));
			return FALSE;
		}
		
		return TRUE;
	}

	bool CreateNewFileImpl()
	{	
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
		char* filename = strdup(GetFileNameString().mb_str(wxConvLibc).data());		
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

class CertRequestPage : public FileSavingPage, ConfigChangeListener
{
	private:
	wxString mKeyFileName;
	int mAccountNumber;
	
	public:
	CertRequestPage(ClientConfig *pConfig, wxWizard* pParent)
 	: FileSavingPage(pConfig, pParent, wxT(
		"<html><body><h1>Certificate Request</h1>"
		"<p>You must send a certificate request to the server operator.</p>"
		"<p>If you already have a certificate request, you can continue to "
		"use it unless the server administrator tells you to generate "
		"a new one.</p>"
		"<p>If you want to use an existing certificate request, select the "
		"<b>Existing Certificate Request</b> option below, and tell Boxi "
		"where to find the file.</p>"
		"<p>Otherwise, select the <b>New Certificate Request</b> option, "
		"and tell Boxi where the new file should be stored.</p>"
		"</body></html>"),
		wxT("Certificate Request"), wxT("*-csr.pem"), pConfig->CertRequestFile)
	{
		mpConfig->AddListener(this);
		SetDefaultValues();
	}
	
	~CertRequestPage() { mpConfig->RemoveListener(this); }
	
	void SetDefaultValues()
	{
		mpConfig->AccountNumber.GetInto(mAccountNumber);
		
		if (!mpConfig->CertRequestFile.IsConfigured() && 
			 mpConfig->PrivateKeyFile.IsConfigured())
		{
			if (!mpConfig->PrivateKeyFile.GetInto(mKeyFileName))
			{
				ShowError(wxT("The private key filename is not set, "
					"but it should be!"));
				return;
			}
			
			wxString oldSuffix = wxT("-key.pem");
			wxString baseName = mKeyFileName;
			
			if (mKeyFileName.Right(oldSuffix.Length()).IsSameAs(oldSuffix))
			{
				baseName = mKeyFileName.Left(mKeyFileName.Length() - 
					oldSuffix.Length());
			}
			
			wxString reqFileName;
			reqFileName.Printf(wxT("%s-csr.pem"), baseName.c_str());
			
			SetFileName(reqFileName);
		}
	}
	
	void NotifyChange() 
	{
		SetDefaultValues();
	}
	
	bool CheckExistingFileImpl()
	{
		BIO* pRequestBio = BIO_new(BIO_s_file());
		if (!pRequestBio)
		{
			ShowSslError(wxT("Failed to create an OpenSSL BIO "
				"to read the certificate request"));
			return FALSE;
		}
		
		bool isOk = CheckCertRequestWithBio(pRequestBio);
		
		BIO_free(pRequestBio);
		
		return isOk;
	}
	
	bool CheckCertRequestWithBio(BIO* pRequestBio)
	{
		wxCharBuffer buf = GetFileNameString().mb_str(wxConvLibc);
		
		if (BIO_read_filename(pRequestBio, buf.data()) <= 0)
		{
			ShowSslError(wxT("Failed to set the BIO filename to the "
				"certificate request file"));
			return FALSE;			
		}
		
		X509_REQ* pRequest = PEM_read_bio_X509_REQ(pRequestBio, NULL, NULL, NULL);
		if (!pRequest)
		{
			ShowSslError(wxT("Failed to read the certificate request file"));
			return FALSE;			
		}
		
		bool isOk = CheckCertRequest(pRequest);
		
		X509_REQ_free(pRequest);
		
		return isOk;
	}
	
	bool CheckCertRequest(X509_REQ* pRequest)
	{
		wxString keyFileName;
		if (!mpConfig->PrivateKeyFile.GetInto(keyFileName))
		{
			ShowError(wxT("The private key filename is not set, "
				"but it should be!"));
			return FALSE;
		}
		
		EVP_PKEY* pKey = LoadKey(keyFileName);

		bool isOk = CheckCertRequestWithKey(pRequest, pKey);
		
		FreeKey(pKey);
		
		return isOk;
	}

	bool CheckCertRequestWithKey(X509_REQ* pRequest, EVP_PKEY* pKey)
	{
		wxString commonNameActual;
		if (!GetCommonName(X509_REQ_get_subject_name(pRequest), &commonNameActual))
		{
			return FALSE;
		}
		
		wxString commonNameExpected;
		commonNameExpected.Printf(wxT("BACKUP-%d"), mAccountNumber);
		
		if (!commonNameExpected.IsSameAs(commonNameActual))
		{
			wxString msg;
			msg.Printf(wxT("The certificate request does not match your "
				"account number. The common name of the certificate request "
				"should be '%s', but is actually '%s'."),
				commonNameExpected.c_str(), commonNameActual.c_str());
			ShowError(msg);
			return FALSE;
		}

		/*		
		EVP_PKEY* pRequestKey = X509_REQ_get_pubkey(pRequest);
		EVP_PKEY_free(pRequestKey);
		*/
		
		if (!X509_REQ_verify(pRequest, pKey))
		{
			ShowSslError(wxT("Failed to verify signature on "
				"certificate request. Perhaps the private key does not match "
				"the certificate request?"));
			return FALSE;
		}
		
		return TRUE;
	}

	bool CreateNewFileImpl()
	{
		CONF* pConfig = ConfigureSsl();
		if (!pConfig)
		{
			return FALSE;
		}
		
		bool isOk = GenerateCertRequestWithOpenSslConfigured(pConfig);
		
		FreeSsl(pConfig);
		
		return isOk;
	}
	
	bool GenerateCertRequestWithOpenSslConfigured(CONF* pConfig)
	{
		char* pExtensions = NCONF_get_string(pConfig, "req", "x509_extensions");
		if (pExtensions) 
		{
			/* Check syntax of file */
			X509V3_CTX ctx;
			X509V3_set_ctx_test(&ctx);
			X509V3_set_nconf(&ctx, pConfig);
			
			if(!X509V3_EXT_add_nconf(pConfig, &ctx, pExtensions, NULL)) 
			{
				wxString msg;
				msg.Printf(wxT("Failed to load certificate extensions "
					"section (%s) specified in OpenSSL configuration file "
					"(%s)"), pExtensions, opensslConfigFileName.c_str());
				ShowSslError(msg);
				return FALSE;
			}
        }
		else
		{
			ERR_clear_error();
		}
		
        if(!ASN1_STRING_set_default_mask_asc("nombstr")) 
		{
			ShowSslError(wxT("Failed to set the OpenSSL string mask to "
				"'nombstr'"));
			return FALSE;
		}

		BIO* pRequestBio = BIO_new(BIO_s_file());
		EVP_PKEY* pKey = LoadKey(mKeyFileName);
		
		bool isOk = GenerateCertRequestWithKeyAndBio(pConfig, pKey, pRequestBio);
		
		FreeKey(pKey);
		BIO_free(pRequestBio);
		
		return isOk;
	}
	
	bool GenerateCertRequestWithKeyAndBio(CONF* pConfig, EVP_PKEY* pKey, 
		BIO* pRequestBio)
	{
		X509_REQ* pRequest = X509_REQ_new();
		if (!pRequest)
		{
			ShowSslError(wxT("Failed to create a new OpenSSL request object"));
			return FALSE;
		}
		
		bool isOk = GenerateCertRequestWithObject(pConfig, pRequest, pKey, 
			pRequestBio);
		
		X509_REQ_free(pRequest);
		
		return isOk;
	}
	
	bool GenerateCertRequestWithObject(CONF* pConfig, X509_REQ* pRequest, 
		EVP_PKEY* pKey, BIO* pRequestBio)
	{
		if (!X509_REQ_set_version(pRequest,0L))
		{
			ShowSslError(wxT("Failed to set version in OpenSSL request object"));
			return FALSE;
		}
		
		int commonNameNid = OBJ_txt2nid("commonName");
		if (commonNameNid == NID_undef)
		{
			ShowSslError(wxT("Failed to find node ID of "
				"X.509 CommonName attribute"));
			return FALSE;
		}
		
		wxString certSubject;
		certSubject.Printf(wxT("BACKUP-%d"), mAccountNumber);
		
		X509_NAME* pX509SubjectName = X509_REQ_get_subject_name(pRequest);

		unsigned char* subject = (unsigned char *)strdup(
			certSubject.mb_str(wxConvLibc).data());
		int result = X509_NAME_add_entry_by_NID(pX509SubjectName, commonNameNid, 
			MBSTRING_ASC, subject, -1, -1, 0);
		free(subject);
		
		// X509_NAME_free(pX509SubjectName);
		
		if (!result)
		{
			ShowSslError(wxT("Failed to add common name to "
				"certificate request"));
			return FALSE;
		}
		
		if (!X509_REQ_set_pubkey(pRequest, pKey))
		{
			ShowSslError(wxT("Failed to set public key of "
				"certificate request"));
			return FALSE;
		}
		
		X509V3_CTX ext_ctx;
		X509V3_set_ctx(&ext_ctx, NULL, NULL, pRequest, NULL, 0);
		X509V3_set_nconf(&ext_ctx, pConfig);
		
		char *pReqExtensions = NCONF_get_string(pConfig, "section", 
			"req_extensions");
		if (pReqExtensions)
		{
			X509V3_CTX ctx;
			X509V3_set_ctx_test(&ctx);
			X509V3_set_nconf(&ctx, pConfig);
			
			if (!X509V3_EXT_REQ_add_nconf(pConfig, &ctx, pReqExtensions, 
				pRequest)) 
			{
				wxString msg;
				msg.Printf(wxT("Failed to load request extensions "
					"section (%s) specified in OpenSSL configuration file "
					"(%s)"), pReqExtensions, opensslConfigFileName.c_str());
				ShowSslError(msg);
				return FALSE;
			}
		}
		else
		{
			ERR_clear_error();
		}

		const EVP_MD* pDigestAlgo = EVP_get_digestbyname("sha1");
		if (!pDigestAlgo)
		{
			ShowSslError(wxT("Failed to find OpenSSL digest algorithm SHA1"));
			return FALSE;
		}
		
		if (!X509_REQ_sign(pRequest, pKey, pDigestAlgo))
		{
			ShowSslError(wxT("Failed to sign certificate request"));
			return FALSE;
		}
		
		if (!X509_REQ_verify(pRequest, pKey))
		{
			ShowSslError(wxT("Failed to verify signature on "
				"certificate request"));
			return FALSE;
		}
		
		char* filename = strdup(GetFileNameString().mb_str(wxConvLibc).data());
		result = BIO_write_filename(pRequestBio, filename);
		free(filename);
		
		if (result <= 0)
		{
			ShowSslError(wxT("Failed to set certificate request "
				"output filename"));
			return FALSE;
		}
		
		if (!PEM_write_bio_X509_REQ(pRequestBio, pRequest))
		{
			ShowSslError(wxT("Failed to write certificate request file"));
			return FALSE;
		}
		
		return TRUE;
	}
};

class CryptoKeyPage : public FileSavingPage, ConfigChangeListener
{
	public:
	CryptoKeyPage(ClientConfig *pConfig, wxWizard* pParent)
 	: FileSavingPage(pConfig, pParent, wxT(
		"<html><body><h1>Encryption Key</h1>"
		"<p>You need an encryption key to protect your backups. "
		"The encryption key is stored in a file on your hard disk.</p>"
		"<p>Without this key, nobody can read your backups, "
		"<strong>NOT EVEN YOU</strong>. You <strong>MUST</strong> "
		"keep a copy of this key somewhere safe, or your backups "
		"will be <strong>useless!</strong></p>"
		"<p>If you already have an encryption key, you should continue to "
		"use it unless it has been compromised (stolen).</p>"
		"<p>If you want to use an existing key, select the "
		"<b>Existing Encryption Key</b> option below, and tell Boxi "
		"where to find the key file.</p>"
		"<p>Otherwise, select the <b>New Encryption Key</b> option, "
		"and tell Boxi where the new key file should be stored.</p>"
		"</body></html>"),
		wxT("Encryption Key"), wxT("*-FileEncKeys.raw"), 
		pConfig->KeysFile)
	{
		mpConfig->AddListener(this);
		SetDefaultValues();
	}
	
	~CryptoKeyPage() { mpConfig->RemoveListener(this); }
	
	void SetDefaultValues()
	{
		if (!mpConfig->KeysFile.IsConfigured() && 
			 mpConfig->PrivateKeyFile.IsConfigured())
		{
			wxString privateKeyFileName;
			
			if (!mpConfig->PrivateKeyFile.GetInto(privateKeyFileName))
			{
				ShowError(wxT("The private key filename is not set, "
					"but it should be!"));
				return;
			}
			
			wxString oldSuffix = wxT("-key.pem");
			wxString baseName = privateKeyFileName;
			
			if (privateKeyFileName.Right(oldSuffix.Length()).IsSameAs(oldSuffix))
			{
				baseName = privateKeyFileName.Left(privateKeyFileName.Length() - 
					oldSuffix.Length());
			}
			
			wxString encFileName;
			encFileName.Printf(wxT("%s-FileEncKeys.raw"), baseName.c_str());
			SetFileName(encFileName);
		}
	}
	
	void NotifyChange() 
	{
		SetDefaultValues();
	}

	bool CheckExistingFileImpl()
	{
		wxFile keysFile(GetFileName().GetFullPath());
		if (keysFile.Length() != 1024)
		{
			ShowError(wxT("The specified encryption key file "
				"is not valid (should be 1024 bytes long)"));
			return FALSE;
		}

		return TRUE;
	}

	bool CreateNewFileImpl()
	{
		BIO* pKeyOutBio = BIO_new(BIO_s_file());
		if (!pKeyOutBio)
		{
			ShowSslError(wxT("Failed to create an OpenSSL BIO to write the "
				"new encryption key"));
			return FALSE;
		}
		
		char* filename = strdup(
			GetFileNameString().mb_str(wxConvLibc).data());
		int result = BIO_write_filename(pKeyOutBio, filename);
		free(filename);
		
		if (!result)
		{
			ShowSslError(wxT("Failed to set the filename of the OpenSSL BIO "
				"to write the new encryption key"));
			BIO_free(pKeyOutBio);
			return FALSE;
		}
		
		unsigned char bytes[1024];
		if (RAND_bytes(bytes, sizeof(bytes)) <= 0)
		{
			ShowSslError(wxT("Failed to generate random data for the "
				"new encryption key"));
			BIO_free(pKeyOutBio);
			return FALSE;
		}
		
		if (BIO_write(pKeyOutBio, bytes, sizeof(bytes)) != sizeof(bytes))
		{
			ShowSslError(wxT("Failed to write random data to the "
				"new encryption key file"));
			BIO_free(pKeyOutBio);
			return FALSE;
		}

		BIO_flush(pKeyOutBio);
		BIO_free(pKeyOutBio);

		return TRUE;
	}
};

class BackedUpPage : public SetupWizardPage, ConfigChangeListener
{
	private:
	wxCheckBox* mpBackedUp;
	
	public:
	BackedUpPage(ClientConfig *config,
		wxWizard* parent = NULL)
 	: SetupWizardPage(config, parent, wxT(
		"<html><body><h1>Back Up Your Keys</h1>"
		"<p>Your backups will be protected by "
		"<strong>strong encryption</strong>. "
		"Without the key, <strong>nobody</strong> can read them: "
		"not us, not you, not the server operator, <strong>NOBODY</strong>.</p>"
		"<p>If your computer is stolen or corrupted, you may lose access to "
		"the encryption key stored on the hard disk. Unless you "
		"<strong>keep safe copies</strong> of this key, you will "
		"<strong>NOT</strong> be able to use your backups.</p>"
		"<p>We recommend that you write your keys onto at least "
		"<strong>TWO</strong> CD-ROMs, keep one safe yourself, "
		"and give the others to a friend for safe keeping.</p>"
		"<p><strong>Please back up your keys NOW!</strong></p>"		
		"</body></html>"))
	{
		mpBackedUp = new wxCheckBox(this, wxID_ANY, 
			wxT("I have backed up my keys in two locations."));
		mpSizer->Add(mpBackedUp, 0, wxGROW | wxTOP, 8);

		Connect(wxID_ANY,wxEVT_WIZARD_PAGE_CHANGING,
                wxWizardEventHandler(
					BackedUpPage::OnWizardPageChanging));
	}

	private:
	
	void OnWizardPageChanging(wxWizardEvent& event)
	{
		// always allow moving backwards
		if (!event.GetDirection())
			return;

		if (!mpBackedUp->GetValue())
		{
			ShowError(wxT("You must check the box to say that you have "
				"backed up your keys before proceeding"));
			event.Veto();
		}
	}
	
};

class CertificatePage : public SetupWizardPage, ConfigChangeListener
{
	private:
	wxTextCtrl* mpCertFileLoc;
	wxTextCtrl* mpCAFileLoc;
	
	public:
	CertificatePage(ClientConfig *pConfig,
		wxWizard* pParent = NULL)
 	: SetupWizardPage(pConfig, pParent, wxT(""))
	{
		wxFlexGridSizer* pLocationSizer = new wxFlexGridSizer(3, 8, 8);
		pLocationSizer->AddGrowableCol(1);
		mpSizer->Add(pLocationSizer, 0, wxGROW | wxTOP, 8);

		wxStaticText* pLabel = new wxStaticText(this, wxID_ANY,
			wxT("Certificate file:"));
		pLocationSizer->Add(pLabel, 0, wxALIGN_CENTER_VERTICAL, 0);
		
		wxString certFile;
		mpConfig->CertificateFile.GetInto(certFile);
		mpCertFileLoc = new wxTextCtrl(this, wxID_ANY, certFile);
		pLocationSizer->Add(mpCertFileLoc, 1, 
			wxALIGN_CENTER_VERTICAL | wxGROW, 0);
			
		FileSelButton* pFileSel = new FileSelButton(this, wxID_ANY, 
			mpCertFileLoc, wxT("Certificate files (*-cert.pem)|*-cert.pem"));
		pLocationSizer->Add(pFileSel, 0, wxALIGN_CENTER_VERTICAL | wxGROW, 0);
				
		pLabel = new wxStaticText(this, wxID_ANY, wxT("CA file:"));
		pLocationSizer->Add(pLabel, 0, wxALIGN_CENTER_VERTICAL, 0);
		
		wxString caFile;
		mpConfig->TrustedCAsFile.GetInto(caFile);
		mpCAFileLoc = new wxTextCtrl(this, wxID_ANY, caFile);
		pLocationSizer->Add(mpCAFileLoc, 1, 
			wxALIGN_CENTER_VERTICAL | wxGROW, 0);
		
		pFileSel = new FileSelButton(this, wxID_ANY, mpCAFileLoc, 
			wxT("CA files (serverCA.pem)|serverCA.pem"));
		pLocationSizer->Add(pFileSel, 0, wxALIGN_CENTER_VERTICAL | wxGROW, 0);

		Connect(wxID_ANY,wxEVT_WIZARD_PAGE_CHANGING,
                wxWizardEventHandler(
					CertificatePage::OnWizardPageChanging));

		mpConfig->AddListener(this);
		UpdateText();
	}
	
	~CertificatePage() { mpConfig->RemoveListener(this); }

	void NotifyChange() { UpdateText(); }
	
	void UpdateText()
	{
		wxString certReqPath;
		mpConfig->CertRequestFile.GetInto(certReqPath);
		
		wxString keyPath;
		mpConfig->KeysFile.GetInto(keyPath);
		
		wxFileName keyPathName(keyPath);
		
		wxString text;
		text.Printf(wxT(
			"<html><body><h1>Certificate File</h1>"
			"<p>You need a certificate to connect to the server. "
			"The certificate will be supplied by the server operator.</p>"
			"<p>Please follow the operator's instructions to send the "
			"certificate request file (<strong>%s</strong>) to them.</p>"
			"<p>They should send you back two files: a "
			"<strong>certificate</strong> and a <strong>CA</strong> file. "
			"Save both these files in the same directory as your key files "
			"(<strong>%s</strong>).</p>"
			"<p>When you have these files, tell Boxi where to find them "
			"using the boxes below.</p>"),
			certReqPath.c_str(), keyPathName.GetPath().c_str());
		SetText(text);
	}
	
	void OnWizardPageChanging(wxWizardEvent& event)
	{
		// always allow moving backwards
		if (!event.GetDirection())
			return;

		bool isOk = CheckServerFiles();
		
		if (isOk) 
		{
			mpConfig->CertificateFile.Set(mpCertFileLoc->GetValue());
			mpConfig->TrustedCAsFile .Set(mpCAFileLoc  ->GetValue());
		}
		else
		{
			event.Veto();
		}
	}
	
	bool CheckServerFiles()
	{
		wxString certFile = mpCertFileLoc->GetValue();
		
		if (!wxFileName::FileExists(certFile))
		{
			ShowError(wxT("The certificate file does not exist!"));
			return FALSE;
		}
		
		if (!wxFile::Access(certFile, wxFile::read))
		{
			ShowError(wxT("The certificate file is not readable!"));
			return FALSE;
		}

		wxString caFile = mpCAFileLoc->GetValue();
		
		if (!wxFileName::FileExists(caFile))
		{
			ShowError(wxT("The CA file does not exist!"));
			return FALSE;
		}
		
		if (!wxFile::Access(caFile, wxFile::read))
		{
			ShowError(wxT("The CA file is not readable!"));
			return FALSE;
		}

		return CheckCertificate();
	}
	
	bool CheckCertificate()
	{
		BIO* pCertBio = BIO_new(BIO_s_file());
		if (!pCertBio)
		{
			ShowSslError(wxT("Failed to create an OpenSSL BIO "
				"to read the certificate"));
			return FALSE;
		}
		
		bool isOk = CheckCertWithBio(pCertBio);
		
		BIO_free(pCertBio);
		
		return isOk;
	}
	
	bool CheckCertWithBio(BIO* pCertBio)
	{
		wxCharBuffer buf = mpCertFileLoc->GetValue().mb_str(wxConvLibc);
		
		if (BIO_read_filename(pCertBio, buf.data()) <= 0)
		{
			ShowSslError(wxT("Failed to set the BIO filename to the "
				"certificate file"));
			return FALSE;			
		}
		
		X509* pCert = PEM_read_bio_X509(pCertBio, NULL, NULL, NULL);
		if (!pCert)
		{
			ShowSslError(wxT("Failed to read the certificate file"));
			return FALSE;			
		}
		
		bool isOk = CheckCertSubject(pCert);
		
		X509_free(pCert);
		
		return isOk;
	}
	
	bool CheckCertSubject(X509* pCert)
	{
		wxString commonNameActual;
		if (!GetCommonName(X509_get_subject_name(pCert), &commonNameActual))
		{
			return FALSE;
		}

		int accountNumber;
		if (!mpConfig->AccountNumber.GetInto(accountNumber))
		{
			ShowError(wxT("The account number is not configured, "
				"but it should be!"));
			return FALSE;
		}
		
		wxString commonNameExpected;
		commonNameExpected.Printf(wxT("BACKUP-%d"), accountNumber);

		if (!commonNameExpected.IsSameAs(commonNameActual))
		{
			wxString msg;
			msg.Printf(wxT("The certificate request does not match your "
				"account number. The common name of the certificate request "
				"should be '%s', but is actually '%s'."),
				commonNameExpected.c_str(), commonNameActual.c_str());
			ShowError(msg);
			return FALSE;
		}

		wxString keyFileName;
		if (!mpConfig->PrivateKeyFile.GetInto(keyFileName))
		{
			ShowError(wxT("The private key file is not configured, "
				"but it should be!"));
			return FALSE;
		}

		EVP_PKEY* pSubjectPublicKey = X509_get_pubkey(pCert);
		if (!pSubjectPublicKey)
		{
			ShowSslError(wxT("Failed to extract the subject's public key"
				"from the certificate"));
			return FALSE;
		}
		
		EVP_PKEY* pMyPublicKey = LoadKey(keyFileName);
		if (!pMyPublicKey)
		{
			EVP_PKEY_free(pSubjectPublicKey);
			return FALSE;
		}
		
		bool isOk = EnsureSamePublicKeys(pSubjectPublicKey, pMyPublicKey);
		
		EVP_PKEY_free(pMyPublicKey);
		EVP_PKEY_free(pSubjectPublicKey);

		return isOk;
	}
	
	bool EnsureSamePublicKeys(EVP_PKEY* pActualKey, EVP_PKEY* pExpectKey)
	{	
		BIO* pActualBio = BIO_new(BIO_s_mem());
		if (!pActualBio)
		{
			ShowSslError(wxT("Failed to allocate BIO for key data"));
			return FALSE;
		}

		BIO* pExpectBio = BIO_new(BIO_s_mem());
		if (!pExpectBio)
		{
			ShowSslError(wxT("Failed to allocate BIO for key data"));
			BIO_free(pActualBio);
			return FALSE;
		}
		
		bool isOk = EnsureSamePublicKeysWithBios(pActualKey, pExpectKey,
			pActualBio, pExpectBio);
		
		BIO_free(pExpectBio);
		BIO_free(pActualBio);
		
		return isOk;
	}
	
	bool EnsureSamePublicKeysWithBios(EVP_PKEY* pActualKey, 
		EVP_PKEY* pExpectKey, BIO* pActualBio, BIO* pExpectBio)
	{
		if (!PEM_write_bio_PUBKEY(pActualBio, pActualKey))
		{
			ShowSslError(wxT("Failed to write public key to memory buffer"));
			return FALSE;
		}

		if (!PEM_write_bio_PUBKEY(pExpectBio, pExpectKey))
		{
			ShowSslError(wxT("Failed to write public key to memory buffer"));
			return FALSE;
		}
		
		int actSize = BIO_get_mem_data(pActualBio, NULL);
		int expSize = BIO_get_mem_data(pExpectBio, NULL);
		
		if (expSize != actSize)
		{
			ShowError(wxT("The certificate file does not match "
				"your private key."));
			return FALSE;
		}

		unsigned char* pExpBuffer = new unsigned char [expSize];
		if (!pExpBuffer)
		{
			ShowSslError(wxT("Failed to allocate space for private key data"));
			return FALSE;
		}
		
		unsigned char* pActBuffer = new unsigned char [actSize];
		if (!pActBuffer)
		{
			ShowSslError(wxT("Failed to allocate space for private key data"));
			delete [] pExpBuffer;
			return FALSE;
		}
			
		bool isOk = EnsureSamePublicKeysWithBuffers(pActualBio, pExpectBio,
			pActBuffer, pExpBuffer, actSize);
		
		delete [] pActBuffer;
		delete [] pExpBuffer;

		return isOk;
	}
	
	bool EnsureSamePublicKeysWithBuffers(BIO* pActualBio, BIO* pExpectBio,
		unsigned char* pActBuffer, unsigned char* pExpBuffer, int size)
	{
		if (BIO_read(pActualBio, pActBuffer, size) != size)
		{
			ShowSslError(wxT("Failed to read enough key data"));
			return FALSE;
		}
		
		if (BIO_read(pExpectBio, pExpBuffer, size) != size)
		{
			ShowSslError(wxT("Failed to read enough key data"));
			return FALSE;
		}
		
		if (memcmp(pActBuffer, pExpBuffer, size) != 0)
		{
			ShowError(wxT("The certificate file does not match "
				"your private key."));
			return FALSE;
		}

		return TRUE;
	}
};

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
	
	wxWizardPageSimple* pIntroPage = new IntroPage(config, this);
	wxWizardPageSimple* pAccountPage = new AccountPage(config, this);
	wxWizardPageSimple::Chain(pIntroPage, pAccountPage);
	
	wxWizardPageSimple* pPrivateKeyPage = new PrivateKeyPage(config, this);
	wxWizardPageSimple::Chain(pAccountPage, pPrivateKeyPage);
	
	wxWizardPageSimple* pCertRequestPage = new CertRequestPage(config, this);
	wxWizardPageSimple::Chain(pPrivateKeyPage, pCertRequestPage);	
	
	wxWizardPageSimple* pCryptoKeyPage = new CryptoKeyPage(config, this);
	wxWizardPageSimple::Chain(pCertRequestPage, pCryptoKeyPage);
	
	wxWizardPageSimple* pBackedUpPage = new BackedUpPage(config, this);
	wxWizardPageSimple::Chain(pCryptoKeyPage,  pBackedUpPage);

	wxWizardPageSimple* pCertificatePage = new CertificatePage(config, this);
	wxWizardPageSimple::Chain(pBackedUpPage, pCertificatePage);

	RunWizard(pIntroPage);
}
