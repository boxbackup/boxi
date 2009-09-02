/***************************************************************************
 *            Property.cc
 *
 *  Thu Dec  1 12:55:13 2005
 *  Copyright 2005-2008 Chris Wilson
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

#include "SandBox.h"
 
#include "Property.h"

Property::Property
(
	const char * pKeyName, 
	PropertyChangeListener* pListener
)
{
	mKeyName = pKeyName;
	mpListener = pListener;
}

BoolProperty::BoolProperty
(
	const char * pKeyName, bool value, 
	PropertyChangeListener* pListener
) 
: Property(pKeyName, pListener) 
{
	mValue = value;
	mConfigured = true;
	
	mDefaultValue = mValue;
	mDefaultConfigured = mConfigured;
	
	SetClean();
}
	
BoolProperty::BoolProperty
(
	const char * pKeyName, 
	const Configuration& rConf,
	PropertyChangeListener* pListener
) 
: Property(pKeyName, pListener) 
{
	SetFrom(rConf);

	mDefaultValue = mValue;
	mDefaultConfigured = mConfigured;
}

void BoolProperty::SetFrom(const Configuration& rConf) 
{
	if (rConf.KeyExists(mKeyName)) {
		mValue = rConf.GetKeyValueBool(mKeyName);
		mConfigured = true;
	} else {
		mConfigured = false;
	}
	SetClean();
}

const bool* BoolProperty::GetPointer()
{ 
	return mConfigured ? &mValue : NULL; 
}

void BoolProperty::Set(bool newValue) 
{
	bool changed = (newValue != mValue || mConfigured == false);
	mValue = newValue; 
	mConfigured = true; 
	if (changed && mpListener) mpListener->OnPropertyChange(this);
}

void BoolProperty::Clear()
{ 
	bool changed = (mConfigured == true);
	mConfigured = false; 
	if (changed && mpListener) mpListener->OnPropertyChange(this);
}

void BoolProperty::Reset()
{
	mValue = mDefaultValue;
	mConfigured = mDefaultConfigured;
	SetClean();
}

bool BoolProperty::GetInto(bool& dest) 
{ 
	if (mConfigured) 
	{
		dest = mValue; 
	}
	return mConfigured; 
}

void BoolProperty::SetClean() 
{ 
	mWasConfigured = mConfigured; 
	mOriginalValue = mValue; 
}

bool BoolProperty::IsClean()  
{ 
	if (mConfigured != mWasConfigured) return false;
	if (!mConfigured) return true;
	return (mOriginalValue == mValue);
}

void BoolProperty::Revert()  
{ 
	mConfigured = mWasConfigured;
	mValue = mOriginalValue;
}

bool BoolProperty::IsDefault()  
{ 
	if (mConfigured != mDefaultConfigured) return false;
	if (!mConfigured) return true;
	return (mDefaultValue == mValue);
}

IntProperty::IntProperty
(
	const char * pKeyName, 
	int value,
	PropertyChangeListener* pListener
) 
: Property(pKeyName, pListener) 
{
	mValue = value;
	mConfigured = true;
	
	mDefaultValue = mValue;
	mDefaultConfigured = mConfigured;
	
	SetClean();
}

IntProperty::IntProperty
(
	const char * pKeyName, 
	const Configuration& rConf,
	PropertyChangeListener* pListener
) 
: Property(pKeyName, pListener)
{
	SetFrom(rConf);

	mDefaultValue = mValue;
	mDefaultConfigured = mConfigured;
}

IntProperty::IntProperty
(
	const char * pKeyName, 
	PropertyChangeListener* pListener
) 
: Property(pKeyName, pListener) 
{
	this->mValue = 0;
	this->mConfigured = false;

	mDefaultValue = mValue;
	mDefaultConfigured = mConfigured;

	SetClean();
}

void IntProperty::SetFrom(const Configuration& rConf) 
{
	if (rConf.KeyExists(mKeyName)) {
		this->mValue = rConf.GetKeyValueInt(mKeyName);
		this->mConfigured = true;
	} else {
		this->mConfigured = false;
	}
	SetClean();
}

const int* IntProperty::GetPointer()
{ 
	return mConfigured ? &mValue : NULL; 
}

void IntProperty::Set(int newValue) 
{ 
	bool changed = (newValue != mValue || mConfigured == false);
	mValue = newValue; 
	mConfigured = true; 
	if (changed && mpListener) mpListener->OnPropertyChange(this);
}

bool IntProperty::SetFromString(const wxString& rSource)
{
	if (rSource.Length() == 0) {
		Clear();
		return true;
	}
	
	unsigned int tempValue;
	char *endptr;

	wxCharBuffer buf = rSource.mb_str(wxConvBoxi);
	
	if (rSource.StartsWith(wxT("0x"))) {
		tempValue = strtol(buf.data() + 2, &endptr, 16);
	} else {
		tempValue = strtol(buf.data(), &endptr, 10);
	}
	
	if (*endptr != '\0') {
		return false;
	}

	Set(tempValue);
	return true;
}

void IntProperty::Clear() 
{ 
	bool changed = (mConfigured == true);
	mConfigured = false;
	if (changed && mpListener) mpListener->OnPropertyChange(this);
}

void IntProperty::Reset()
{
	mValue = mDefaultValue;
	mConfigured = mDefaultConfigured;
	SetClean();
}

void IntProperty::Revert()  
{ 
	mConfigured = mWasConfigured;
	mValue = mOriginalValue;
}

bool IntProperty::GetInto(wxString& rDest) 
{ 
	if (!mConfigured)
	{
		return false;
	}
	rDest.Printf(wxT("%d"), mValue);
	return true;
}

bool IntProperty::GetInto(std::string& rDest) 
{ 
	if (!mConfigured)
	{
		return false;
	}
	wxString formatted;
	formatted.Printf(wxT("%d"), mValue);
	wxCharBuffer buf = formatted.mb_str(wxConvBoxi);
	rDest = buf.data();
	return true;
}

bool IntProperty::GetInto(int& dest) 
{ 
	if (mConfigured)
	{
		dest = mValue; 
	}
	return mConfigured;
}

void IntProperty::SetClean() 
{ 
	mWasConfigured = mConfigured; 
	mOriginalValue = mValue; 
}

bool IntProperty::IsClean()  
{
	if (mConfigured != mWasConfigured) return false;
	if (!mConfigured) return true;
	return (mOriginalValue == mValue);
}

bool IntProperty::IsDefault()  
{ 
	if (mConfigured != mDefaultConfigured) return false;
	if (!mConfigured) return true;
	return (mDefaultValue == mValue);
}

StringProperty::StringProperty
(
	const char * pKeyName, 
	const std::string& value,
	PropertyChangeListener* pListener
) 
: Property(pKeyName, pListener) 
{
	this->mValue = value;

	if (value.length() > 0)
		this->mConfigured = true;
	else
		this->mConfigured = false;

	mDefaultValue = mValue;
	mDefaultConfigured = mConfigured;

	SetClean();
}

StringProperty::StringProperty
(
	const char * pKeyName, 
	const Configuration& rConf,
	PropertyChangeListener* pListener
) 
: Property(pKeyName, pListener) 
{
	SetFrom(rConf);

	mDefaultValue = mValue;
	mDefaultConfigured = mConfigured;
}

StringProperty::StringProperty
(
	const char * pKeyName, 
	PropertyChangeListener* pListener
) 
: Property(pKeyName, pListener) 
{
	this->mConfigured = false;

	SetClean();

	mDefaultValue = mValue;
	mDefaultConfigured = mConfigured;
}

void StringProperty::SetFrom(const Configuration& rConf)	
{
	if (rConf.KeyExists(mKeyName)) {
		this->mValue = rConf.GetKeyValue(mKeyName);
		this->mConfigured = true;
	} else {
		this->mConfigured = false;
	}
	SetClean();
}

const std::string* StringProperty::GetPointer() 
{ 
	return mConfigured ? &mValue : NULL; 
}

void StringProperty::Set(const wxString& rNewValue) 
{
	wxString oldValueString(mValue.c_str(), wxConvBoxi);
	bool changed = (rNewValue != oldValueString || mConfigured == false);
	wxCharBuffer buf = rNewValue.mb_str(wxConvBoxi);
	mValue = buf.data(); 
	mConfigured = true; 
	if (changed && mpListener) mpListener->OnPropertyChange(this);
}

void StringProperty::Set(const std::string& rNewValue) 
{ 
	bool changed = (rNewValue != mValue || mConfigured == false);
	mValue = rNewValue; 
	mConfigured = true; 
	if (changed && mpListener) mpListener->OnPropertyChange(this);
}

void StringProperty::Set(const char * pNewValue) 
{	
	bool changed = (pNewValue != mValue || mConfigured == false);
	mValue = pNewValue; 
	mConfigured = true; 
	if (changed && mpListener) mpListener->OnPropertyChange(this);
}

void StringProperty::Clear() 
{ 
	bool changed = (mConfigured == true);
	mConfigured = false;
	if (changed && mpListener) mpListener->OnPropertyChange(this);
}

void StringProperty::Reset()
{
	mValue = mDefaultValue;
	mConfigured = mDefaultConfigured;
	SetClean();
}

void StringProperty::Revert()  
{ 
	mConfigured = mWasConfigured;
	mValue = mOriginalValue;
}

bool StringProperty::GetInto(std::string& rDest) 
{
	if (mConfigured)
	{
		rDest = mValue;
	}
	return mConfigured;
}

bool StringProperty::GetInto(wxString& rDest) 
{
	if (mConfigured)
	{
		rDest = wxString(mValue.c_str(), wxConvBoxi);
	}
	return mConfigured;
}

void StringProperty::SetClean() 
{ 
	mWasConfigured = mConfigured; 
	mOriginalValue = mValue; 
}

bool StringProperty::IsClean()  
{ 
	if (mConfigured != mWasConfigured) return false;
	if (!mConfigured) return true;
	return (mOriginalValue == mValue);
}

bool StringProperty::IsDefault()  
{ 
	if (mConfigured != mDefaultConfigured) return false;
	if (!mConfigured) return true;
	return (mDefaultValue == mValue);
}
