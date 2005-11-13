/***************************************************************************
 *            Property.h
 *
 *  Mon Feb 28 23:36:29 2005
 *  Copyright  2005  Chris Wilson
 *  anjuta@qwirx.com
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
STR_PROP(CertificateFile) \
STR_PROP(PrivateKeyFile) \
STR_PROP(DataDirectory) \
STR_PROP(NotifyScript) \
STR_PROP(TrustedCAsFile) \
STR_PROP(KeysFile) \
STR_PROP(StoreHostname) \
STR_PROP(SyncAllowScript) \
STR_PROP(CommandSocket) \
STR_PROP_SUBCONF(PidFile, Server)

#define INT_PROPS \
INT_PROP(AccountNumber) \
INT_PROP(UpdateStoreInterval) \
INT_PROP(MinimumFileAge) \
INT_PROP(MaxUploadWait) \
INT_PROP(FileTrackingSizeThreshold) \
INT_PROP(DiffingUploadSizeThreshold) \
INT_PROP(MaximumDiffingTime)
	
#define BOOL_PROPS \
BOOL_PROP(ExtendedLogging)

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
	Property(const char * pKeyName, PropertyChangeListener* pListener)
	{
		mKeyName = pKeyName;
		mpListener = pListener;
	}
	virtual ~Property() { }
	
	virtual void SetFrom(const Configuration* pConf) = 0;
	virtual void SetClean() = 0;
	virtual bool IsClean () = 0;
	const std::string& GetKeyName() { return mKeyName; }
};

class BoolProperty : public Property 
{
	public:
	BoolProperty(const char * pKeyName, bool value, 
		PropertyChangeListener* pListener) 
	: Property(pKeyName, pListener) 
	{
		mValue = value;
		mConfigured = TRUE;
		SetClean();
	}
		
	BoolProperty(const char * pKeyName, const Configuration* pConf,
		PropertyChangeListener* pListener) 
	: Property(pKeyName, pListener) 
	{
		SetFrom(pConf);
	}
	
	void SetFrom(const Configuration* pConf) 
	{
		if (pConf->KeyExists(mKeyName.c_str())) {
			mValue = pConf->GetKeyValueBool(mKeyName.c_str());
			mConfigured = TRUE;
		} else {
			mConfigured = FALSE;
		}
		SetClean();
	}
	
	const bool* Get()        { return mConfigured ? &mValue : NULL; }
	void Set(bool newValue) 
	{
		bool changed = (newValue != mValue || mConfigured == FALSE);
		mValue = newValue; 
		mConfigured = TRUE; 
		if (changed && mpListener) mpListener->OnPropertyChange(this);
	}
	void Clear()
	{ 
		bool changed = (mConfigured == TRUE);
		mConfigured = FALSE; 
		if (changed && mpListener) mpListener->OnPropertyChange(this);
	}
	bool GetInto(bool& dest) { 
		if (mConfigured) 
			dest = mValue; 
		return mConfigured; 
	}

	void SetClean() { 
		mWasConfigured = mConfigured; 
		mOriginalValue = mValue; 
	}
	bool IsClean()  { 
		if (mConfigured != mWasConfigured) return FALSE;
		if (!mConfigured) return TRUE;
		return (mOriginalValue == mValue);
	}

	private:
	bool mValue, mOriginalValue;
	bool mConfigured, mWasConfigured;
};

class IntProperty : public Property 
{
	public:
	IntProperty(const char * pKeyName, int value,
		PropertyChangeListener* pListener) 
	: Property(pKeyName, pListener) 
	{
		this->mValue = value;
		this->mConfigured = TRUE;
		SetClean();
	}
	IntProperty(const char * pKeyName, const Configuration* pConf,
		PropertyChangeListener* pListener) 
	: Property(pKeyName, pListener)
	{
		SetFrom(pConf);
	}
	
	void SetFrom(const Configuration* pConf) 
	{
		if (pConf->KeyExists(mKeyName.c_str())) {
			this->mValue = pConf->GetKeyValueInt(mKeyName.c_str());
			this->mConfigured = TRUE;
		} else {
			this->mConfigured = FALSE;
		}
		SetClean();
	}
	
	const int* Get()       { return mConfigured ? &mValue : NULL; }
	void Set(int newValue) { 
		bool changed = (newValue != mValue || mConfigured == FALSE);
		mValue = newValue; 
		mConfigured = TRUE; 
		if (changed && mpListener) mpListener->OnPropertyChange(this);
	}
	void Clear() { 
		bool changed = (mConfigured == TRUE);
		mConfigured = FALSE;
		if (changed && mpListener) mpListener->OnPropertyChange(this);
	}
	bool GetInto(std::string& dest) { 
		if (!mConfigured) return FALSE;
		wxString formatted;
		formatted.Printf(wxT("%d"), mValue);
		wxCharBuffer buf = formatted.mb_str(wxConvLibc);
		dest = buf.data();
		return TRUE;
	}
	bool GetInto(int& dest) { 
		if (mConfigured) 
			dest = mValue; 
		return mConfigured; 
	}
	void SetClean() { 
		mWasConfigured = mConfigured; 
		mOriginalValue = mValue; 
	}
	bool IsClean()  {
		if (mConfigured != mWasConfigured) return FALSE;
		if (!mConfigured) return TRUE;
		return (mOriginalValue == mValue);
	}
	
	private:
	int mValue, mOriginalValue;
	bool mConfigured, mWasConfigured;
};

class StringProperty : public Property 
{
	public:
	StringProperty(const char * pKeyName, const std::string& value,
		PropertyChangeListener* pListener) 
	: Property(pKeyName, pListener) 
	{
		this->mValue = value;
		if (value.length() > 0)
			this->mConfigured = TRUE;
		else
			this->mConfigured = FALSE;
		SetClean();
	}
	
	StringProperty(const char * pKeyName, const Configuration* pConf,
		PropertyChangeListener* pListener) 
	: Property(pKeyName, pListener) 
	{
		SetFrom(pConf);
	}

	void SetFrom(const Configuration* pConf)	
	{
		if (pConf->KeyExists(mKeyName.c_str())) {
			this->mValue = pConf->GetKeyValue(mKeyName.c_str());
			this->mConfigured = TRUE;
		} else {
			this->mConfigured = FALSE;
		}
		SetClean();
	}
	
	const std::string* Get()              { return mConfigured ? &mValue : NULL; }
	void Set(const std::string& newValue) 
	{ 
		bool changed = (newValue != mValue || mConfigured == FALSE);
		mValue = newValue; 
		mConfigured = TRUE; 
		if (changed && mpListener) mpListener->OnPropertyChange(this);
	}
	void Set(const char * newValue) { 
		bool changed = (newValue != mValue || mConfigured == FALSE);
		mValue = newValue; 
		mConfigured = TRUE; 
		if (changed && mpListener) mpListener->OnPropertyChange(this);
	}
	void Clear() { 
		bool changed = (mConfigured == TRUE);
		mConfigured = FALSE;
		if (changed && mpListener) mpListener->OnPropertyChange(this);
	}
	bool GetInto(std::string& dest) { 
		if (mConfigured) 
			dest = mValue; 
		return mConfigured;
	}
	
	void SetClean() { 
		mWasConfigured = mConfigured; 
		mOriginalValue = mValue; 
	}
	bool IsClean()  { 
		if (mConfigured != mWasConfigured) return FALSE;
		if (!mConfigured) return TRUE;
		return (mOriginalValue == mValue);
	}
	
	private:
	std::string mValue, mOriginalValue;
	bool mConfigured, mWasConfigured;
};	

#endif /* _PROPERTY_H */
