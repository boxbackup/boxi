///////////////////////////////////////////////////////////////////////////////
// Name:        swWxGuiTesting/swWxGuiTestHelper.h
// Author:      Reinhold Füreder
// Created:     2004
// Copyright:   (c) 2005 Reinhold Füreder
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef SWWXGUITESTHELPER_H
#define SWWXGUITESTHELPER_H

#ifdef __GNUG__
    #pragma interface "WxGuiTestHelper.h"
#endif

#include <wx/window.h>

// #include "Common.h"

#include <map>

namespace sw {
    class DefaultDialog;
}


// If the special symbol SW_USE_WX_APP_GUI_TESTING is defined in the sources
// CMakeLists.txt, for instance by:
//   ADD_DEFINITIONS (-DSW_USE_WX_APP_GUI_TESTING)
// the sources application won't be recognised as such, but can be used for
// wxAppGuiTesting:
#ifdef SW_USE_WX_APP_GUI_TESTING
    #ifdef IMPLEMENT_APP
        #undef IMPLEMENT_APP
    #endif
    #define IMPLEMENT_APP(appname)
#endif


// namespace swTst {

class WxGuiTestWarningAsserterInterface;


/*! \class WxGuiTestHelper
    \brief Provides some convenience or helper methods for wxGui test cases.
    
    Mainly to (a) configure the behaviour of WxGuiTestApp class (e.g. showing
    modal dialogs non-modal), and (b) facilitate the usage of some common
    features (e.g. flushing the event queue, or making the test temporary
    interactive for manual visual checks during GUI test implementation).

    Currently "single stepping" is implemented through flushing the event queue
    whose finishing is detected by an idle event (cf. s_useExitMainLoopOnIdle,
    s_doExitMainLoopOnIdle flags and their respective getter/setter methods.
    But is it possible that some standard wxWidgets code is sending idle events
    in between?

    The flag s_isGuiLessUnitTest (and getter/setter method) can be used to
    prevent any showing of dialogs -- if the actual call to someDialog->Show()
    is diverted to ::Show() method of this class! Thus, it only works for own
    code, and wxMessageBox as well as wxWidgets standard dialogs are of course
    an exception.

    Finally, s_showModalDialogsNonModal (and getter/setter method) can be used
    to show modal dialogs non-modal. This is critical for unit tests, as modal
    dialogs have their own message processing loop, blocking for some user
    input.

    "Temporary interactive tests" are provided in two different shapes:
        - if inspection is enough and no interaction is desired a standard
        wxMessageBox as used by BreakTestToShowCurrentGui() is enough.
        - if interaction is necessary, use WxGuiTestTempInteractive though.
	This temporary interactivity in (and breaking of) tests can be disabled
	completely for real test runs by means of s_disableTestInteractivity flag
	and the corresponding getter/setter method.
    Likewise failing assertions are popping-up a message box by default, thus
    for real automation of test running the flag s_popupWarningForFailingAssert
    should be set to false via the corresponding setter method.

    Of course, some additonal more complex functionality requires its own
    classes and is therefore not accessible via this class, e.g.:
        - WxGuiTestTimedDialogEnder to automatically close message boxes (or
            in general modal dialogs)
        - "Temporary interactive tests" by means of WxGuiTestTempInteractive

    Finally, the occurence of unexpected or unprovoked warnings via
    App::DisplayWarning() method calls can be detected based on calling
    WxGuiTestHelper::IsProvokedWarning() at the beginning of the
    aforementioned method in the application under test (AUT): such a warning
    means the test case has failed.
*/
class WxGuiTestHelper
{
public:
    /*! \fn WxGuiTestHelper ()
        \brief Constructor
    */
    WxGuiTestHelper ();


    /*! \fn virtual ~WxGuiTestHelper ()
        \brief Destructor
    */
    virtual ~WxGuiTestHelper ();


    /*! \fn static int FlushEventQueue ()
        \brief Flushes event queue in wxWidgets main loop.

        As this functionality requires changing some of the flags, they will be
        stored temporarily and set back at the end to their initial values.

        \return return value of run main loop
    */
    static int FlushEventQueue ();

    static void ShowModal(wxDialog* pWindow);

    /*! \fn static int Show (wxWindow *wdw, bool show, bool isModal)
        \brief Show frames, dialogs conditionally using global s_isGuiLessUnitTest flag.

        \param wdw dialog or frame pointer
        \param show true, if wdw should be shown (taking s_isGuiLessUnitTest into accout)
        \param isModal for dialogs intended to be shown in modal style (taking s_showModalDialogsNonModal into account)
        \return -1 default, otherwise return values of standard Show()/ShowModal() methods
    */
    static int Show (wxWindow *wdw, bool show, bool isModal);


    /*! \fn static int Show (wxWindow *wdw, bool show, bool isModal, bool isGuiLessUnitTest)
        \brief Show frames, dialogs conditionally using isGuiLessUnitTest flag.

        \param wdw dialog or frame handle
        \param show true, if wdw should be shown (taking isGuiLessUnitTest into accout)
        \param isModal for dialogs intended to be shown in modal style (taking s_showModalDialogsNonModal into account)
        \param isGuiLessUnitTest "overwriting" global s_isGuiLessUnitTest flag
        \return -1 default, otherwise return values of standard Show()/ShowModal() methods
    */
    static int Show (wxWindow *wdw, bool show, bool isModal,
            bool isGuiLessUnitTest);


    /*! \fn static bool PopupMenu (wxWindow *wdw, wxMenu *menu, const wxPoint &pos, const wxString &cacheMapKey)
        \brief Pop up menus conditionally using global s_isGuiLessUnitTest flag.

        \param wdw parent component to pop up menu
        \param menu menu to pop up
        \param pos position of menu
        \param cacheMapKey key under which the pop-up menu is cached for subsequent finding in test code
        \return wxWindow::PopupMenu() return value if called, or false otherwise
    */
    static bool PopupMenu (wxWindow *wdw, wxMenu *menu, const wxPoint &pos,
            const wxString &cacheMapKey);


    /*! \fn static bool PopupMenu (wxWindow *wdw, wxMenu *menu, const wxPoint &pos, const wxString &cacheMapKey, bool isGuiLessUnitTest)
        \brief Pop up menus conditionally using global s_isGuiLessUnitTest flag.

        \param wdw parent component to pop up menu
        \param menu menu to pop up
        \param pos position of menu
        \param cacheMapKey key under which the pop-up menu is cached for subsequent finding in test code
        \param isGuiLessUnitTest "overwriting" global s_isGuiLessUnitTest flag
        \return wxWindow::PopupMenu() return value if called, or false otherwise
    */
    static bool PopupMenu (wxWindow *wdw, wxMenu *menu, const wxPoint &pos,
            const wxString &cacheMapKey, bool isGuiLessUnitTest);


	/*! \fn static DefaultDialog * CreateDefaultDialog (bool isModal)
		\brief Create a DefaultDialog with the appropriate modality.

        Internally the current frame factory is employed.

        \param isModal for dialogs intended to be shown in modal style (taking s_showModalDialogsNonModal into account)
	*/
    // static sw::DefaultDialog * CreateDefaultDialog (bool isModal);


    /*! \fn static void BreakTestToShowCurrentGui ()
        \brief Break test and show current GUI and query user for continuation.

        Using a standard wxMessageBox the user can introspect the current GUI
        without interaction. Testing is continued when confirming the popup
        message.
        In fact, only windows/dialogs/frames really "shown" (cf.
        s_isGuiLessUnitTest flag) can be shown.
    */
    static void BreakTestToShowCurrentGui ();


    /*! \fn static void SetUseExitMainLoopOnIdleFlag (bool use)
        \brief Set flag indicating usage of exit main loop on idle event flag.

        \param use if true, use exit main loop on idle event flag
    */
    static void SetUseExitMainLoopOnIdleFlag (bool use);

    
    /*! \fn static bool GetUseExitMainLoopOnIdleFlag ()
        \brief Get flag indicating usage of exit main loop on idle event flag.

        \return true, if exit main loop on idle event flag is taken into account
    */
    static bool GetUseExitMainLoopOnIdleFlag ();


    /*! \fn static void SetExitMainLoopOnIdleFlag (bool doExit)
        \brief Set flag indicating to exit main loop on idle event.

        \param doExit if true, set exit main loop on idle event flag
    */
    static void SetExitMainLoopOnIdleFlag (bool doExit);

    
    /*! \fn static bool GetExitMainLoopOnIdleFlag ()
        \brief Get flag indicating to exit main loop on idle event.

        \return true, if exit main loop on idle event flag is set
    */
    static bool GetExitMainLoopOnIdleFlag ();


    /*! \fn static void SetIsGuiLessUnitTestFlag (bool isGuiLess)
        \brief Set flag indicating if no GUI should be shown at all.

        This is "only" used in ::Show() method which should be employed for all
        own code with the need for showing a dialog.

        \param isGuiLess if true, no GUI should be shown at all
    */
    static void SetIsGuiLessUnitTestFlag (bool isGuiLess);

    
    /*! \fn static bool IsGuiLessUnitTestFlag ()
        \brief Get flag indicating if no GUI should be shown at all.

        \return true, if no GUI should be shown at all
    */
    static bool IsGuiLessUnitTestFlag ();


    /*! \fn static void SetShowModalDialogsNonModalFlag (bool showNonModal)
        \brief Set flag indicating modal dialogs should be shown non-modal.

        Again, this is "only" used in ::Show() method which should be employed
        for all own code with the need for showing a dialog.

        \param showNonModal if true, modal dialogs are shown non-modal
    */
    static void SetShowModalDialogsNonModalFlag (bool showNonModal);

    
    /*! \fn static bool GetShowModalDialogsNonModalFlag ()
        \brief Get flag indicating modal dialogs should be shown non-modal.

        Again, this is "only" used in ::Show() method which should be employed
        for all own code with the need for showing a dialog.

        \return true, if modal dialogs are shown non-modal
    */
    static bool GetShowModalDialogsNonModalFlag ();


    /*! \fn static void SetShowPopupMenusFlag (bool showPopupMenus)
        \brief Set flag indicating popup menu showing.

        This is "only" used in PopupMenu() method which should be employed
        for all own code with the need of popping up a menu.

        \param showPopupMenus if true, pop-up menus are actually shown
    */
    static void SetShowPopupMenusFlag (bool showPopupMenus);

    
    /*! \fn static bool GetShowPopupMenusFlag ()
        \brief Get flag indicating popup menu showing.

        This is "only" used in PopupMenu() method which should be employed
        for all own code with the need of popping up a menu.

        \return true, if pop-up menus are actually shown
    */
    static bool GetShowPopupMenusFlag ();

    
    /*! \fn static void SetDisableTestInteractivity (bool disable)
        \brief Set flag indicating disable status of interactive tests.

        Is used by all methods allowing to either break or temorary making a
		test interactive.

        \param disable if true, no breaking of, or temporary interactivity in tests is allowed
    */
	static void SetDisableTestInteractivity (bool disable);


    /*! \fn static bool GetDisableTestInteractivity ()
        \brief Get flag indicating disable status of interactive tests.

        \return true, if no breaking of, or temporary interactivity in tests is allowed
    */
	static bool GetDisableTestInteractivity ();


    /*! \fn static void SetPopupWarningForFailingAssert (bool popup)
        \brief Set flag indicating if failing assertions should pop-up the
        default warning dialog allowing to break debugging.

        Is only used when testing in debug mode.
        For real test automation the associated flag
        s_popupWarningForFailingAssert should be set to false; as well as
        s_disableTestInteractivity should be set to true.

        \param popup if true, failing assertions pop-up the warning message box
    */
	static void SetPopupWarningForFailingAssert (bool popup);


    /*! \fn static bool GetPopupWarningForFailingAssert ()
        \brief Get flag indicating if failing assertions should pop-up the
        default warning dialog allowing to break debugging.

        \return true, if failing assertions pop-up the warning message box
    */
	static bool GetPopupWarningForFailingAssert ();


    /*! \fn static wxMenu *FindPopupMenu (const wxString &key)
        \brief Look for with key specified pop-up menu in cached pop-up menu map.

        \param key string specifying pop-up menu (used during PopupMenu() method)
        \return pop-up menu cached under specified key, or NULL
    */
    static wxMenu *FindPopupMenu (const wxString &key);


    /*! \fn static wxString FindPopupMenuKey (wxMenu *menu)
        \brief Look for key of given pop-up menu in cached pop-up menu map.

        Only used for capturing & replay!

        \param menu pop-up menu to look up map key
        \return key string of given pop-up menu (used during PopupMenu() method)
    */
    static wxString FindPopupMenuKey (wxMenu *menu);


    /*! \fn static wxWindow *FindPopupMenuEvtHandlerWdw (const wxString &key)
        \brief Look for with key specified pop-up menu event handler window in
        cached pop-up menu map.

        Only used for capturing & replay!

        \param key string specifying pop-up menu event handler window (used
            during PopupMenu() method)
        \return pop-up menu event handler window, or NULL
    */
    static wxWindow *FindPopupMenuEvtHandlerWdw (const wxString &key);


    /*! \fn static bool IsProvokedWarning (const wxString &caption, const wxString &message)
        \brief In testing mode check if this warning is a provoked one to test
        for some non-functional case, or some unfulfilled condition leading to
        a warning.

        Allow detection of unexpected/unprovoked warnings which means a failing
        test case. Internally WxGuiTestProvokedWarningRegistry is used.

        \param caption caption of occured warning
        \param message message of occured warning
        \return true, if warning is a provoked one (i.e. registered)
    */
    static bool IsProvokedWarning (const wxString &caption, const wxString &message);


    /*! \fn static void SetCheckForProvokedWarnings (bool check)
        \brief Set flag indicating check for unexpected warnings.

        \param check if true, means unexpected warnings will lead to test case
            failure in CheckForProvokedWarning() method
    */
	static void SetCheckForProvokedWarnings (bool check);


    /*! \fn static bool GetCheckForProvokedWarnings ()
        \brief Get flag indicating check for unexpected warnings.

        \return true, if unexpected warnings will lead to test case failure in
            CheckForProvokedWarning() method
    */
	static bool GetCheckForProvokedWarnings ();


    /*! \fn static void SetWarningAsserter (WxGuiTestWarningAsserterInterface *warningAsserter)
        \brief Set warning asserter to be used in testing mode.

        \param warningAsserter asserter to be used in testing mode
    */
    static void SetWarningAsserter (WxGuiTestWarningAsserterInterface *warningAsserter);


    static void AddTestFailure (const wxString &file, const int line,
            const wxString &shortDescription, const wxString &message);

private:
    static bool s_useExitMainLoopOnIdle;
    static bool s_doExitMainLoopOnIdle;

    static bool s_isGuiLessUnitTest;

    static bool s_showModalDialogsNonModal;
    static bool s_showPopupMenus;

	static bool s_disableTestInteractivity;

    static bool s_popupWarningForFailingAssert;

    typedef std::pair< wxMenu *, wxWindow * > MenuWdwPair;
    typedef std::map< wxString, MenuWdwPair > PopupMenuMap;
    static PopupMenuMap s_popupMenuMap;

    static bool s_checkForProvokedWarnings;

    static WxGuiTestWarningAsserterInterface *s_warningAsserter;

    // For storing test case failures due to failing assertions and unexpected
    // warnings:
    static wxString s_fileOfFirstTestFailure;
    static int s_lineNmbOfFirstTestFailure;
    static wxString s_shortDescriptionOfFirstTestFailure;
    static wxString s_accTestFailures;
};

// } // End namespace swTst

#endif // SWWXGUITESTHELPER_H
