/***************************************************************************
 *            Property.h
 *
 *  Mon Feb 28 23:36:29 2005
 *  Copyright 2005-2006 Chris Wilson
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
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
 
#ifndef _PROPERTY_H
#define _PROPERTY_H

#include <string>
#include <wx/string.h>

#include "Configuration.h"

#define STR_PROPS \
STR_PROP(StoreHostname) \
STR_PROP(CertificateFile) \
STR_PROP(PrivateKeyFile) \
STR_PROP(KeysFile) \
STR_PROP(DataDirectory) \
STR_PROP(NotifyScript) \
STR_PROP(TrustedCAsFile) \
STR_PROP(SyncAllowScript) \
STR_PROP(CommandSocket) \
STR_PROP_SUBCONF(PidFile, Server) \
STR_PROP(StoreObjectInfoFile)

#define INT_PROPS \
INT_PROP(AccountNumber) \
INT_PROP(UpdateStoreInterval) \
INT_PROP(MinimumFileAge) \
INT_PROP(MaxUploadWait) \
INT_PROP(FileTrackingSizeThreshold) \
INT_PROP(DiffingUploadSizeThreshold) \
INT_PROP(MaximumDiffingTime) \
INT_PROP(MaxFileTimeInFuture) \
INT_PROP(KeepAliveTime)
	
#define BOOL_PROPS \
BOOL_PROP(ExtendedLogging) \
BOOL_PROP(AutomaticBackup)

#define ALL_PROPS STR_PROPS INT_PROPS BOOL_PROPS

class Property;

class PropertyChangeListener {
	public:
	virtual void OnPropertyChange(Property* pProp) = 0;
	virtual ~PropertyChangeListener() {}
};

class Property {
	protected:
	std::string mKeyName;
	PropertyChangeListener* mpListener;

	public:
	Property(const char * pKeyName, PropertyChangeListener* pListener);
	virtual ~Property() { }
	
	virtual void SetFrom(const Configuration* pConf) = 0;
	virtual void SetClean() = 0;
	virtual bool IsClean () = 0;
	virtual void Reset   () = 0;
	virtual void Revert  () = 0;
	const std::string& GetKeyName() { return mKeyName; }
};

class BoolProperty : public Property 
{
	public:
	BoolProperty(const char * pKeyName, bool value, 
		PropertyChangeListener* pListener);
	BoolProperty(const char * pKeyName, const Configuration* pConf,
		PropertyChangeListener* pListener);
	
	virtual void SetFrom(const Configuration* pConf);
	virtual void Reset();
	virtual void Revert();
	virtual void SetClean();
	virtual bool IsClean();

	const bool* GetPointer();
	void Set(bool newValue);
	void Clear();
	bool GetInto(bool& dest);
	bool Is(bool expectedValue) 
	{ return mConfigured && mValue == expectedValue; }

	private:
	bool mValue, mOriginalValue, mDefaultValue;
	bool mConfigured, mWasConfigured, mDefaultConfigured;
};

class IntProperty : public Property 
{
	public:
	IntProperty(const char * pKeyName, int value,
		PropertyChangeListener* pListener);
	IntProperty(const char * pKeyName, const Configuration* pConf,
		PropertyChangeListener* pListener);
	IntProperty(const char * pKeyName, PropertyChangeListener* pListener);
	
	virtual void SetFrom(const Configuration* pConf);
	virtual void Reset();
	virtual void Revert();
	virtual void SetClean();
	virtual bool IsClean();

	const int* GetPointer();
	void Set(int newValue);
	bool SetFromString(const wxString& rSource);
	void Clear();
	bool GetInto(wxString& rDest);
	bool GetInto(std::string& rDest);
	bool GetInto(int& dest);
	bool IsConfigured() { return mConfigured; }
	bool Is(int expectedValue)
	{ return mConfigured && mValue == expectedValue; }
	
	private:
	int mValue, mOriginalValue, mDefaultValue;
	bool mConfigured, mWasConfigured, mDefaultConfigured;
};

class StringProperty : public Property 
{
	public:
	StringProperty(const char * pKeyName, const std::string& value,
		PropertyChangeListener* pListener);
	StringProperty(const char * pKeyName, const Configuration* pConf,
		PropertyChangeListener* pListener);
	StringProperty(const char * pKeyName, PropertyChangeListener* pListener);

	virtual void SetFrom(const Configuration* pConf);
	virtual void Reset();
	virtual void Revert();
	virtual void SetClean();
	virtual bool IsClean();
		
	const std::string* GetPointer();
	void Set(const wxString&    rNewValue);
	void Set(const std::string& rNewValue);
	void Set(const char *       pNewValue);
	void Clear();
	bool GetInto(std::string& rDest);
	bool GetInto(wxString&    rDest);
	bool IsConfigured() { return mConfigured; }
	bool Is(const wxString& expectedValue)
	{ return mConfigured && expectedValue.IsSameAs(wxString(mValue.c_str(), wxConvLibc)); }
	
	private:
	std::string mValue, mOriginalValue, mDefaultValue;
	bool mConfigured, mWasConfigured, mDefaultConfigured;
};	

#endif /* _PROPERTY_H */
