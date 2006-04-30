///////////////////////////////////////////////////////////////////////////////
// Name:        swWxGuiTesting/swWxGuiTestWarningAsserterInterface.h
// Author:      Reinhold Füreder
// Created:     2004
// Copyright:   (c) 2005 Reinhold Füreder
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef SWWXGUITESTWARNINGASSERTERINTERFACE_H
#define SWWXGUITESTWARNINGASSERTERINTERFACE_H

#ifdef __GNUG__
    #pragma interface "WxGuiTestWarningAsserterInterface.h"
#endif

// #include "Common.h"

// namespace swTst {


/*! \class WxGuiTestWarningAsserterInterface
    \brief Prevent the need to link to CppUnit by removing #include dependency.
*/
class WxGuiTestWarningAsserterInterface
{
public:
    /*! \fn virtual void FailAssert (const wxString &file, const int line, const wxString &shortDescription, const wxString &message) const = 0
        \brief Execute a failing CppUnit framework asserter call.

        \param file file where first failure occured
        \param line line number where first failure occured
        \param shortDescription short description of first failure
        \param message message of occured failure
    */
    virtual void FailAssert (const wxString &file, const int line,
            const wxString &shortDescription, const wxString &message) const = 0;
    virtual ~WxGuiTestWarningAsserterInterface() { }
};

// } // End namespace swTst

#endif // SWWXGUITESTWARNINGASSERTERINTERFACE_H
