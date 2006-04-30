///////////////////////////////////////////////////////////////////////////////
// Name:        swWxGuiTesting/swWxGuiTestHelper.cpp
// Author:      Reinhold Füreder
// Created:     2004
// Copyright:   (c) 2005 Reinhold Füreder
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifdef __GNUG__
    #pragma implementation "WxGuiTestHelper.h"
#endif

#include <wx/app.h>
#include <wx/dialog.h>
#include <wx/msgdlg.h>

#include "WxGuiTestHelper.h"

#include "WxGuiTestWarningAsserterInterface.h"
#include "WxGuiTestProvokedWarningRegistry.h"

// #include "swDefaultDialog.h"
// #include "swFrameFactory.h"

// namespace swTst {

// Set default settings for normal application logic:
// "Single stepping" through unit test case code:
bool WxGuiTestHelper::s_useExitMainLoopOnIdle = true;
bool WxGuiTestHelper::s_doExitMainLoopOnIdle = true;
// Allow manual visual checking of GUI (cf. temporary interactive test):
bool WxGuiTestHelper::s_isGuiLessUnitTest = false;
// Unit testing will be blocked, but by default the app should act normaly:
bool WxGuiTestHelper::s_showModalDialogsNonModal = false;
bool WxGuiTestHelper::s_showPopupMenus = true;
// By default enable test interactivity:
bool WxGuiTestHelper::s_disableTestInteractivity = false;
// By default pop-up warning message box on failing assertions:
bool WxGuiTestHelper::s_popupWarningForFailingAssert = true;
// By default don't check the occurence of unexpected warnings:
bool WxGuiTestHelper::s_checkForProvokedWarnings = false;

WxGuiTestWarningAsserterInterface *WxGuiTestHelper::s_warningAsserter = NULL;

WxGuiTestHelper::PopupMenuMap WxGuiTestHelper::s_popupMenuMap;

wxString WxGuiTestHelper::s_fileOfFirstTestFailure;
int WxGuiTestHelper::s_lineNmbOfFirstTestFailure = -1;
wxString WxGuiTestHelper::s_shortDescriptionOfFirstTestFailure;
wxString WxGuiTestHelper::s_accTestFailures;


WxGuiTestHelper::WxGuiTestHelper ()
{
    // Nothing to do
}


WxGuiTestHelper::~WxGuiTestHelper ()
{
    s_popupMenuMap.clear ();
    if (s_warningAsserter != NULL) {

        delete s_warningAsserter;
        s_warningAsserter = NULL;
    }
}


int WxGuiTestHelper::FlushEventQueue ()
{
    // Reset test case failure occurence store:
    s_fileOfFirstTestFailure.Clear ();
    s_lineNmbOfFirstTestFailure = -1;
    s_accTestFailures.Clear ();

    bool oldUseExitMainLoopOnIdle = s_useExitMainLoopOnIdle;
    bool oldDoExitMainLoopOnIdle = s_doExitMainLoopOnIdle;
    int retCode;

    s_useExitMainLoopOnIdle = true;
    s_doExitMainLoopOnIdle = true;

    // Check if really all events under GTK do not send an explicit 
    // additional idle event (instead of being lazy):
#ifdef __WXGTK__
    wxIdleEvent *idleEvt = new wxIdleEvent ();
    ::wxPostEvent (wxTheApp->GetTopWindow ()->GetEventHandler (), *idleEvt);
#endif

#ifdef __WXGTK__
    // if (wxTheApp->Pending ()) {
#endif
        retCode = wxTheApp->MainLoop ();
#ifdef __WXGTK__
    // }
#endif

    s_useExitMainLoopOnIdle = oldUseExitMainLoopOnIdle;
    s_doExitMainLoopOnIdle = oldDoExitMainLoopOnIdle;

    // If a failure has occured, throw exception:
    if (!s_accTestFailures.IsEmpty ()) {

        wxASSERT (s_warningAsserter != NULL);
        s_warningAsserter->FailAssert (s_fileOfFirstTestFailure,
                s_lineNmbOfFirstTestFailure,
                s_shortDescriptionOfFirstTestFailure, s_accTestFailures);
    }

    return retCode;
}

void WxGuiTestHelper::ShowModal(wxDialog* pWindow)
{
	if (WxGuiTestHelper::s_showModalDialogsNonModal)
	{
		pWindow->Show();
	}
	else
	{
		pWindow->ShowModal();
	}
}

int WxGuiTestHelper::Show (wxWindow *wdw, bool show, bool isModal)
{
    return WxGuiTestHelper::Show (wdw, show, isModal,
            WxGuiTestHelper::s_isGuiLessUnitTest);
}


int WxGuiTestHelper::Show (wxWindow *wdw, bool show, bool isModal,
        bool isGuiLessUnitTest)
{
    wxASSERT (wdw != NULL);

    int ret = -1;

    if (isModal) {

        // Only dialogs can be shown modal:
        wxDialog *dlg = wxDynamicCast (wdw, wxDialog);
        wxASSERT (dlg != NULL);

        if (isGuiLessUnitTest) {

            // Nothing to do/show.

        } else {

            if (show) {

                if (WxGuiTestHelper::s_showModalDialogsNonModal) {

                    ret = dlg->Show ();

                } else {

                    ret = dlg->ShowModal ();
                }

            } else {

                if (WxGuiTestHelper::s_showModalDialogsNonModal) {

                    ret = dlg->Hide ();

                } else {

                    dlg->EndModal (show);
                }
            }
        }

    } else {

        if (!isGuiLessUnitTest) {

            if (show) {

                ret = wdw->Show (show);

            } else {

                ret = wdw->Hide ();
            }
        }
    }

    return ret;
}


bool WxGuiTestHelper::PopupMenu (wxWindow *wdw, wxMenu *menu,
        const wxPoint &pos, const wxString &cacheMapKey)
{
    return WxGuiTestHelper::PopupMenu (wdw, menu, pos, cacheMapKey,
            WxGuiTestHelper::s_isGuiLessUnitTest);
}


bool WxGuiTestHelper::PopupMenu (wxWindow *wdw, wxMenu *menu,
        const wxPoint &pos, const wxString &cacheMapKey, bool isGuiLessUnitTest)
{
    bool ret = false;

    // Cache wxMenu for subsequent finding in test units:
    WxGuiTestHelper::s_popupMenuMap[cacheMapKey] = std::make_pair (menu, wdw);

    if (isGuiLessUnitTest) {

        // Do nothing

    } else {

        if (WxGuiTestHelper::s_showPopupMenus) {

            ret = wdw->PopupMenu (menu, pos);

        } else {

            // Do nothing
        }
    }

    return ret;
}

/*
sw::DefaultDialog * WxGuiTestHelper::CreateDefaultDialog (bool isModal)
{
    if ((isModal) && (!WxGuiTestHelper::s_showModalDialogsNonModal)) {

        return sw::FrameFactory::GetInstance ()->CreateDefaultDialog ();

    } else {

        return sw::FrameFactory::GetInstance ()->CreateDefaultDialog (
                wxDEFAULT_DIALOG_STYLE);
    }
}
*/

void WxGuiTestHelper::BreakTestToShowCurrentGui ()
{
	if (WxGuiTestHelper::GetDisableTestInteractivity ()) {

		return;
	}

    ::wxMessageBox (_T("Continue unit testing?"),
            _T("wxGui CppUnit Testing Suspended"), wxOK | wxICON_QUESTION);
}


void WxGuiTestHelper::SetUseExitMainLoopOnIdleFlag (bool use)
{
    s_useExitMainLoopOnIdle = use;
}

    
bool WxGuiTestHelper::GetUseExitMainLoopOnIdleFlag ()
{
    return s_useExitMainLoopOnIdle;
}


void WxGuiTestHelper::SetExitMainLoopOnIdleFlag (bool doExit)
{
    s_doExitMainLoopOnIdle = doExit;
}

    
bool WxGuiTestHelper::GetExitMainLoopOnIdleFlag ()
{
    return s_doExitMainLoopOnIdle;
}


void WxGuiTestHelper::SetIsGuiLessUnitTestFlag (bool isGuiLess)
{
    s_isGuiLessUnitTest = isGuiLess;
}

    
bool WxGuiTestHelper::IsGuiLessUnitTestFlag ()
{
    return s_isGuiLessUnitTest;
}


void WxGuiTestHelper::SetShowModalDialogsNonModalFlag (bool showNonModal)
{
    s_showModalDialogsNonModal = showNonModal;
}


bool WxGuiTestHelper::GetShowModalDialogsNonModalFlag ()
{
    return s_showModalDialogsNonModal;
}


void WxGuiTestHelper::SetShowPopupMenusFlag (bool showPopupMenus)
{
    s_showPopupMenus = showPopupMenus;
}


bool WxGuiTestHelper::GetShowPopupMenusFlag ()
{
    return s_showPopupMenus;
}


void WxGuiTestHelper::SetDisableTestInteractivity (bool disable)
{
	s_disableTestInteractivity = disable;
}


bool WxGuiTestHelper::GetDisableTestInteractivity ()
{
	return s_disableTestInteractivity;
}


void WxGuiTestHelper::SetPopupWarningForFailingAssert (bool popup)
{
    s_popupWarningForFailingAssert = popup;
}


bool WxGuiTestHelper::GetPopupWarningForFailingAssert ()
{
    return s_popupWarningForFailingAssert;
}


wxMenu *WxGuiTestHelper::FindPopupMenu (const wxString &key)
{
    PopupMenuMap::const_iterator it;

    it = s_popupMenuMap.find (key);
    return (it != s_popupMenuMap.end () ? (*it).second.first : NULL);
}


wxString WxGuiTestHelper::FindPopupMenuKey (wxMenu *menu)
{
    bool found = false;
    PopupMenuMap::const_iterator it = s_popupMenuMap.begin ();
    while ((!found) && (it != s_popupMenuMap.end ())) {

        if ((*it).second.first == menu) {

            found = true;

        } else {
        
            it++;
        }
    }
    return (found ? (*it).first : wxT(""));
}


wxWindow *WxGuiTestHelper::FindPopupMenuEvtHandlerWdw (const wxString &key)
{
    PopupMenuMap::const_iterator it;

    it = s_popupMenuMap.find (key);
    return (it != s_popupMenuMap.end () ? (*it).second.second : NULL);
}

/*
bool WxGuiTestHelper::IsProvokedWarning (const wxString &caption,
        const wxString &message)
{
    bool isProvoked = false;

    if (WxGuiTestHelper::GetCheckForProvokedWarnings ()) {

        WxGuiTestProvokedWarningRegistry &provWarningRegistry =
                WxGuiTestProvokedWarningRegistry::GetInstance ();
        const WxGuiTestProvokedWarning *warning =
                provWarningRegistry.FindRegisteredWarning (caption, message);
        if ((warning != NULL) && (
                provWarningRegistry.IsRegisteredAndInTime (*warning))) {

            // It is a provoked warning -> label it as detected:
            provWarningRegistry.SetWarningAsDetected (*warning);
            isProvoked = true;

        } else {

            // Unexpected warning -> test case failure:
            wxString failMsg = wxString::Format (
                    wxT("Caption \"%s\", message \"%s\" occured"), 
		    caption.c_str (), message.c_str ());
            WxGuiTestHelper::AddTestFailure (wxT(""), -1,
                    wxT("Unexpected App application warning detected"), failMsg);
            //WxGuiTestHelper::SetCheckForProvokedWarnings (false);
            wxTheApp->ExitMainLoop ();
        }
    }

    return isProvoked;
}


void WxGuiTestHelper::SetCheckForProvokedWarnings (bool check)
{
    WxGuiTestHelper::s_checkForProvokedWarnings = check;
}


bool WxGuiTestHelper::GetCheckForProvokedWarnings ()
{
    return WxGuiTestHelper::s_checkForProvokedWarnings;
}
*/

void WxGuiTestHelper::SetWarningAsserter (
        WxGuiTestWarningAsserterInterface *warningAsserter)
{
    // As a convenience we allow resetting the warning asserter with a NULL
    // pointer:
    //wxASSERT (warningAsserter != NULL);
    s_warningAsserter = warningAsserter;
}


void WxGuiTestHelper::AddTestFailure (const wxString &file, const int line,
        const wxString &shortDescription, const wxString &message)
{
    if (s_accTestFailures.IsEmpty ()) {

        s_fileOfFirstTestFailure = file;
        s_lineNmbOfFirstTestFailure = line;
        s_shortDescriptionOfFirstTestFailure = shortDescription;

    } else {

        s_accTestFailures += wxT("\nAND SUBSEQUENTLY:");
        if (!shortDescription.IsEmpty ()) {

            s_accTestFailures += wxT("\n");
            s_accTestFailures += shortDescription;
        }
    }
    s_accTestFailures += wxT("\n");
    s_accTestFailures += message;
}

// } // End namespace swTst
