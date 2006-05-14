/***************************************************************************
 *            TestConfig.cc
 *
 *  Tue May  9 19:41:28 2006
 *  Copyright  2006  Chris Wilson
 *  chris-boxisource@qwirx.com
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

#include <wx/file.h>
#include <wx/filename.h>

#include <cppunit/TestSuite.h>
#include <cppunit/extensions/HelperMacros.h>

#include "ClientConfig.h"
#include "MainFrame.h"
#include "TestFrame.h"

class TestConfig : public TestWithConfig
{
	public:
	TestConfig() { }
	virtual void RunTest();
	static CppUnit::Test *suite();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(TestConfig, "WxGuiTest");

CppUnit::Test *TestConfig::suite()
{
	CppUnit::TestSuite *suiteOfTests =
		new CppUnit::TestSuite("TestConfig");
	suiteOfTests->addTest(
		new CppUnit::TestCaller<TestConfig>(
			"TestConfig",
			&TestConfig::RunTest));
	return suiteOfTests;
}

void TestConfig::RunTest()
{
	// check that wxFileDialog behaves itself

	wxFileName configTestDir;
	configTestDir.AssignTempFileName(wxT("boxi-configTestDir-"));
	CPPUNIT_ASSERT(wxRemoveFile(configTestDir.GetFullPath()));
	CPPUNIT_ASSERT(configTestDir.Mkdir(wxS_IRUSR | wxS_IWUSR | wxS_IXUSR));

	CPPUNIT_ASSERT(configTestDir.DirExists());
	wxFileName configFileName(configTestDir.GetFullPath(), wxT("config.foo"));
	CPPUNIT_ASSERT(!configFileName.FileExists());

	{
		wxFileDialog dialog(
			NULL, wxT("Save file"), wxT(""), wxT("bbackupd.conf"), 
			_("Box Backup client configuration file|bbackupd.conf"), 
			wxSAVE | wxOVERWRITE_PROMPT, wxDefaultPosition);
		wxString in = configFileName.GetFullPath();
		dialog.SetPath(configFileName.GetFullPath());
		dialog.SetFilename(configFileName.GetFullName());
		wxString out = dialog.GetPath();
		CPPUNIT_ASSERT_EQUAL(in, out);
	}
	
	// bug in GTK file chooser widget
	// http://bugzilla.gnome.org/show_bug.cgi?id=340835
	/*
	{
		GtkWidget* foo = gtk_file_chooser_dialog_new(
			"hello",
			NULL,
			GTK_FILE_CHOOSER_ACTION_OPEN,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_OPEN,   GTK_RESPONSE_ACCEPT,
			NULL);
		char* expected = "/etc/issue";
		gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(foo),
			expected);
		char* actual = gtk_file_chooser_get_filename(
			GTK_FILE_CHOOSER(foo));
		CPPUNIT_ASSERT(actual != NULL);
		CPPUNIT_ASSERT(strcmp(expected, actual) == 0);
	}

	{
		wxFileDialog dialog(
			NULL, wxT("Open file"), wxT(""), wxT("bbackupd.conf"),  	
			_("Box Backup client configuration file|bbackupd.conf"), 
			wxOPEN | wxFILE_MUST_EXIST, wxDefaultPosition);
		wxString in = configFileName.GetFullPath();
		dialog.SetPath(configFileName.GetFullPath());
		dialog.SetFilename(configFileName.GetFullName());
		wxString out = dialog.GetPath();
		CPPUNIT_ASSERT_EQUAL(in, out);
	}
	*/
	
	MainFrame* pMainFrame = GetMainFrame();
	CPPUNIT_ASSERT(pMainFrame);
	
	mpConfig = pMainFrame->GetConfig();
	CPPUNIT_ASSERT(mpConfig);

	// check that all config options are at defaults
	#define PROP(name) CPPUNIT_ASSERT_MESSAGE(#name \
		" should not be dirty yet", mpConfig->name.IsClean());
	#define STR_PROP_SUBCONF(name, section)  PROP(name)
	#define STR_PROP(name)  PROP(name)
	#define INT_PROP(name)  PROP(name)
	#define BOOL_PROP(name)
	ALL_PROPS
	#undef BOOL_PROP
	#undef INT_PROP
	#undef STR_PROP
	#undef STR_PROP_SUBCONF
	#undef PROP
	
	CPPUNIT_ASSERT(mpConfig->IsClean());
	
	// set them to something wacky
	mpConfig->CertRequestFile.Set(wxT("/foo/req.foo"));
	CPPUNIT_ASSERT(mpConfig->IsClean());
	mpConfig->CertificateFile.Set(wxT("/foo/cert.foo"));
	CPPUNIT_ASSERT(!mpConfig->IsClean());
	mpConfig->PrivateKeyFile.Set (wxT("/foo/privatekey.foo"));
	mpConfig->DataDirectory.Set  (wxT("/var/bbackupd/foo"));
	mpConfig->NotifyScript.Set   (wxT("/foo/foo.sh"));
	mpConfig->TrustedCAsFile.Set (wxT("/foo/serverCA.foo"));
	mpConfig->KeysFile.Set       (wxT("/foo/keysfile.foo"));
	mpConfig->StoreHostname.Set  (wxT("host.foo.bar"));
	mpConfig->SyncAllowScript.Set(wxT("/foo/sync.sh"));
	mpConfig->CommandSocket.Set  (wxT("/foo/socket.foo"));
	mpConfig->PidFile.Set        (wxT("/foo/pid.foo"));
	mpConfig->StoreObjectInfoFile.Set(wxT("/foo/cache.foo"));
	mpConfig->AccountNumber.Set(12345);
	mpConfig->UpdateStoreInterval.Set(1);
	mpConfig->MinimumFileAge.Set(2);
	mpConfig->MaxUploadWait.Set(3);
	mpConfig->FileTrackingSizeThreshold.Set(4);
	mpConfig->DiffingUploadSizeThreshold.Set(5);
	mpConfig->MaximumDiffingTime.Set(6);
	mpConfig->MaxFileTimeInFuture.Set(7);
	mpConfig->KeepAliveTime.Set(8);
	mpConfig->ExtendedLogging.Set(true);
	mpConfig->AutomaticBackup.Set(false);

	// check that all config options are configured
	#define PROP(name) CPPUNIT_ASSERT_MESSAGE(#name \
		" should be configured by now", mpConfig->name.IsConfigured()); \
		CPPUNIT_ASSERT_MESSAGE(#name \
		" should be dirty by now", !mpConfig->name.IsClean());
	#define STR_PROP_SUBCONF(name, section)  PROP(name)
	#define STR_PROP(name)  PROP(name)
	#define INT_PROP(name)  PROP(name)
	#define BOOL_PROP(name)
	ALL_PROPS
	#undef BOOL_PROP
	#undef INT_PROP
	#undef STR_PROP
	#undef STR_PROP_SUBCONF
	#undef PROP
	
	CPPUNIT_ASSERT(mpConfig->CertRequestFile.Is(wxT("/foo/req.foo")));
	CPPUNIT_ASSERT(mpConfig->CertificateFile.Is(wxT("/foo/cert.foo")));
	CPPUNIT_ASSERT(mpConfig->PrivateKeyFile.Is (wxT("/foo/privatekey.foo")));
	CPPUNIT_ASSERT(mpConfig->DataDirectory.Is  (wxT("/var/bbackupd/foo")));
	CPPUNIT_ASSERT(mpConfig->NotifyScript.Is   (wxT("/foo/foo.sh")));
	CPPUNIT_ASSERT(mpConfig->TrustedCAsFile.Is (wxT("/foo/serverCA.foo")));
	CPPUNIT_ASSERT(mpConfig->KeysFile.Is       (wxT("/foo/keysfile.foo")));
	CPPUNIT_ASSERT(mpConfig->StoreHostname.Is  (wxT("host.foo.bar")));
	CPPUNIT_ASSERT(mpConfig->SyncAllowScript.Is(wxT("/foo/sync.sh")));
	CPPUNIT_ASSERT(mpConfig->CommandSocket.Is  (wxT("/foo/socket.foo")));
	CPPUNIT_ASSERT(mpConfig->PidFile.Is        (wxT("/foo/pid.foo")));
	CPPUNIT_ASSERT(mpConfig->StoreObjectInfoFile.Is(wxT("/foo/cache.foo")));
	CPPUNIT_ASSERT(mpConfig->AccountNumber.Is(12345));
	CPPUNIT_ASSERT(mpConfig->UpdateStoreInterval.Is(1));
	CPPUNIT_ASSERT(mpConfig->MinimumFileAge.Is(2));
	CPPUNIT_ASSERT(mpConfig->MaxUploadWait.Is(3));
	CPPUNIT_ASSERT(mpConfig->FileTrackingSizeThreshold.Is(4));
	CPPUNIT_ASSERT(mpConfig->DiffingUploadSizeThreshold.Is(5));
	CPPUNIT_ASSERT(mpConfig->MaximumDiffingTime.Is(6));
	CPPUNIT_ASSERT(mpConfig->MaxFileTimeInFuture.Is(7));
	CPPUNIT_ASSERT(mpConfig->KeepAliveTime.Is(8));
	CPPUNIT_ASSERT(mpConfig->ExtendedLogging.Is(true));
	CPPUNIT_ASSERT(mpConfig->AutomaticBackup.Is(false));
	
	// save the configuration so that we can load it again later
	// expect the "configuration has errors" message while saving
	
	CPPUNIT_ASSERT(!mpConfig->IsClean());
	wxGetApp().ExpectMessageBox(BM_MAIN_FRAME_CONFIG_HAS_ERRORS_WHEN_SAVING,
		wxYES, CPPUNIT_SOURCELINE());
	wxGetApp().ExpectFileDialog(configFileName.GetFullPath());
	ClickMenuItem(ID_File_Save, pMainFrame);
	CPPUNIT_ASSERT(wxGetApp().ShowedFileDialog());
	CPPUNIT_ASSERT(wxGetApp().ShowedMessageBox());
	CPPUNIT_ASSERT(configFileName.FileExists());
	CPPUNIT_ASSERT(mpConfig->IsClean());

	wxRemoveFile(configFileName.GetFullPath());
	CPPUNIT_ASSERT(!configFileName.FileExists());
	wxGetApp().ExpectMessageBox(BM_MAIN_FRAME_CONFIG_HAS_ERRORS_WHEN_SAVING,
		wxYES, CPPUNIT_SOURCELINE());
	ClickMenuItem(ID_File_Save, pMainFrame);
	CPPUNIT_ASSERT(wxGetApp().ShowedMessageBox());
	CPPUNIT_ASSERT(configFileName.FileExists());

	wxRemoveFile(configFileName.GetFullPath());
	CPPUNIT_ASSERT(!configFileName.FileExists());
	wxGetApp().ExpectMessageBox(BM_MAIN_FRAME_CONFIG_HAS_ERRORS_WHEN_SAVING,
		wxYES, CPPUNIT_SOURCELINE());
	wxGetApp().ExpectFileDialog(configFileName.GetFullPath());
	ClickMenuItem(ID_File_SaveAs, pMainFrame);
	CPPUNIT_ASSERT(wxGetApp().ShowedFileDialog());
	CPPUNIT_ASSERT(wxGetApp().ShowedMessageBox());
	CPPUNIT_ASSERT(configFileName.FileExists());

	// file new, check that the configuration is fresh
	// and all settings are at their defaults
	ClickMenuItem(ID_File_New, pMainFrame);
	CPPUNIT_ASSERT(mpConfig->IsClean());
	
	CPPUNIT_ASSERT(!mpConfig->CertRequestFile.IsConfigured());
	CPPUNIT_ASSERT(!mpConfig->CertificateFile.IsConfigured());
	CPPUNIT_ASSERT(!mpConfig->PrivateKeyFile .IsConfigured());
	CPPUNIT_ASSERT(!mpConfig->DataDirectory  .IsConfigured());
	CPPUNIT_ASSERT(!mpConfig->NotifyScript   .IsConfigured());
	CPPUNIT_ASSERT(!mpConfig->TrustedCAsFile .IsConfigured());
	CPPUNIT_ASSERT(!mpConfig->KeysFile       .IsConfigured());
	CPPUNIT_ASSERT(!mpConfig->StoreHostname  .IsConfigured());
	CPPUNIT_ASSERT(!mpConfig->SyncAllowScript.IsConfigured());
	CPPUNIT_ASSERT(!mpConfig->CommandSocket  .IsConfigured());
	CPPUNIT_ASSERT(!mpConfig->PidFile        .IsConfigured());
	CPPUNIT_ASSERT(!mpConfig->StoreObjectInfoFile.IsConfigured());
	CPPUNIT_ASSERT(!mpConfig->AccountNumber .IsConfigured());
	CPPUNIT_ASSERT(mpConfig->UpdateStoreInterval.Is(3600));
	CPPUNIT_ASSERT(mpConfig->MinimumFileAge  .Is(21600));
	CPPUNIT_ASSERT(mpConfig->MaxUploadWait   .Is(86400));
	CPPUNIT_ASSERT(mpConfig->FileTrackingSizeThreshold.Is(65535));
	CPPUNIT_ASSERT(mpConfig->DiffingUploadSizeThreshold.Is(8192));
	CPPUNIT_ASSERT(mpConfig->MaximumDiffingTime.Is(20));
	CPPUNIT_ASSERT(mpConfig->MaxFileTimeInFuture.Is(0));
	CPPUNIT_ASSERT(!mpConfig->KeepAliveTime.IsConfigured());
	CPPUNIT_ASSERT(mpConfig->ExtendedLogging.Is(false));
	CPPUNIT_ASSERT(mpConfig->AutomaticBackup.Is(true));

	// open the saved configuration again
	// cannot do this because of the bug in the file chooser widget
	// wxGetApp().ExpectFileDialog(configFileName.GetFullPath());
	// ClickMenuItem(ID_File_Open, pMainFrame);
	// CPPUNIT_ASSERT(wxGetApp().ShowedFileDialog());
	pMainFrame->DoFileOpen(configFileName.GetFullPath());

	CPPUNIT_ASSERT(!mpConfig->CertRequestFile.IsConfigured());
	CPPUNIT_ASSERT(mpConfig->CertificateFile.Is(wxT("/foo/cert.foo")));
	CPPUNIT_ASSERT(mpConfig->PrivateKeyFile.Is (wxT("/foo/privatekey.foo")));
	CPPUNIT_ASSERT(mpConfig->DataDirectory.Is  (wxT("/var/bbackupd/foo")));
	CPPUNIT_ASSERT(mpConfig->NotifyScript.Is   (wxT("/foo/foo.sh")));
	CPPUNIT_ASSERT(mpConfig->TrustedCAsFile.Is (wxT("/foo/serverCA.foo")));
	CPPUNIT_ASSERT(mpConfig->KeysFile.Is       (wxT("/foo/keysfile.foo")));
	CPPUNIT_ASSERT(mpConfig->StoreHostname.Is  (wxT("host.foo.bar")));
	CPPUNIT_ASSERT(mpConfig->SyncAllowScript.Is(wxT("/foo/sync.sh")));
	CPPUNIT_ASSERT(mpConfig->CommandSocket.Is  (wxT("/foo/socket.foo")));
	CPPUNIT_ASSERT(mpConfig->PidFile.Is        (wxT("/foo/pid.foo")));
	CPPUNIT_ASSERT(mpConfig->StoreObjectInfoFile.Is(wxT("/foo/cache.foo")));
	CPPUNIT_ASSERT(mpConfig->AccountNumber.Is(12345));
	CPPUNIT_ASSERT(mpConfig->UpdateStoreInterval.Is(1));
	CPPUNIT_ASSERT(mpConfig->MinimumFileAge.Is(2));
	CPPUNIT_ASSERT(mpConfig->MaxUploadWait.Is(3));
	CPPUNIT_ASSERT(mpConfig->FileTrackingSizeThreshold.Is(4));
	CPPUNIT_ASSERT(mpConfig->DiffingUploadSizeThreshold.Is(5));
	CPPUNIT_ASSERT(mpConfig->MaximumDiffingTime.Is(6));
	CPPUNIT_ASSERT(mpConfig->MaxFileTimeInFuture.Is(7));
	CPPUNIT_ASSERT(mpConfig->KeepAliveTime.Is(8));
	CPPUNIT_ASSERT(mpConfig->ExtendedLogging.Is(true));
	CPPUNIT_ASSERT(mpConfig->AutomaticBackup.Is(false));
	CPPUNIT_ASSERT(mpConfig->IsClean());

	// check that all config options are configured, and clear them
	#define PROP(name) CPPUNIT_ASSERT_MESSAGE(#name \
		" should be configured by now", mpConfig->name.IsConfigured()); \
		mpConfig->name.Clear(); \
		CPPUNIT_ASSERT(!mpConfig->name.IsConfigured());
	#define STR_PROP_SUBCONF(name, section)  PROP(name)
	#define STR_PROP(name)  PROP(name)
	#define INT_PROP(name)  PROP(name)
	#define BOOL_PROP(name)
	ALL_PROPS
	#undef BOOL_PROP
	#undef INT_PROP
	#undef STR_PROP
	#undef STR_PROP_SUBCONF
	#undef PROP

	// save the configuration with all settings cleared
	wxRemoveFile(configFileName.GetFullPath());
	CPPUNIT_ASSERT(!configFileName.FileExists());
	wxGetApp().ExpectMessageBox(BM_MAIN_FRAME_CONFIG_HAS_ERRORS_WHEN_SAVING,
		wxYES, CPPUNIT_SOURCELINE());
	wxGetApp().ExpectFileDialog(configFileName.GetFullPath());
	ClickMenuItem(ID_File_SaveAs, pMainFrame);
	CPPUNIT_ASSERT(wxGetApp().ShowedFileDialog());
	CPPUNIT_ASSERT(wxGetApp().ShowedMessageBox());
	CPPUNIT_ASSERT(configFileName.FileExists());

	// back to default configuration
	ClickMenuItem(ID_File_New, pMainFrame);
	CPPUNIT_ASSERT(mpConfig->IsClean());

	// load the saved configuration with all settings cleared
	// cannot do this because of the bug in the file chooser widget
	// wxGetApp().ExpectFileDialog(configFileName.GetFullPath());
	// ClickMenuItem(ID_File_Open, pMainFrame);
	// CPPUNIT_ASSERT(wxGetApp().ShowedFileDialog());
	pMainFrame->DoFileOpen(configFileName.GetFullPath());

	// check that no options are configured
	#define PROP(name) CPPUNIT_ASSERT(!mpConfig->name.IsConfigured());
	#define STR_PROP_SUBCONF(name, section)  PROP(name)
	#define STR_PROP(name)  PROP(name)
	#define INT_PROP(name)  PROP(name)
	#define BOOL_PROP(name)
	ALL_PROPS
	#undef BOOL_PROP
	#undef INT_PROP
	#undef STR_PROP
	#undef STR_PROP_SUBCONF
	#undef PROP

	// back to default configuration again
	ClickMenuItem(ID_File_New, pMainFrame);
	CPPUNIT_ASSERT(mpConfig->IsClean());
	
	// close the main frame, check that it closes cleanly
	ClickMenuItem(wxID_EXIT, pMainFrame);
	CPPUNIT_ASSERT_EQUAL(0, WxGuiTestHelper::FlushEventQueue());
	CPPUNIT_ASSERT(!FindWindow(ID_Main_Frame));
}
