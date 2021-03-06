/***************************************************************************
 *            TestWizard.cc
 *
 *  Tue May  9 19:44:11 2006
 *  Copyright 2006-2007 Chris Wilson
 *  Email chris-boxisource@qwirx.com
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
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "SandBox.h"

#include <wx/file.h>
#include <wx/progdlg.h>

#include <cppunit/TestSuite.h>
#include <cppunit/extensions/HelperMacros.h>

#include <openssl/conf.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>

#include "BoxiApp.h"
#include "ClientConfig.h"
#include "MainFrame.h"
#include "SetupWizard.h"
#include "SslConfig.h"
#include "TestWizard.h"

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(TestWizard, "WxGuiTest");

CppUnit::Test *TestWizard::suite()
{
	CppUnit::TestSuite *suiteOfTests =
		new CppUnit::TestSuite("TestWizard");
	suiteOfTests->addTest(
		new CppUnit::TestCaller<TestWizard>(
			"TestWizard",
			&TestWizard::RunTest));
	return suiteOfTests;
}

static int numCheckedTest = 0;

static wxString MySslProgressCallbackMessage(int n)
{
	wxString msg;
	msg.Printf(_("Generating a fake server private key (2048 bits).\n"
			"This may take a minute.\n\n"
			"(%d potential primes checked)"), n);
	return msg;
}

static void MySslProgressCallbackTest(int p, int n, void* cbinfo)
{
	wxProgressDialog* pProgress = (wxProgressDialog *)cbinfo;
	pProgress->Pulse(MySslProgressCallbackMessage(++numCheckedTest));
}

X509* TestWizard::LoadCertificate
(const wxFileName& rCertFile)
{
	BIO* pCertBio = BIO_new(BIO_s_file());
	CPPUNIT_ASSERT(pCertBio);
	
	wxCharBuffer buf = rCertFile.GetFullPath().mb_str(wxConvBoxi);
	int result = BIO_read_filename(pCertBio, buf.data());
	CPPUNIT_ASSERT(result > 0);
	
	X509* pCert = PEM_read_bio_X509_AUX(pCertBio, NULL, NULL, NULL);
	CPPUNIT_ASSERT(pCert);
	
	return pCert; // caller must free with X509_free
}

void TestWizard::SignRequestWithKey
(
	const wxFileName& rRequestFile, 
	const wxFileName& rKeyFile, 
	const wxFileName& rIssuerCertFile, 
	const wxFileName& rCertFile
)
{
	CPPUNIT_ASSERT(rRequestFile.FileExists());
	CPPUNIT_ASSERT(rKeyFile.FileExists());
	CPPUNIT_ASSERT(!rIssuerCertFile.IsOk() || rIssuerCertFile.FileExists());
	CPPUNIT_ASSERT(!rCertFile.FileExists());
	
	X509V3_CTX ctx2;
	X509V3_set_ctx_test(&ctx2);
	
	/*
	CONF *extconf = NULL;
	X509V3_set_nconf(&ctx2, extconf);
	
	char* extsect = "v3_ca";
	
	CPPUNIT_ASSERT_MESSAGE("Error loading extension section",
		X509V3_EXT_add_nconf(extconf, &ctx2, extsect, NULL));
	*/
	
	BIO* pRequestBio = BIO_new(BIO_s_file());
	CPPUNIT_ASSERT_MESSAGE("Failed to create an OpenSSL BIO", pRequestBio);
	
	wxCharBuffer buf = rRequestFile.GetFullPath().mb_str(wxConvBoxi);
	char* filename = strdup(buf.data());
	CPPUNIT_ASSERT_MESSAGE("Failed to read certificate signing request",
		BIO_read_filename(pRequestBio, filename) > 0);
	free(filename);
	
	X509_REQ* pRequest = PEM_read_bio_X509_REQ(pRequestBio,NULL,NULL,NULL);
	CPPUNIT_ASSERT_MESSAGE("Failed to parse certificate signing request",
		pRequest);
	
	BIO_free(pRequestBio);
	pRequestBio = NULL;

	CPPUNIT_ASSERT_MESSAGE("The certificate does not appear to "
		"contain a public key", pRequest->req_info);

	CPPUNIT_ASSERT_MESSAGE("The certificate does not appear to "
		"contain a public key", pRequest->req_info->pubkey);

	CPPUNIT_ASSERT_MESSAGE("The certificate does not appear to "
		"contain a public key", 
		pRequest->req_info->pubkey->public_key);

	CPPUNIT_ASSERT_MESSAGE("The certificate does not appear to "
		"contain a public key", 
		pRequest->req_info->pubkey->public_key->data);

	EVP_PKEY* pRequestKey = X509_REQ_get_pubkey(pRequest);
	CPPUNIT_ASSERT_MESSAGE("Error unpacking request public key", pRequestKey);
	
	CPPUNIT_ASSERT_MESSAGE("Signature verification error",
		X509_REQ_verify(pRequest, pRequestKey) >= 0);

	CPPUNIT_ASSERT_MESSAGE("Signature did not match the "
		"certificate request",
		X509_REQ_verify(pRequest, pRequestKey) != 0);

	wxString msg;
	EVP_PKEY* pSigKey = LoadKey(rKeyFile.GetFullPath(), &msg);
	CPPUNIT_ASSERT_MESSAGE("Failed to load private key", pSigKey);	

	X509* pIssuerCert = NULL;
	if (rIssuerCertFile.IsOk())
	{
		pIssuerCert = LoadCertificate(rIssuerCertFile);
		CPPUNIT_ASSERT(pIssuerCert);
	
		{
			EVP_PKEY* pIssuerPublicKey = X509_get_pubkey(pIssuerCert);
			CPPUNIT_ASSERT(pIssuerPublicKey);
			
			EVP_PKEY_copy_parameters(pIssuerPublicKey, pSigKey);
			EVP_PKEY_free(pIssuerPublicKey);
		}
	
		// allow badly signed certificates for testing
		// CPPUNIT_ASSERT(X509_check_private_key(pIssuerCert, pSigKey));
	}
	
	X509_STORE * pStore = X509_STORE_new();
	CPPUNIT_ASSERT(pStore);
	CPPUNIT_ASSERT(X509_STORE_set_default_paths(pStore));
	
	X509* pNewCert = X509_new();
	CPPUNIT_ASSERT_MESSAGE("Error creating certificate object",
		pNewCert);
		
	X509_STORE_CTX pContext;
	CPPUNIT_ASSERT(X509_STORE_CTX_init(&pContext, pStore, pNewCert, NULL));
	X509_STORE_CTX_set_cert(&pContext, pNewCert);
	
	ASN1_INTEGER* pSerialNo = ASN1_INTEGER_new();
	CPPUNIT_ASSERT_MESSAGE("Failed to set serial number", pSerialNo);
	
	BIGNUM *btmp = BN_new();
	CPPUNIT_ASSERT(btmp);
	
	CPPUNIT_ASSERT(BN_pseudo_rand(btmp, 64, 0, 0));
	CPPUNIT_ASSERT(BN_to_ASN1_INTEGER(btmp, pSerialNo));
	BN_free(btmp);
	
	CPPUNIT_ASSERT_MESSAGE("Failed to set certificate serial number",
		X509_set_serialNumber(pNewCert, pSerialNo));
	ASN1_INTEGER_free(pSerialNo);

	if (pIssuerCert)
	{		
		CPPUNIT_ASSERT_MESSAGE("Failed to set certificate issuer",
			X509_set_issuer_name(pNewCert, 
				X509_get_subject_name(pIssuerCert)));
	}
	else
	{
		// self signed
		
		CPPUNIT_ASSERT_MESSAGE("Failed to set certificate issuer",
			X509_set_issuer_name(pNewCert, 
				pRequest->req_info->subject));
	}
		
	CPPUNIT_ASSERT_MESSAGE("Failed to set certificate subject",
		X509_set_subject_name(pNewCert, pRequest->req_info->subject));
	
	X509_set_pubkey(pNewCert, pRequestKey);

	/*
	EVP_PKEY_copy_parameters(pIssuerPublicKey, pSigKey);
	EVP_PKEY_save_parameters(pKey, 1);
	EVP_PKEY_free(pKey);
	pKey = NULL;
	*/

	CPPUNIT_ASSERT_MESSAGE("Failed to set certificate signing date",
		X509_gmtime_adj(X509_get_notBefore(pNewCert), 0));
	CPPUNIT_ASSERT_MESSAGE("Failed to set certificate expiry date",
		X509_gmtime_adj(X509_get_notAfter(pNewCert), (long)60*60*24*1));

	/*
	if (extconf)
	{
		X509V3_CTX ctx;
		X509_set_version(pCert, 2); // version 3 certificate
		X509V3_set_ctx(&ctx, pCert, pCert, NULL, NULL, 0);
		X509V3_set_nconf(&ctx, extconf);
		CPPUNIT_ASSERT(X509V3_EXT_add_nconf(extconf, &ctx, 
			extsect, pCert));
	}
	*/
	
	CPPUNIT_ASSERT_MESSAGE("Failed to sign certificate",
		X509_sign(pNewCert, pSigKey, EVP_sha1()));
	
	BIO* pOut = BIO_new(BIO_s_file());
	CPPUNIT_ASSERT_MESSAGE("Failed to create output BIO", pOut);

	buf = rCertFile.GetFullPath().mb_str(wxConvBoxi);
	filename = strdup(buf.data());
	CPPUNIT_ASSERT_MESSAGE("Failed to set output filename", 
		BIO_write_filename(pOut, filename));
	free(filename);
	
	CPPUNIT_ASSERT_MESSAGE("Failed to write certificate", 
		PEM_write_bio_X509(pOut, pNewCert));
	
	BIO_free_all(pOut);
	X509_REQ_free(pRequest);
	X509_free(pNewCert);
	if (pIssuerCert) X509_free(pIssuerCert);
	X509_STORE_CTX_cleanup(&pContext);	
	X509_STORE_free(pStore);
	// NCONF_free(extconf);
	EVP_PKEY_free(pSigKey);
	EVP_PKEY_free(pRequestKey);

	CPPUNIT_ASSERT(rCertFile.FileExists());
}

#define CheckForwardError(messageId) \
	this->TestWizard::CheckForwardErrorImpl \
	(messageId, _(#messageId), CPPUNIT_SOURCELINE())

void TestWizard::CheckForwardErrorImpl
(
	message_t messageId, 
	const wxString& rMessageName, 
	const CppUnit::SourceLine& rLine
)
{
	CPPUNIT_ASSERT_AT(mpWizard, rLine);

	SetupWizardPage_id_t oldPageId = mpWizard->GetCurrentPageId();
	MessageBoxSetResponse(messageId, wxID_OK);

	ClickForward();
	
	wxString msg;
	msg.Printf(_("Expected error message was not shown: %s"),
		rMessageName.c_str());
	
	wxCharBuffer buf = msg.mb_str(wxConvBoxi);
	CPPUNIT_ASSERT_MESSAGE_AT(buf.data(), wxGetApp().ShowedMessageBox(), 
		rLine);
	
	CPPUNIT_ASSERT_EQUAL_AT(oldPageId, mpWizard->GetCurrentPageId(), rLine);
}

void TestWizard::RunTest()
{
	mpMainFrame = GetMainFrame();
	CPPUNIT_ASSERT(mpMainFrame);
	
	CPPUNIT_ASSERT(!FindWindow(ID_Setup_Wizard_Frame));
	ClickButtonWaitIdle(mpMainFrame->FindWindow(
		ID_General_Setup_Wizard_Button));
	
	mpWizard = (SetupWizard*)FindWindow(ID_Setup_Wizard_Frame);
	CPPUNIT_ASSERT(mpWizard);
	CPPUNIT_ASSERT_EQUAL(BWP_INTRO, mpWizard->GetCurrentPageId());
	
	mpForwardButton = (wxButton*) mpWizard->FindWindow(wxID_FORWARD);
	wxWindow* pBackButton = mpWizard->FindWindow(wxID_BACKWARD);

	CPPUNIT_ASSERT(mpForwardButton);
	CPPUNIT_ASSERT(pBackButton);
	
	ClickForward();
	CPPUNIT_ASSERT_EQUAL(BWP_ACCOUNT, mpWizard->GetCurrentPageId());

	TestAccountPage();
	TestPrivateKeyPage();
	TestCertExistsPage();
	TestCertRequestPage();
	SignCertificate();
	TestCertificatePage();
	TestCryptoKeyPage();
	TestBackedUpPage();
	TestDataDirPage();
	TestConfigOptionalRequiredItems();
	
	CloseWindowWaitClosed(mpMainFrame);

	CPPUNIT_ASSERT(wxRmdir(mTempDir.GetFullPath()));
}

void TestWizard::TestAccountPage()
{
	CPPUNIT_ASSERT_EQUAL(BWP_ACCOUNT, mpWizard->GetCurrentPageId());
	
	wxTextCtrl* pStoreHost;
	wxTextCtrl* pAccountNo;
	{
		wxWizardPage* pPage = mpWizard->GetCurrentPage();
		pStoreHost = GetTextCtrl(pPage, 
			ID_Setup_Wizard_Store_Hostname_Ctrl);
		pAccountNo = GetTextCtrl(pPage,
			ID_Setup_Wizard_Account_Number_Ctrl);
	}
	CPPUNIT_ASSERT(pStoreHost->GetValue().IsSameAs(wxEmptyString));
	CPPUNIT_ASSERT(pAccountNo->GetValue().IsSameAs(wxEmptyString));
	
	CheckForwardError(BM_SETUP_WIZARD_BAD_STORE_HOST);

	mpConfig = mpMainFrame->GetConfig();
	CPPUNIT_ASSERT(mpConfig);
	CPPUNIT_ASSERT(!mpConfig->StoreHostname.IsConfigured());
	
	SetValueDefocusCheck(pStoreHost, _("no-such-host"));
	// calling SetValue should configure the property by itself
	CPPUNIT_ASSERT(mpConfig->StoreHostname.IsConfigured());
	CheckForwardError(BM_SETUP_WIZARD_BAD_STORE_HOST);
	CPPUNIT_ASSERT(mpConfig->StoreHostname.IsConfigured());
	CPPUNIT_ASSERT_EQUAL(wxString(_("no-such-host")), 
		pStoreHost->GetValue());
	
	SetValueDefocusCheck(pStoreHost, _("localhost"));
	CheckForwardError(BM_SETUP_WIZARD_BAD_ACCOUNT_NO);
	CPPUNIT_ASSERT_EQUAL(wxString(_("localhost")), 
		pStoreHost->GetValue());
	
	SetValueDefocusCheck(pAccountNo, _("localhost")); // invalid number
	CheckForwardError(BM_SETUP_WIZARD_BAD_ACCOUNT_NO);
	CPPUNIT_ASSERT(!mpConfig->AccountNumber.IsConfigured());
	CPPUNIT_ASSERT_EQUAL(wxString(_("localhost")), 
		pAccountNo->GetValue());
	
	SetValueAndDefocus(pAccountNo, _("-1"));
	CheckForwardError(BM_SETUP_WIZARD_BAD_ACCOUNT_NO);
	CPPUNIT_ASSERT(!mpConfig->AccountNumber.IsConfigured());

	SetValueDefocusCheck(pAccountNo, _("ffffffff"));
	ClickForward();
	CPPUNIT_ASSERT_EQUAL(BWP_PRIVATE_KEY, mpWizard->GetCurrentPageId());
	unsigned int AccountNumber;
	CPPUNIT_ASSERT(mpConfig->AccountNumber.GetInto((int&)AccountNumber));
	CPPUNIT_ASSERT_EQUAL(0xffffffff, AccountNumber);
	ClickBackward();
	CPPUNIT_ASSERT_EQUAL(BWP_ACCOUNT, mpWizard->GetCurrentPageId());

	SetValueDefocusCheck(pAccountNo, _("12ag"));
	CheckForwardError(BM_SETUP_WIZARD_BAD_ACCOUNT_NO);
	CPPUNIT_ASSERT(!mpConfig->AccountNumber.IsConfigured());
	CPPUNIT_ASSERT_EQUAL(wxString(_("12ag")), pAccountNo->GetValue());

	SetValueDefocusCheck(pAccountNo, _("12345678"));
	ClickForward();
	CPPUNIT_ASSERT_EQUAL(BWP_PRIVATE_KEY, mpWizard->GetCurrentPageId());
	CPPUNIT_ASSERT(mpConfig->AccountNumber.GetInto((int&)AccountNumber));
	CPPUNIT_ASSERT_EQUAL(0x12345678, AccountNumber);
	CPPUNIT_ASSERT_EQUAL(wxString(_("12345678")), 
		pAccountNo->GetValue());

	ClickBackward();
	CPPUNIT_ASSERT_EQUAL(BWP_ACCOUNT, mpWizard->GetCurrentPageId());
	
	SetValueDefocusCheck(pAccountNo, _("12ab"));
	ClickForward();
	CPPUNIT_ASSERT_EQUAL(BWP_PRIVATE_KEY, mpWizard->GetCurrentPageId());
	
	wxString StoredHostname;
	CPPUNIT_ASSERT(mpConfig->StoreHostname.GetInto(StoredHostname));
	CPPUNIT_ASSERT(StoredHostname.IsSameAs(_("localhost")));
	CPPUNIT_ASSERT_EQUAL(wxString(_("localhost")), 
		pStoreHost->GetValue());
	
	CPPUNIT_ASSERT(mpConfig->AccountNumber.GetInto((int&)AccountNumber));
	CPPUNIT_ASSERT_EQUAL(0x12ab, AccountNumber);
	CPPUNIT_ASSERT_EQUAL(wxString(_("12ab")), pAccountNo->GetValue());
}

void TestWizard::TestPrivateKeyPage()
{
	CPPUNIT_ASSERT_EQUAL(BWP_PRIVATE_KEY, mpWizard->GetCurrentPageId());

	mConfigTestDir.AssignTempFileName(_("boxi-configTestDir-"));
	CPPUNIT_ASSERT(wxRemoveFile(mConfigTestDir.GetFullPath()));
	mConfigTestDir = wxFileName(mConfigTestDir.GetFullPath(), wxT(""));
	CPPUNIT_ASSERT(mConfigTestDir.Mkdir(wxS_IRUSR | wxS_IWUSR | wxS_IXUSR));
	
	mTempDir.AssignTempFileName(_("boxi-tempdir-"));
	CPPUNIT_ASSERT(wxRemoveFile(mTempDir.GetFullPath()));
	
	mPrivateKeyFileName = wxFileName(mConfigTestDir.GetFullPath(), 
		_("12ab-key.pem"));

	wxRadioButton* pNewFileRadioButton = 
		(wxRadioButton*)mpWizard->GetCurrentPage()->FindWindow(
			ID_Setup_Wizard_New_File_Radio);
	ClickRadioWaitEvent(pNewFileRadioButton);
	CPPUNIT_ASSERT(pNewFileRadioButton->GetValue());
	
	CheckForwardError(BM_SETUP_WIZARD_NO_FILE_NAME);
	
	wxTextCtrl* pFileName = GetTextCtrl(
		mpWizard->GetCurrentPage(),
		ID_Setup_Wizard_File_Name_Text);
	
	CPPUNIT_ASSERT(pFileName->GetValue().IsSameAs(wxEmptyString));

	wxGetApp().ExpectFileDialog(_("Private Key (*-key.pem)|*-key.pem"),
		wxT(""));
	ClickButtonWaitEvent(ID_Setup_Wizard_File_Selector_Button);
	BOXI_ASSERT(wxGetApp().ShowedFileDialog());

	ClickRadioWaitEvent(ID_Setup_Wizard_Existing_File_Radio);
	wxGetApp().ExpectFileDialog(_("Private Key (*-key.pem)|*-key.pem|"
		"All Files|*"), wxT(""));
	ClickButtonWaitEvent(ID_Setup_Wizard_File_Selector_Button);
	BOXI_ASSERT(wxGetApp().ShowedFileDialog());

	ClickRadioWaitEvent(pNewFileRadioButton);
	wxFileName nonexistantfile(mTempDir.GetFullPath(), 
		_("nonexistant"));
	SetValueDefocusCheck(pFileName, nonexistantfile.GetFullPath());

	// filename refers to a path that doesn't exist
	// expect BM_SETUP_WIZARD_FILE_DIR_NOT_FOUND
	CheckForwardError(BM_SETUP_WIZARD_FILE_DIR_NOT_FOUND);

	// create the directory, but not the file
	// make the directory not writable
	// expect BM_SETUP_WIZARD_FILE_DIR_NOT_WRITABLE
	CPPUNIT_ASSERT(wxFileName::Mkdir(mTempDir.GetFullPath(),
		wxS_IRUSR | wxS_IXUSR));
	CPPUNIT_ASSERT(wxFileName::DirExists(mTempDir.GetFullPath()));

	#ifndef WIN32 // it is writable on Windows
	CheckForwardError(BM_SETUP_WIZARD_FILE_DIR_NOT_WRITABLE);
	#endif
	
	// change the filename to refer to the directory
	// expect BM_SETUP_WIZARD_FILE_IS_A_DIRECTORY
	SetValueDefocusCheck(pFileName, mTempDir.GetFullPath());
	CPPUNIT_ASSERT(pFileName->GetValue().IsSameAs(mTempDir.GetFullPath()));
	CheckForwardError(BM_SETUP_WIZARD_FILE_IS_A_DIRECTORY);
	
	// make the directory read write
	CPPUNIT_ASSERT(wxFileName::Rmdir(mTempDir.GetFullPath()));
	CPPUNIT_ASSERT(wxFileName::Mkdir(mTempDir.GetFullPath(),
		wxS_IRUSR | wxS_IWUSR | wxS_IXUSR));
	
	// create a new file in the directory, make it read only, 
	// and change the filename to refer to it. 
	// expect BM_SETUP_WIZARD_FILE_OVERWRITE
	wxFileName existingfile(mTempDir.GetFullPath(), _("existing"));
	wxString   existingpath = existingfile.GetFullPath();
	wxFile     existing;

	CPPUNIT_ASSERT(existing.Create(existingpath, false, wxS_IRUSR));
	CPPUNIT_ASSERT(  wxFile::Access(existingpath, wxFile::read));
	CPPUNIT_ASSERT(! wxFile::Access(existingpath, wxFile::write));
	
	SetValueDefocusCheck(pFileName, existingpath);
	CheckForwardError(BM_SETUP_WIZARD_FILE_NOT_WRITABLE);

	// make the file writable, expect BM_SETUP_WIZARD_FILE_OVERWRITE
	// reply No and check that we stay put
	
	wxCharBuffer buf = existingpath.mb_str();
	CPPUNIT_ASSERT_MESSAGE(buf.data(), 
		chmod(buf.data(), 0777) == 0);
	CPPUNIT_ASSERT_MESSAGE(buf.data(), wxRemoveFile(existingpath));
	CPPUNIT_ASSERT(existing.Create(existingpath, false, 
		wxS_IRUSR | wxS_IWUSR));
	CPPUNIT_ASSERT(wxFile::Access(existingpath, wxFile::read));
	CPPUNIT_ASSERT(wxFile::Access(existingpath, wxFile::write));
	
	MessageBoxSetResponse(BM_SETUP_WIZARD_FILE_OVERWRITE, wxNO);
	ClickForward();
	MessageBoxCheckFired();
	
	CPPUNIT_ASSERT_EQUAL(BWP_PRIVATE_KEY, mpWizard->GetCurrentPageId());
	
	// now try again, reply Yes, and check that we move forward
	MessageBoxSetResponse(BM_SETUP_WIZARD_FILE_OVERWRITE, wxYES);
	ClickForward();
	MessageBoxCheckFired();

	CPPUNIT_ASSERT_EQUAL(BWP_CERT_EXISTS, mpWizard->GetCurrentPageId());

	// was the configuration updated?
	wxString keyfile;
	CPPUNIT_ASSERT(mpConfig->PrivateKeyFile.GetInto(keyfile));
	CPPUNIT_ASSERT(keyfile.IsSameAs(existingpath));

	// go back, select "existing", choose a nonexistant file,
	// expect BM_SETUP_WIZARD_FILE_NOT_FOUND
	ClickBackward();
	CPPUNIT_ASSERT_EQUAL(BWP_PRIVATE_KEY, mpWizard->GetCurrentPageId());
	
	wxRadioButton* pExistingFileRadioButton = 
		(wxRadioButton*)mpWizard->GetCurrentPage()->FindWindow(
			ID_Setup_Wizard_Existing_File_Radio);
	CPPUNIT_ASSERT(pExistingFileRadioButton);
	ClickRadioWaitEvent(pExistingFileRadioButton);
	CPPUNIT_ASSERT(pExistingFileRadioButton->GetValue());
	
	CPPUNIT_ASSERT(pFileName->GetValue().IsSameAs(existingpath));
	
	SetValueDefocusCheck(pFileName, nonexistantfile.GetFullPath());
	CheckForwardError(BM_SETUP_WIZARD_FILE_NOT_FOUND);
	
	// set the path to a bogus path, 
	// expect BM_SETUP_WIZARD_FILE_NOT_FOUND
	wxString boguspath = nonexistantfile.GetFullPath();
	boguspath.Append(_("/foo/bar"));
	SetValueDefocusCheck(pFileName, boguspath);
	CheckForwardError(BM_SETUP_WIZARD_FILE_NOT_FOUND);
	
	// create another file, make it unreadable,
	// expect BM_SETUP_WIZARD_FILE_NOT_READABLE
	wxString anotherfilename = existingpath;
	anotherfilename.Append(_("2"));
	wxFile anotherfile;
	CPPUNIT_ASSERT(anotherfile.Create(anotherfilename, false, 0));
	
	SetValueDefocusCheck(pFileName, anotherfilename);
	CheckForwardError(BM_SETUP_WIZARD_FILE_NOT_READABLE);
	
	// make it readable, but not a valid key,
	// expect BM_SETUP_WIZARD_BAD_PRIVATE_KEY
	CPPUNIT_ASSERT(wxRemoveFile(anotherfilename));
	CPPUNIT_ASSERT(anotherfile.Create(anotherfilename, wxS_IRUSR));
	CheckForwardError(BM_SETUP_WIZARD_BAD_PRIVATE_KEY);
	
	// delete that file, move the private key file to the 
	// mConfigTestDir, set the filename to that new location,
	// expect that Boxi can read the key file and we move on
	// to the next page
	CPPUNIT_ASSERT(::wxRemoveFile(anotherfilename));
	CPPUNIT_ASSERT(::wxRenameFile(existingpath, 
		mPrivateKeyFileName.GetFullPath()));
	SetValueDefocusCheck(pFileName, mPrivateKeyFileName.GetFullPath());
	ClickForward();

	CPPUNIT_ASSERT_EQUAL(BWP_CERT_EXISTS, mpWizard->GetCurrentPageId());
	
	// was the configuration updated?
	CPPUNIT_ASSERT(mpConfig->PrivateKeyFile.GetInto(keyfile));
	CPPUNIT_ASSERT(keyfile.IsSameAs(mPrivateKeyFileName.GetFullPath()));
}

void TestWizard::TestCertExistsPage()
{
	CPPUNIT_ASSERT_EQUAL(BWP_CERT_EXISTS, mpWizard->GetCurrentPageId());

	// ask for a new certificate request to be generated
	wxRadioButton* pNewFileRadioButton =
		(wxRadioButton*)mpWizard->GetCurrentPage()->FindWindow(
			ID_Setup_Wizard_New_File_Radio);
	ClickRadioWaitEvent(pNewFileRadioButton);
	CPPUNIT_ASSERT(pNewFileRadioButton->GetValue());
	
	ClickForward();
	
	CPPUNIT_ASSERT_EQUAL(BWP_CERT_REQUEST, mpWizard->GetCurrentPageId());
}

void TestWizard::TestCertRequestPage()
{
	CPPUNIT_ASSERT_EQUAL(BWP_CERT_REQUEST, mpWizard->GetCurrentPageId());

	mClientCsrFileName = wxFileName(mConfigTestDir.GetFullPath(), 
		_("12ab-csr.pem"));

	// ask for a new certificate request to be generated
	wxRadioButton* pNewFileRadioButton = 
		(wxRadioButton*)mpWizard->GetCurrentPage()->FindWindow(
			ID_Setup_Wizard_New_File_Radio);
	ClickRadioWaitEvent(pNewFileRadioButton);
	CPPUNIT_ASSERT(pNewFileRadioButton->GetValue());
	
	wxString tmp;
	
	CPPUNIT_ASSERT(mpConfig->CertRequestFile.IsConfigured());
	CPPUNIT_ASSERT(mpConfig->CertRequestFile.GetInto(tmp));
	CPPUNIT_ASSERT(tmp.IsSameAs(mClientCsrFileName.GetFullPath()));
	
	// go back, and check that the property is unconfigured for us
	CPPUNIT_ASSERT_EQUAL(BWP_CERT_REQUEST, mpWizard->GetCurrentPageId());
	ClickBackward();
	CPPUNIT_ASSERT_EQUAL(BWP_CERT_EXISTS, mpWizard->GetCurrentPageId());
	CPPUNIT_ASSERT(!mpConfig->CertRequestFile.IsConfigured());
	
	// set the certificate file manually, 
	// check that it is not overwritten.
	wxFileName dummyName(mTempDir.GetFullPath(), _("nosuchfile"));
	mpConfig->CertRequestFile.Set(dummyName.GetFullPath());
	
	CPPUNIT_ASSERT(mpConfig->CertRequestFile.IsConfigured());
	CPPUNIT_ASSERT(mpConfig->CertRequestFile.GetInto(tmp));
	CPPUNIT_ASSERT(tmp.IsSameAs(dummyName.GetFullPath()));
	
	CPPUNIT_ASSERT_EQUAL(BWP_CERT_EXISTS, mpWizard->GetCurrentPageId());
	ClickForward();
	CPPUNIT_ASSERT_EQUAL(BWP_CERT_REQUEST, mpWizard->GetCurrentPageId());
	CPPUNIT_ASSERT(mpConfig->CertRequestFile.GetInto(tmp));
	CPPUNIT_ASSERT(tmp.IsSameAs(dummyName.GetFullPath()));

	// clear the value again, check that the property is 
	// unconfigured
	wxTextCtrl* pText = GetTextCtrl(mpWizard->GetCurrentPage(), 
		ID_Setup_Wizard_File_Name_Text);
	CPPUNIT_ASSERT(pText);
	SetValueDefocusCheck(pText, wxEmptyString);
	CPPUNIT_ASSERT(!mpConfig->CertRequestFile.IsConfigured());
	
	// go back, forward again, check that the default value is inserted
	CPPUNIT_ASSERT_EQUAL(BWP_CERT_REQUEST, mpWizard->GetCurrentPageId());

	// assign a non-standard private key file ending in .pem, check that
	// the CSR filename is generated correctly.
	wxString oldPrivateKeyFile = mpConfig->PrivateKeyFile.GetWxString();
	mpConfig->PrivateKeyFile.Set(_("/foobar/29a.pem"));
	ClickBackward();
	CPPUNIT_ASSERT_EQUAL(BWP_CERT_EXISTS, mpWizard->GetCurrentPageId());
	ClickForward();
	CPPUNIT_ASSERT_EQUAL(BWP_CERT_REQUEST, mpWizard->GetCurrentPageId());
	CPPUNIT_ASSERT(mpConfig->CertRequestFile.IsConfigured());
	CPPUNIT_ASSERT(mpConfig->CertRequestFile.GetInto(tmp));
	BOXI_ASSERT_EQUAL(wxString(_("/foobar/29a-csr.pem")), tmp);

	// assign a custom value, check that going back and forward does not
	// clear it
	SetValueDefocusCheck(pText, _("/whee/xxx"));
	ClickBackward();
	CPPUNIT_ASSERT_EQUAL(BWP_CERT_EXISTS, mpWizard->GetCurrentPageId());
	ClickForward();
	CPPUNIT_ASSERT_EQUAL(BWP_CERT_REQUEST, mpWizard->GetCurrentPageId());
	CPPUNIT_ASSERT(mpConfig->CertRequestFile.IsConfigured());
	BOXI_ASSERT_EQUAL(wxString(_("/whee/xxx")),
		mpConfig->CertRequestFile.GetWxString());
	BOXI_ASSERT_EQUAL(wxString(_("/whee/xxx")), pText->GetValue());

	// reset private key file, clear cert request file, check that
	// going back and forward resets the cert request file path to
	// the default value
	mpConfig->PrivateKeyFile.Set(oldPrivateKeyFile);
	mpConfig->CertRequestFile.Clear();
	ClickBackward();
	BOXI_ASSERT_EQUAL(BWP_CERT_EXISTS, mpWizard->GetCurrentPageId());
	ClickForward();
	BOXI_ASSERT_EQUAL(BWP_CERT_REQUEST, mpWizard->GetCurrentPageId());
	CPPUNIT_ASSERT(mpConfig->CertRequestFile.IsConfigured());
	CPPUNIT_ASSERT(mpConfig->CertRequestFile.GetInto(tmp));
	CPPUNIT_ASSERT(tmp.IsSameAs(mClientCsrFileName.GetFullPath()));
	
	// go forward, check that the file is created
	CPPUNIT_ASSERT(!mClientCsrFileName.FileExists());
	CPPUNIT_ASSERT_EQUAL(BWP_CERT_REQUEST, mpWizard->GetCurrentPageId());

	ClickForward();
	CPPUNIT_ASSERT_EQUAL(BWP_CERTIFICATE, mpWizard->GetCurrentPageId());
	CPPUNIT_ASSERT(mClientCsrFileName.FileExists());
	
	// go back, choose "existing cert request", check that
	// our newly generated request is accepted.
	CPPUNIT_ASSERT_EQUAL(BWP_CERTIFICATE, mpWizard->GetCurrentPageId());
	
	ClickBackward();
	CPPUNIT_ASSERT_EQUAL(BWP_CERT_EXISTS, mpWizard->GetCurrentPageId());
	
	ClickForward();
	CPPUNIT_ASSERT_EQUAL(BWP_CERT_REQUEST, mpWizard->GetCurrentPageId());
	
	wxRadioButton* pExistingFileRadioButton = 
		(wxRadioButton*)mpWizard->GetCurrentPage()->FindWindow(
			ID_Setup_Wizard_Existing_File_Radio);
	CPPUNIT_ASSERT(pExistingFileRadioButton);
	ClickRadioWaitEvent(pExistingFileRadioButton);
	CPPUNIT_ASSERT(pExistingFileRadioButton->GetValue());
		
	CPPUNIT_ASSERT(mpConfig->CertRequestFile.IsConfigured());
	CPPUNIT_ASSERT(mpConfig->CertRequestFile.GetInto(tmp));
	CPPUNIT_ASSERT(tmp.IsSameAs(mClientCsrFileName.GetFullPath()));
	
	ClickForward();
	CPPUNIT_ASSERT_EQUAL(BWP_CERTIFICATE, mpWizard->GetCurrentPageId());
}

void TestWizard::SignCertificate()
{
	// fake a server signing of this private keyfile

	mClientCertFileName = wxFileName(mConfigTestDir.GetFullPath(), 
		_("12ab-cert.pem"));
	CPPUNIT_ASSERT(!mClientCertFileName.FileExists());
	
	mCaFileName = wxFileName(mConfigTestDir.GetFullPath(), 
		_("client-ca-cert.pem"));
	CPPUNIT_ASSERT(!mCaFileName.FileExists());
	
	mClientBadSigCertFileName = wxFileName(mConfigTestDir.GetFullPath(), 
		_("12ab-cert-badsig.pem"));
	CPPUNIT_ASSERT(!mClientBadSigCertFileName.FileExists());
	
	mClientSelfSigCertFileName = wxFileName(mConfigTestDir.GetFullPath(), 
		_("12ab-cert-selfsig.pem"));
	CPPUNIT_ASSERT(!mClientSelfSigCertFileName.FileExists());
	
	// generate a private key for the fake client CA
	
	wxFileName clientCaKeyFileName(mConfigTestDir.GetFullPath(), 
		_("client-ca-key.pem"));
	CPPUNIT_ASSERT(!clientCaKeyFileName.FileExists());
	
	BIGNUM* pBigNum = BN_new();
	CPPUNIT_ASSERT_MESSAGE("Failed to create an OpenSSL BN object",
		pBigNum);
	
	RSA* pRSA = RSA_new();
	CPPUNIT_ASSERT_MESSAGE("Failed to create an OpenSSL RSA object",
		pRSA);

	BIO* pKeyOutBio = BIO_new(BIO_s_file());
	CPPUNIT_ASSERT_MESSAGE("Failed to create an OpenSSL BIO object",
		pKeyOutBio);
	
	wxCharBuffer filename = 
		clientCaKeyFileName.GetFullPath().mb_str(wxConvBoxi);
	CPPUNIT_ASSERT_MESSAGE("Failed to open key file using an "
		"OpenSSL BIO",
		BIO_write_filename(pKeyOutBio, filename.data()) >= 0);
	
	CPPUNIT_ASSERT_MESSAGE("Failed to set key type to RSA F4",
		BN_set_word(pBigNum, RSA_F4));

	wxProgressDialog progress(_("Boxi: Generating Server Key"),
		MySslProgressCallbackMessage(0), 2, mpMainFrame, 
		wxPD_APP_MODAL | wxPD_ELAPSED_TIME);
	numCheckedTest = 0;
	
	RSA* pRSA2 = RSA_generate_key(2048, 0x10001, 
		MySslProgressCallbackTest, &progress);
	CPPUNIT_ASSERT_MESSAGE("Failed to generate an RSA key", pRSA2);

	progress.Hide();
	
	CPPUNIT_ASSERT_MESSAGE("Failed to write the key "
		"to the specified file",
		PEM_write_bio_RSAPrivateKey(pKeyOutBio, pRSA2, NULL, 
			NULL, 0, NULL, NULL));
	
	BIO_free_all(pKeyOutBio);
	RSA_free(pRSA);
	BN_free(pBigNum);

	CPPUNIT_ASSERT(clientCaKeyFileName.FileExists());
	
	// generate a certificate signing request (CSR) for 
	// the fake client CA, which we will sign ourselves

	wxFileName clientCaCsrFileName(mConfigTestDir.GetFullPath(), 
		_("client-ca-csr.pem"));
	CPPUNIT_ASSERT(!clientCaCsrFileName.FileExists());

	wxString msg;
	std::auto_ptr<SslConfig> apConfig(SslConfig::Get(&msg));
	CPPUNIT_ASSERT_MESSAGE("Error loading OpenSSL config", 
		apConfig.get());

	char* pExtensions = NCONF_get_string(apConfig->GetConf(), "req", 
		"x509_extensions");
	if (pExtensions) 
	{
		/* Check syntax of file */
		X509V3_CTX ctx;
		X509V3_set_ctx_test(&ctx);
		X509V3_set_nconf(&ctx, apConfig->GetConf());
		
		CPPUNIT_ASSERT_MESSAGE("Error in OpenSSL configuration: "
			"Failed to load certificate extensions section",
			X509V3_EXT_add_nconf(apConfig->GetConf(), &ctx, 
				pExtensions, NULL));
	}
	else
	{
		ERR_clear_error();
	}
	
	CPPUNIT_ASSERT_MESSAGE("Failed to set the OpenSSL string mask to "
		"'nombstr'", 
		ASN1_STRING_set_default_mask_asc("nombstr"));
	
	EVP_PKEY* pKey = LoadKey(clientCaKeyFileName.GetFullPath(), &msg);
	CPPUNIT_ASSERT_MESSAGE("Failed to load private key", pKey);

	BIO* pRequestBio = BIO_new(BIO_s_file());
	CPPUNIT_ASSERT_MESSAGE("Failed to create an OpenSSL BIO", pRequestBio);
	
	X509_REQ* pRequest = X509_REQ_new();
	CPPUNIT_ASSERT_MESSAGE("Failed to create a new OpenSSL "
		"request object", pRequest);
	
	CPPUNIT_ASSERT_MESSAGE("Failed to set version in "
		"OpenSSL request object",
		X509_REQ_set_version(pRequest,0L));
	
	int commonNameNid = OBJ_txt2nid("commonName");
	CPPUNIT_ASSERT_MESSAGE("Failed to find node ID of "
		"X.509 CommonName attribute", commonNameNid != NID_undef);
	
	wxString certSubject = _("Backup system client root");
	X509_NAME* pX509SubjectName = X509_REQ_get_subject_name(pRequest);
	CPPUNIT_ASSERT_MESSAGE("X.509 subject name was null", 
		pX509SubjectName);

	wxCharBuffer subject = certSubject.mb_str(wxConvBoxi);
	CPPUNIT_ASSERT_MESSAGE("Failed to add common name to "
		"certificate request",
		X509_NAME_add_entry_by_NID(pX509SubjectName, 
			commonNameNid, MBSTRING_ASC, 
			(unsigned char *)subject.data(), 
			-1, -1, 0));

	CPPUNIT_ASSERT_MESSAGE("Failed to set public key of "
		"certificate request", X509_REQ_set_pubkey(pRequest, pKey));
	
	X509V3_CTX ext_ctx;
	X509V3_set_ctx(&ext_ctx, NULL, NULL, pRequest, NULL, 0);
	X509V3_set_nconf(&ext_ctx, apConfig->GetConf());
	
	char *pReqExtensions = NCONF_get_string(apConfig->GetConf(), 
		"section", "req_extensions");
	if (pReqExtensions)
	{
		X509V3_CTX ctx;
		X509V3_set_ctx_test(&ctx);
		X509V3_set_nconf(&ctx, apConfig->GetConf());
		
		CPPUNIT_ASSERT_MESSAGE("Failed to load request extensions "
			"section", 
			X509V3_EXT_REQ_add_nconf(apConfig->GetConf(), 
				&ctx, pReqExtensions, pRequest));
	}
	else
	{
		ERR_clear_error();
	}

	const EVP_MD* pDigestAlgo = EVP_get_digestbyname("sha1");
	CPPUNIT_ASSERT_MESSAGE("Failed to find OpenSSL digest "
		"algorithm SHA1", pDigestAlgo);
	
	CPPUNIT_ASSERT_MESSAGE("Failed to sign certificate request",
		X509_REQ_sign(pRequest, pKey, pDigestAlgo));
	
	CPPUNIT_ASSERT_MESSAGE("Failed to verify signature on "
		"certificate request", X509_REQ_verify(pRequest, pKey));
	
	wxCharBuffer buf = 
		clientCaCsrFileName.GetFullPath().mb_str(wxConvBoxi);
	CPPUNIT_ASSERT_MESSAGE("Failed to set certificate request "
		"output filename", 
		BIO_write_filename(pRequestBio, buf.data()) > 0);
	
	CPPUNIT_ASSERT_MESSAGE("Failed to write certificate request "
		"file", PEM_write_bio_X509_REQ(pRequestBio, pRequest));
	
	X509_REQ_free(pRequest);
	pRequest = NULL;
	
	FreeKey(pKey);
	pKey = NULL;
	
	BIO_free(pRequestBio);
	pRequestBio = NULL;

	CPPUNIT_ASSERT(clientCaCsrFileName.FileExists());
	
	// sign our own request with our own private key
	// (self signed)

	wxFileName clientCaCertFileName(mConfigTestDir.GetFullPath(), 
		_("client-ca-cert.pem"));
	CPPUNIT_ASSERT(!clientCaCertFileName.FileExists());

	{
		wxFileName dummy;
		SignRequestWithKey
		(
			clientCaCsrFileName, 
			clientCaKeyFileName,
			dummy, 
			clientCaCertFileName
		);
	}

	CPPUNIT_ASSERT(clientCaCertFileName.FileExists());
	
	// sign the client's request with our private key

	CPPUNIT_ASSERT(!mClientCertFileName.FileExists());

	SignRequestWithKey(mClientCsrFileName, clientCaKeyFileName,
		clientCaCertFileName, mClientCertFileName);
	CPPUNIT_ASSERT(mClientCertFileName.FileExists());

	// make a "bad signature" copy of the client's request,
	// signed with its own private key (mismatches the cert)

	CPPUNIT_ASSERT(!mClientBadSigCertFileName.FileExists());

	SignRequestWithKey(mClientCsrFileName, mPrivateKeyFileName,
		clientCaCertFileName, mClientBadSigCertFileName);
	CPPUNIT_ASSERT(mClientBadSigCertFileName.FileExists());		

	// make a "self signed" copy of the client's request,
	// signed with its own certificate and private key

	CPPUNIT_ASSERT(!mClientSelfSigCertFileName.FileExists());

	SignRequestWithKey(mClientCsrFileName, mPrivateKeyFileName,
		wxFileName(), mClientSelfSigCertFileName);
	CPPUNIT_ASSERT(mClientSelfSigCertFileName.FileExists());		
}

void TestWizard::TestCertificatePage()
{
	CPPUNIT_ASSERT_EQUAL(BWP_CERTIFICATE, mpWizard->GetCurrentPageId());

	// clear both fields, try to go forward, check that it fails
	CPPUNIT_ASSERT(!mpConfig->CertificateFile.IsConfigured());
	CPPUNIT_ASSERT(!mpConfig->TrustedCAsFile.IsConfigured());		
	CheckForwardError(BM_SETUP_WIZARD_NO_FILE_NAME);
	
	// set the certificate file to a name that doesn't exist
	wxFileName nonexistantfile(mTempDir.GetFullPath(), 
		_("nonexistant"));
	mpConfig->CertificateFile.Set(nonexistantfile.GetFullPath());
	
	// try to go forward, check that it fails
	CheckForwardError(BM_SETUP_WIZARD_FILE_NOT_FOUND);

	// create another file, make it unreadable,
	// expect BM_SETUP_WIZARD_FILE_NOT_READABLE
	wxString anotherfilename = mClientCertFileName.GetFullPath();
	anotherfilename.Append(_("2"));
	wxFile anotherfile;
	CPPUNIT_ASSERT(anotherfile.Create(anotherfilename, false, 0));
	
	mpConfig->CertificateFile.Set(anotherfilename);
	CheckForwardError(BM_SETUP_WIZARD_FILE_NOT_READABLE);
	
	// make it readable, but not a valid certificate,
	// expect BM_SETUP_WIZARD_CERTIFICATE_FILE_ERROR
	CPPUNIT_ASSERT(wxRemoveFile(anotherfilename));
	CPPUNIT_ASSERT(anotherfile.Create(anotherfilename, wxS_IRUSR));
	CheckForwardError(BM_SETUP_WIZARD_CERTIFICATE_FILE_ERROR);
	CPPUNIT_ASSERT(wxRemoveFile(anotherfilename));

	// set the CertificateFile to the real certificate,
	// expect complaints about the Trusted CAs File
	mpConfig->CertificateFile.Set(mClientCertFileName.GetFullPath());
	CPPUNIT_ASSERT(!mpConfig->TrustedCAsFile.IsConfigured());
	CheckForwardError(BM_SETUP_WIZARD_NO_FILE_NAME);

	// same for the trusted CAs file
	// set the certificate file to a name that doesn't exist
	mpConfig->TrustedCAsFile.Set(nonexistantfile.GetFullPath());
	
	// try to go forward, check that it fails
	CheckForwardError(BM_SETUP_WIZARD_FILE_NOT_FOUND);

	// create another file, make it unreadable,
	// expect BM_SETUP_WIZARD_FILE_NOT_READABLE
	CPPUNIT_ASSERT(anotherfile.Create(anotherfilename, false, 0));		
	mpConfig->TrustedCAsFile.Set(anotherfilename);
	CheckForwardError(BM_SETUP_WIZARD_FILE_NOT_READABLE);
	
	// make it readable, but not a valid certificate,
	// expect BM_SETUP_WIZARD_CERTIFICATE_FILE_ERROR
	CPPUNIT_ASSERT(wxRemoveFile(anotherfilename));
	CPPUNIT_ASSERT(anotherfile.Create(anotherfilename, wxS_IRUSR));
	CheckForwardError(BM_SETUP_WIZARD_CERTIFICATE_FILE_ERROR);
	CPPUNIT_ASSERT(wxRemoveFile(anotherfilename));

	// set it to the real trusted CAs certificate,
	mpConfig->TrustedCAsFile.Set(mCaFileName.GetFullPath());

	// set CertificateFile to a valid certificate file, 
	// but with a bad signature, expect BM_SETUP_WIZARD_CERTIFICATE_FILE_ERROR
	/*
	mpConfig->CertificateFile.Set(mClientBadSigCertFileName.GetFullPath());
	CheckForwardError(BM_SETUP_WIZARD_CERTIFICATE_FILE_ERROR);
	*/
	
	// set CertificateFile to a valid certificate file, 
	// but self signed, expect BM_SETUP_WIZARD_CERTIFICATE_FILE_ERROR
	// (key mismatch between the certs)
	/*
	mpConfig->CertificateFile.Set(mClientSelfSigCertFileName.GetFullPath());
	CheckForwardError(BM_SETUP_WIZARD_CERTIFICATE_FILE_ERROR);
	*/

	// set the CertificateFile to the real one,
	// expect that we can go forward
	mpConfig->CertificateFile.Set(mClientCertFileName.GetFullPath());
	CPPUNIT_ASSERT_EQUAL(BWP_CERTIFICATE, mpWizard->GetCurrentPageId());
	ClickForward();
	CPPUNIT_ASSERT_EQUAL(BWP_CRYPTO_KEY, mpWizard->GetCurrentPageId());
}

void TestWizard::TestCryptoKeyPage()
{
	CPPUNIT_ASSERT_EQUAL(BWP_CRYPTO_KEY, mpWizard->GetCurrentPageId());

	mClientCryptoFileName = wxFileName(mConfigTestDir.GetFullPath(), 
		_("12ab-FileEncKeys.raw"));
	CPPUNIT_ASSERT(!mClientCryptoFileName.FileExists());
	
	// ask for new crypto keys to be generated
	wxRadioButton* pNewFileRadioButton = 
		(wxRadioButton*)mpWizard->GetCurrentPage()->FindWindow(
			ID_Setup_Wizard_New_File_Radio);
	ClickRadioWaitEvent(pNewFileRadioButton);
	CPPUNIT_ASSERT(pNewFileRadioButton->GetValue());
	
	wxString tmp;
	
	CPPUNIT_ASSERT(mpConfig->KeysFile.IsConfigured());
	CPPUNIT_ASSERT(mpConfig->KeysFile.GetInto(tmp));
	CPPUNIT_ASSERT(tmp.IsSameAs(mClientCryptoFileName.GetFullPath()));
	
	// go back, and check that the property is unconfigured for us
	CPPUNIT_ASSERT_EQUAL(BWP_CRYPTO_KEY, mpWizard->GetCurrentPageId());
	ClickBackward();
	CPPUNIT_ASSERT_EQUAL(BWP_CERTIFICATE, mpWizard->GetCurrentPageId());
	CPPUNIT_ASSERT(!mpConfig->KeysFile.IsConfigured());
	
	// set the crypto key file manually, 
	// check that it is not overwritten.
	wxFileName dummyName(mTempDir.GetFullPath(), _("nosuchfile"));
	mpConfig->KeysFile.Set(dummyName.GetFullPath());
	
	CPPUNIT_ASSERT(mpConfig->KeysFile.IsConfigured());
	CPPUNIT_ASSERT(mpConfig->KeysFile.GetInto(tmp));
	CPPUNIT_ASSERT(tmp.IsSameAs(dummyName.GetFullPath()));
	
	CPPUNIT_ASSERT_EQUAL(BWP_CERTIFICATE, mpWizard->GetCurrentPageId());
	ClickForward();
	CPPUNIT_ASSERT_EQUAL(BWP_CRYPTO_KEY, mpWizard->GetCurrentPageId());
	CPPUNIT_ASSERT(mpConfig->KeysFile.GetInto(tmp));
	CPPUNIT_ASSERT(tmp.IsSameAs(dummyName.GetFullPath()));

	// clear the value again, check that the property is 
	// unconfigured
	wxTextCtrl* pText = GetTextCtrl(mpWizard->GetCurrentPage(), 
		ID_Setup_Wizard_File_Name_Text);
	CPPUNIT_ASSERT(pText);
	SetValueDefocusCheck(pText, wxEmptyString);
	CPPUNIT_ASSERT(!mpConfig->KeysFile.IsConfigured());
	
	// go back, forward again, check that the default 
	// value is inserted
	CPPUNIT_ASSERT_EQUAL(BWP_CRYPTO_KEY, mpWizard->GetCurrentPageId());

	ClickBackward();
	CPPUNIT_ASSERT_EQUAL(BWP_CERTIFICATE, mpWizard->GetCurrentPageId());
	
	ClickForward();
	CPPUNIT_ASSERT_EQUAL(BWP_CRYPTO_KEY, mpWizard->GetCurrentPageId());
	
	CPPUNIT_ASSERT(mpConfig->KeysFile.IsConfigured());
	CPPUNIT_ASSERT(mpConfig->KeysFile.GetInto(tmp));
	CPPUNIT_ASSERT(tmp.IsSameAs(mClientCryptoFileName.GetFullPath()));
	
	// go forward, check that the file is created
	CPPUNIT_ASSERT(!mClientCryptoFileName.FileExists());
	CPPUNIT_ASSERT_EQUAL(BWP_CRYPTO_KEY, mpWizard->GetCurrentPageId());

	ClickForward();
	CPPUNIT_ASSERT_EQUAL(BWP_BACKED_UP, mpWizard->GetCurrentPageId());
	CPPUNIT_ASSERT(mClientCryptoFileName.FileExists());

	// go back
	ClickBackward();
	CPPUNIT_ASSERT_EQUAL(BWP_CRYPTO_KEY, mpWizard->GetCurrentPageId());

	// choose "existing keys file"
	wxRadioButton* pExistingFileRadioButton = 
		(wxRadioButton*)mpWizard->GetCurrentPage()->FindWindow(
			ID_Setup_Wizard_Existing_File_Radio);
	CPPUNIT_ASSERT(pExistingFileRadioButton);
	ClickRadioWaitEvent(pExistingFileRadioButton);
	CPPUNIT_ASSERT(pExistingFileRadioButton->GetValue());

	// specify a newly created empty file, 
	// expect BM_SETUP_WIZARD_ENCRYPTION_KEY_FILE_NOT_VALID
	wxString anotherfilename = mClientCryptoFileName.GetFullPath();
	anotherfilename.Append(_(".invalid"));
	mpConfig->KeysFile.Set(anotherfilename);
	wxFile anotherfile;
	CPPUNIT_ASSERT(anotherfile.Create(anotherfilename, wxS_IRUSR));
	CheckForwardError(BM_SETUP_WIZARD_ENCRYPTION_KEY_FILE_NOT_VALID);
	CPPUNIT_ASSERT(wxRemoveFile(anotherfilename));
	
	// check that our newly generated key is accepted.	
	mpConfig->KeysFile.Set(mClientCryptoFileName.GetFullPath());
	
	ClickForward();
	CPPUNIT_ASSERT_EQUAL(BWP_BACKED_UP, mpWizard->GetCurrentPageId());
}

void TestWizard::TestBackedUpPage()
{
	CPPUNIT_ASSERT_EQUAL(BWP_BACKED_UP, mpWizard->GetCurrentPageId());
	
	wxCheckBox* pCheckBox = (wxCheckBox*) FindWindow
		(ID_Setup_Wizard_Backed_Up_Checkbox);
	CPPUNIT_ASSERT(pCheckBox);
	CPPUNIT_ASSERT(!pCheckBox->GetValue());
	
	CheckForwardError(BM_SETUP_WIZARD_MUST_CHECK_THE_BOX_KEYS_BACKED_UP);
	
	pCheckBox->SetValue(true);
	ClickForward();

	CPPUNIT_ASSERT_EQUAL(BWP_DATA_DIR, mpWizard->GetCurrentPageId());
}

void TestWizard::TestDataDirPage()
{
	CPPUNIT_ASSERT_EQUAL(BWP_DATA_DIR, mpWizard->GetCurrentPageId());

	mpConfig->DataDirectory.Clear();
	CheckForwardError(BM_SETUP_WIZARD_NO_FILE_NAME);
	
	CPPUNIT_ASSERT(wxRmdir(mTempDir.GetFullPath()));
	CPPUNIT_ASSERT(!wxFileName::DirExists(mTempDir.GetFullPath()));
	wxFile temp;
	CPPUNIT_ASSERT(temp.Create(mTempDir.GetFullPath()));
	CPPUNIT_ASSERT(mTempDir.FileExists());
	mpConfig->DataDirectory.Set(mTempDir.GetFullPath());
	CheckForwardError(BM_SETUP_WIZARD_FILE_IS_A_DIRECTORY);
	CPPUNIT_ASSERT(wxRemoveFile(mTempDir.GetFullPath()));
	
	CPPUNIT_ASSERT(wxMkdir(mTempDir.GetFullPath(), 0000));
	CheckForwardError(BM_SETUP_WIZARD_FILE_NOT_READABLE);
	CPPUNIT_ASSERT(wxRmdir(mTempDir.GetFullPath()));

	CPPUNIT_ASSERT(wxMkdir(mTempDir.GetFullPath(), 0600));
	CheckForwardError(BM_SETUP_WIZARD_FILE_NOT_READABLE);
	CPPUNIT_ASSERT(wxRmdir(mTempDir.GetFullPath()));

	CPPUNIT_ASSERT(wxMkdir(mTempDir.GetFullPath(), 0500));
	CheckForwardError(BM_SETUP_WIZARD_FILE_NOT_WRITABLE);
	CPPUNIT_ASSERT(wxRmdir(mTempDir.GetFullPath()));
	
	CPPUNIT_ASSERT(wxMkdir(mTempDir.GetFullPath(), 0770));
	CheckForwardError(BM_SETUP_WIZARD_BAD_FILE_PERMISSIONS);
	CPPUNIT_ASSERT(wxRmdir(mTempDir.GetFullPath()));

	CPPUNIT_ASSERT(wxMkdir(mTempDir.GetFullPath(), 0707));
	CheckForwardError(BM_SETUP_WIZARD_BAD_FILE_PERMISSIONS);
	CPPUNIT_ASSERT(wxRmdir(mTempDir.GetFullPath()));

	wxFileName tempdir2(mTempDir.GetFullPath(), _("foo"));
	mpConfig->DataDirectory.Set(tempdir2.GetFullPath());
	CheckForwardError(BM_SETUP_WIZARD_FILE_DIR_NOT_FOUND);
	
	CPPUNIT_ASSERT(wxMkdir(mTempDir.GetFullPath(), 0400));
	CheckForwardError(BM_SETUP_WIZARD_FILE_DIR_NOT_WRITABLE);
	CPPUNIT_ASSERT(wxRmdir(mTempDir.GetFullPath()));

	CPPUNIT_ASSERT(wxMkdir(mTempDir.GetFullPath(), 0600));
	CheckForwardError(BM_SETUP_WIZARD_FILE_DIR_NOT_WRITABLE);
	CPPUNIT_ASSERT(wxRmdir(mTempDir.GetFullPath()));

	CPPUNIT_ASSERT(wxMkdir(mTempDir.GetFullPath(), 0700));
	mpConfig->DataDirectory.Set(mTempDir.GetFullPath());
	ClickForward();
	
	wxFileName socket (mTempDir.GetFullPath(), _("bbackupd.sock"));
	wxFileName pidfile(mTempDir.GetFullPath(), _("bbackupd.pid"));
	CPPUNIT_ASSERT(mpConfig->CommandSocket.Is(socket.GetFullPath()));
	CPPUNIT_ASSERT(mpConfig->PidFile      .Is(pidfile.GetFullPath()));

	// last page, wizard should have closed
	CPPUNIT_ASSERT_EQUAL(0, WxGuiTestHelper::FlushEventQueue());
	CPPUNIT_ASSERT(!FindWindow(ID_Setup_Wizard_Frame));		
}

void TestWizard::TestConfigOptionalRequiredItems()
{
	CPPUNIT_ASSERT(mConfigTestDir.DirExists());
	mConfigFileName = wxFileName(mConfigTestDir.GetFullPath(),
		_("config.foo"));
	CPPUNIT_ASSERT(!mConfigFileName.FileExists());

	// check that the configuration verifies OK
	wxString msg;
	bool isOk = mpConfig->Check(msg);
	wxCharBuffer buf = msg.mb_str(wxConvBoxi);
	CPPUNIT_ASSERT_MESSAGE(buf.data(), isOk);
	
	// save it
	mpConfig->Save(mConfigFileName.GetFullPath());
	
	#define CHECK_OPTIONAL_PROPERTY(name) \
		CPPUNIT_ASSERT(mpConfig->Check(msg)); \
		mpConfig->name.Clear(); \
		isOk = mpConfig->Check(msg); \
		if (!isOk) { \
		wxString assertMsg; \
		assertMsg.Printf(_("Clearing " #name " caused " \
			"config check to fail: %s"), msg.c_str()); \
		buf = assertMsg.mb_str(wxConvBoxi); \
		CPPUNIT_ASSERT_MESSAGE(buf.data(), isOk); \
		}

	// check that we can remove optional items and it still checks out
	CHECK_OPTIONAL_PROPERTY(StoreObjectInfoFile)
	CHECK_OPTIONAL_PROPERTY(MaxFileTimeInFuture)
	CHECK_OPTIONAL_PROPERTY(AutomaticBackup)
	CHECK_OPTIONAL_PROPERTY(SyncAllowScript)
	CHECK_OPTIONAL_PROPERTY(MaximumDiffingTime)
	CHECK_OPTIONAL_PROPERTY(ExtendedLogging)
	CHECK_OPTIONAL_PROPERTY(CommandSocket)
	CHECK_OPTIONAL_PROPERTY(KeepAliveTime)
	CHECK_OPTIONAL_PROPERTY(StoreObjectInfoFile)
	CHECK_OPTIONAL_PROPERTY(NotifyScript)
	// mpConfig->User.Clear();
	#undef CHECK_OPTIONAL_PROPERTY
	
	// still OK?
	isOk = mpConfig->Check(msg);
	buf = msg.mb_str(wxConvBoxi);
	CPPUNIT_ASSERT_MESSAGE(buf.data(), isOk);
	
	#define CHECK_REQUIRED_PROPERTY(name) \
		CPPUNIT_ASSERT(mpConfig->Check(msg)); \
		mpConfig->name.Clear(); \
		CPPUNIT_ASSERT_MESSAGE("Clearing " #name " did not cause " \
			"config check to fail", !mpConfig->Check(msg)); \
		mpConfig->Load(mConfigFileName.GetFullPath()); \
		CPPUNIT_ASSERT(mpConfig->Check(msg));
		
	CHECK_REQUIRED_PROPERTY(PidFile)
	CHECK_REQUIRED_PROPERTY(StoreHostname)
	CHECK_REQUIRED_PROPERTY(AccountNumber)
	CHECK_REQUIRED_PROPERTY(UpdateStoreInterval)
	CHECK_REQUIRED_PROPERTY(MinimumFileAge)
	CHECK_REQUIRED_PROPERTY(MaxUploadWait)
	CHECK_REQUIRED_PROPERTY(FileTrackingSizeThreshold)
	CHECK_REQUIRED_PROPERTY(DiffingUploadSizeThreshold)
	CHECK_REQUIRED_PROPERTY(CertificateFile)
	CHECK_REQUIRED_PROPERTY(PrivateKeyFile)
	CHECK_REQUIRED_PROPERTY(TrustedCAsFile)
	CHECK_REQUIRED_PROPERTY(KeysFile)
	CHECK_REQUIRED_PROPERTY(DataDirectory)
	
	#undef CHECK_REQUIRED_PROPERTY
}
