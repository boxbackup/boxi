///////////////////////////////////////////////////////////////////////////////
// Name:        swWxGuiTesting/swWxGuiTestProvokedWarning.h
// Author:      Reinhold Füreder
// Created:     2004
// Copyright:   (c) 2005 Reinhold Füreder
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef SWWXGUITESTPROVOKEDWARNING_H
#define SWWXGUITESTPROVOKEDWARNING_H

#ifdef __GNUG__
    #pragma interface "WxGuiTestProvokedWarning.h"
#endif

// #include "Common.h"

#include "wx/datetime.h"

// namespace swTst {


/*! \class WxGuiTestProvokedWarning
    \brief Holds information semi-uniquely identifying provoked/expected warnings.

    Based on the specified timeout interval provoked warnings must occur, i.e.
    pop-up within a given time span. This avoids conflicts with other provoked
    warnings issued later with overlapping or even equal caption and message
    parameters (=> semi-uniquely identification).
*/
class WxGuiTestProvokedWarning
{
public:
    /*! \fn WxGuiTestProvokedWarning (const wxString &caption, const wxString *message, const unsigned int timeout)
        \brief Constructor

        \param caption caption of provoked/expected warnings
        \param message message of provoked/expected warnings, or NULL if only
            caption should be used for convenience reasons; message is deleted
            in destructor if none-NULL
        \param timeout timeout in seconds starting during object creation
    */
    WxGuiTestProvokedWarning (const wxString &caption,
            const wxString *message, const unsigned int timeout);


    /*! \fn virtual ~WxGuiTestProvokedWarning ()
        \brief Destructor
    */
    virtual ~WxGuiTestProvokedWarning ();


    /*! \fn virtual const wxString & GetCaption () const
        \brief Get caption being part of the semi-uniquely identification.

        \return caption of expected warning
    */
    virtual const wxString & GetCaption () const;


    /*! \fn virtual const wxString * GetMessage () const
        \brief Get message being part of the semi-uniquely identification.

        If NULL, only caption should be used for convenience reasons.

        \return message of expected warning, may be NULL
    */
    virtual const wxString * GetMessage () const;


    /*! \fn virtual const wxDateTime & GetTimeout () const
        \brief Get timeout of the provoked/expected warning.

        \return timeout of expected warning
    */
    virtual const wxDateTime & GetTimeout () const;

protected:

private:
    wxString m_caption;
    const wxString *m_message;
    wxDateTime m_timeout;

private:
    // No copy and assignment constructor:
    WxGuiTestProvokedWarning (const WxGuiTestProvokedWarning &rhs);
    WxGuiTestProvokedWarning & operator= (const WxGuiTestProvokedWarning &rhs);
};

// } // End namespace swTst

#endif // SWWXGUITESTPROVOKEDWARNING_H
