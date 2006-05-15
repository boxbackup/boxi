///////////////////////////////////////////////////////////////////////////////
// Name:        swWxGuiTesting/swWxGuiTestEventSimulationHelper.h
// Author:      Reinhold Füreder
// Created:     2004
// Copyright:   (c) 2005 Reinhold Füreder
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef SWWXGUITESTEVENTSIMULATIONHELPER_H
#define SWWXGUITESTEVENTSIMULATIONHELPER_H

#ifdef __GNUG__
    #pragma interface "swWxGuiTestEventSimulationHelper.h"
#endif

// #include "Common.h"
#include <wx/wx.h>

class wxTreeItemId;
class wxTreeCtrl;
class wxSpinCtrl;

namespace sw {
    class ToolBar;
    class SpinCtrlDouble;
}

// namespace swTst {


/*! \class WxGuiTestEventSimulationHelper
    \brief Provides methods for simulating control specific events.

    Flushing the event queue is not part of the event simulation helper methods!
*/
class WxGuiTestEventSimulationHelper
{
public:
    /*! \fn WxGuiTestEventSimulationHelper ()
        \brief Constructor
    */
    WxGuiTestEventSimulationHelper ();


    /*! \fn virtual ~WxGuiTestEventSimulationHelper ()
        \brief Destructor
    */
    virtual ~WxGuiTestEventSimulationHelper ();


    /*! \fn static void SelectMenuItem (int id, wxWindow *frame)
        \brief Select a menu item.

        \param id id of menu item to select
        \param frame frame owning the menu bar where the menu item is a child
            of (= parent of menu item)
    */
    static void SelectMenuItem (int id, wxWindow *frame);


    /*! \fn static void SelectMenuItem (int id, wxEvtHandler *evtHandler)
        \brief Select a menu item (convenience method for popup menus).

        \param id id of menu item to select
        \param evtHandler menu (= event handler) holding the menu item
    */
    static void SelectMenuItem (int id, wxEvtHandler *evtHandler);


    /*! \fn static void SelectAndCheckMenuItem (int id, wxWindow *window, wxMenu **menu = NULL)
        \brief Select and check a menu item.

        \param id id of menu item to select and check
        \param window window owning the menu (bar) where the menu item is a child
            of (= parent of menu item)
        \param menu menu owning the menu item; if NULL, it is assumed the window
            is in fact a wxFrame with a menu bar holding the menu item
    */
    static void SelectAndCheckMenuItem (int id, wxWindow *window, wxMenu **menu = NULL);


    /*! \fn static void ClickButton (int id, wxWindow *parent)
        \brief Click on a button.

        \param id id of button to click
        \param parent parent window owning the button (or button itself)
    */
    static void ClickButton (int id, wxWindow *parent);


    /*! \fn static void SetTextCtrlValue (wxTextCtrl *textCtrl, const wxString &value)
        \brief Set text control value.

        \param textCtrl text control to manipulate
        \param value new value for text control
    */
    static void SetTextCtrlValue (wxTextCtrl *textCtrl,
            const wxString &value);


    /*! \fn static void SetSpinCtrlDblValue (sw::SpinCtrlDouble *spinCtrl, double value)
        \brief Set double typed spin control value.

        \param spinCtrl double typed spin control to manipulate
        \param value new value for spin control
    */
    static void SetSpinCtrlDblValue (sw::SpinCtrlDouble *spinCtrl, double value);

            
    /*! \fn static void SetSpinCtrlDblValueWithoutEvent (sw::SpinCtrlDouble *spinCtrl, double value)
        \brief Set double typed spin control value without generating event.

        \param spinCtrl double typed spin control to manipulate
        \param value new value for spin control
    */
    static void SetSpinCtrlDblValueWithoutEvent (sw::SpinCtrlDouble *spinCtrl,
            double value);
    

    /*! \fn static void SelectTreeItem (const wxTreeItemId &id, wxTreeCtrl *treeCtrl)
        \brief Select a tree item.

        \param id id of tree item to select
        \param treeCtrl tree control owning the tree item (= parent of tree item)
    */
    static void SelectTreeItem (const wxTreeItemId &id, wxTreeCtrl *treeCtrl);


    /*! \fn static void RightClickTreeItem (const wxTreeItemId &id, wxTreeCtrl *treeCtrl)
        \brief Right click on visible (!) tree item, i.e. tree must be expanded
        to show this tree item.

        \param id id of tree item to right click
        \param treeCtrl tree control owning the tree item (= parent of tree item)
    */
    static void RightClickTreeItem (const wxTreeItemId &id, wxTreeCtrl *treeCtrl);


    /*! \fn static void SelectNotebookPage (wxNotebook *notebook, unsigned int page)
        \brief Select notebook page.

        \param notebook notebook owning the page, i.e. whose selection should be manipulated
        \param page page of notebook to select
    */
    static void SelectNotebookPage (wxNotebook *notebook, unsigned int page);


    /*! \fn static void SelectChoiceItem (wxChoice *choice, unsigned int item)
        \brief Select choice item.

        \param choice choice whose selection should be manipulated
        \param item index of item to select from choice
    */
    static void SelectChoiceItem (wxChoice *choice, unsigned int item);


    /*! \fn static void SetCheckboxState (wxCheckBox *checkbox, bool isChecked)
        \brief Check or uncheck checkbox.

        \param checkbox checkbox whose state should be manipulated
        \param isChecked sets the checkbox to the given state
    */
    static void SetCheckboxState (wxCheckBox *checkbox, bool isChecked);


    /*! \fn static void SelectRadioBoxItem (wxRadioBox *radioBox, unsigned int item)
        \brief Select radiobox item.

        \param radioBox radio box whose selection should be manipulated
        \param item index of item to select from radio box
    */
    static void SelectRadioBoxItem (wxRadioBox *radioBox, unsigned int item);


    /*! \fn static void SetSliderValue (wxSlider *slider, unsigned int value)
        \brief Set slider value.

        \param slider slider that needs to be changed.
        \param value value of slider to set.
    */
    static void SetSliderValue (wxSlider *slider, unsigned int value);


    /*! \fn static void SetSpinCtrlValue (wxSpinCtrl *spinCtrl, int value)
        \brief Set spin control value.

        \param spinCtrl spinctrl that needs to be changed.
        \param value value of spin control to set.
    */
    static void SetSpinCtrlValue (wxSpinCtrl *spinCtrl, int value);


    /*! \fn static bool IsSettingSpinCtrlValue ()
        \brief Return if spin control value is currently being set/simulated.
        
        Only needed for correct capturing of standard wxSpinCtrl, as it does
        NOT trigger a spin control event when text, i.e. the integer value, is
        entered.

        Based on s_isSettingSpinCtrlValue member.

        \return true, if spin control value is currently being set/simulated
    */
    static bool IsSettingSpinCtrlValue ();


    /*! \fn static void ToggleToolOnlyEvent (int id, bool enabled, wxWindow *parent)
        \brief Toggle a tool of a toolbar without GUI state changing.
        
        This is almost equivalent to selecting a menu item.

        \param id id of tool to toggle
        \param enabled if true, toggles on the tool
        \param parent parent window owning the tool
    */
    static void ToggleToolOnlyEvent (int id, bool enabled, wxWindow *parent);


    /*! \fn static void ToggleTool (int id, bool enabled, sw::ToolBar *toolbar, wxWindow *parent)
        \brief Toggle a tool of a toolbar.
        
        This is almost equivalent to selecting a menu item.

        \param id id of tool to toggle
        \param enabled if true, toggles on the tool
        \param toolbar toolbar owning the tool with the given id
        \param parent parent window owning the tool or toolbar
    */
    static void ToggleTool (int id, bool enabled, sw::ToolBar *toolbar, wxWindow *parent);

protected:

private:
    static bool s_isSettingSpinCtrlValue;

};

// } // End namespace swTst

#endif // SWWXGUITESTEVENTSIMULATIONHELPER_H
