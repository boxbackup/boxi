/***************************************************************************
 *            TestWizard.h
 *
 *  Sat Aug 12 14:34:26 2006
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
 
#ifndef _TESTWIZARD_H
#define _TESTWIZARD_H

#include <openssl/x509.h>

#include <wx/filename.h>

#include "TestFrame.h"

class TestWizard : public TestWithConfig
{
	public:
	TestWizard() { }
	virtual void RunTest();
	static CppUnit::Test *suite();

	private:
	MainFrame*   mpMainFrame;
	wxButton*    mpForwardButton;
	SetupWizard* mpWizard;
	wxFileName   mConfigFileName;
	wxFileName   mTempDir;
	wxFileName   mClientCryptoFileName;
	wxFileName   mClientCertFileName;
	wxFileName   mCaFileName;
	wxFileName   mClientBadSigCertFileName;
	wxFileName   mClientSelfSigCertFileName;
	wxFileName   mClientCsrFileName;
	wxFileName   mPrivateKeyFileName;
	wxFileName   mConfigTestDir;
	
	void CheckForwardErrorImpl(message_t messageId, 
		const wxString& rMessageName, const CppUnit::SourceLine& rLine);
	X509* LoadCertificate(const wxFileName& rCertFile);
	void SignRequestWithKey
	(
		const wxFileName& rRequestFile, 
		const wxFileName& rKeyFile, 
		const wxFileName& rIssuerCertFile, 
		const wxFileName& rCertFile
	);
	void ClickForward()
	{
		ClickButtonWaitEvent(ID_Setup_Wizard_Frame, wxID_FORWARD);
	}
	void ClickBackward()
	{
		ClickButtonWaitEvent(ID_Setup_Wizard_Frame, wxID_BACKWARD);
	}
	void TestAccountPage();
	void TestPrivateKeyPage();
	void TestCertExistsPage();
	void TestCertRequestPage();
	void SignCertificate();
	void TestCertificatePage();
	void TestCryptoKeyPage();
	void TestBackedUpPage();
	void TestDataDirPage();
	void TestConfigOptionalRequiredItems();
};

#endif /* _TESTWIZARD_H */
