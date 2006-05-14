/***************************************************************************
 *            Property.cc
 *
 *  Thu Dec  1 12:55:13 2005
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
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
 
#include "Property.h"

Property::Property(
	const char * pKeyName, 
	PropertyChangeListener* pListener)
{
	mKeyName = pKeyName;
	mpListener = pListener;
}

BoolProperty::BoolProperty(
	const char * pKeyName, bool value, 
	PropertyChangeListener* pListener) 
: Property(pKeyName, pListener) 
{
	mValue = value;
	mConfigured = TRUE;
	
	mDefaultValue = mValue;
	mDefaultConfigured = mConfigured;
	
	SetClean();
}
	
BoolProperty::BoolProperty(
	const char * pKeyName, const Configuration* pConf,
	PropertyChangeListener* pListener) 
: Property(pKeyName, pListener) 
{
	SetFrom(pConf);

	mDefaultValue = mValue;
	mDefaultConfigured = mConfigured;
}

void BoolProperty::SetFrom(const Configuration* pConf) 
{
	if (pConf->KeyExists(mKeyName.c_str())) {
		mValue = pConf->GetKeyValueBool(mKeyName.c_str());
		mConfigured = TRUE;
	} else {
		mConfigured = FALSE;
	}
	SetClean();
}

const bool* BoolProperty::Get()
{ 
	return mConfigured ? &mValue : NULL; 
}

void BoolProperty::Set(bool newValue) 
{
	bool changed = (newValue != mValue || mConfigured == FALSE);
	mValue = newValue; 
	mConfigured = TRUE; 
	if (changed && mpListener) mpListener->OnPropertyChange(this);
}

void BoolProperty::Clear()
{ 
	bool changed = (mConfigured == TRUE);
	mConfigured = FALSE; 
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
		dest = mValue; 
	return mConfigured; 
}

void BoolProperty::SetClean() 
{ 
	mWasConfigured = mConfigured; 
	mOriginalValue = mValue; 
}

bool BoolProperty::IsClean()  { 
	if (mConfigured != mWasConfigured) return FALSE;
	if (!mConfigured) return TRUE;
	return (mOriginalValue == mValue);
}

IntProperty::IntProperty(
	const char * pKeyName, int value,
	PropertyChangeListener* pListener) 
: Property(pKeyName, pListener) 
{
	mValue = value;
	mConfigured = TRUE;
	
	mDefaultValue = mValue;
	mDefaultConfigured = mConfigured;
	
	SetClean();
}

IntProperty::IntProperty(
	const char * pKeyName, 
	const Configuration* pConf,
	PropertyChangeListener* pListener) 
: Property(pKeyName, pListener)
{
	SetFrom(pConf);

	mDefaultValue = mValue;
	mDefaultConfigured = mConfigured;
}

IntProperty::IntProperty(
	const char * pKeyName, PropertyChangeListener* pListener) 
: Property(pKeyName, pListener) 
{
	this->mValue = 0;
	this->mConfigured = FALSE;

	mDefaultValue = mValue;
	mDefaultConfigured = mConfigured;

	SetClean();
}

void IntProperty::SetFrom(const Configuration* pConf) 
{
	if (pConf->KeyExists(mKeyName.c_str())) {
		this->mValue = pConf->GetKeyValueInt(mKeyName.c_str());
		this->mConfigured = TRUE;
	} else {
		this->mConfigured = FALSE;
	}
	SetClean();
}

const int* IntProperty::Get()
{ 
	return mConfigured ? &mValue : NULL; 
}

void IntProperty::Set(int newValue) 
{ 
	bool changed = (newValue != mValue || mConfigured == FALSE);
	mValue = newValue; 
	mConfigured = TRUE; 
	if (changed && mpListener) mpListener->OnPropertyChange(this);
}

bool IntProperty::SetFromString(const wxString& rSource)
{
	if (rSource.Length() == 0) {
		Clear();
		return TRUE;
	}
	
	unsigned int tempValue;
	char *endptr;

	wxCharBuffer buf = rSource.mb_str(wxConvLibc);
	
	if (rSource.StartsWith(wxT("0x"))) {
		tempValue = strtol(buf.data() + 2, &endptr, 16);
	} else {
		tempValue = strtol(buf.data(), &endptr, 10);
	}
	
	if (*endptr != '\0') {
		return FALSE;
	}

	Set(tempValue);
	return TRUE;
}

void IntProperty::Clear() 
{ 
	bool changed = (mConfigured == TRUE);
	mConfigured = FALSE;
	if (changed && mpListener) mpListener->OnPropertyChange(this);
}

void IntProperty::Reset()
{
	mValue = mDefaultValue;
	mConfigured = mDefaultConfigured;
	SetClean();
}

bool IntProperty::GetInto(wxString& rDest) 
{ 
	if (!mConfigured) return FALSE;
	rDest.Printf(wxT("%d"), mValue);
	return TRUE;
}

bool IntProperty::GetInto(std::string& rDest) 
{ 
	if (!mConfigured) return FALSE;
	wxString formatted;
	formatted.Printf(wxT("%d"), mValue);
	wxCharBuffer buf = formatted.mb_str(wxConvLibc);
	rDest = buf.data();
	return TRUE;
}

bool IntProperty::GetInto(int& dest) 
{ 
	if (mConfigured) 
		dest = mValue; 
	return mConfigured; 
}

void IntProperty::SetClean() 
{ 
	mWasConfigured = mConfigured; 
	mOriginalValue = mValue; 
}

bool IntProperty::IsClean()  
{
	if (mConfigured != mWasConfigured) return FALSE;
	if (!mConfigured) return TRUE;
	return (mOriginalValue == mValue);
}

StringProperty::StringProperty(
	const char * pKeyName, 
	const std::string& value,
	PropertyChangeListener* pListener) 
: Property(pKeyName, pListener) 
{
	this->mValue = value;

	if (value.length() > 0)
		this->mConfigured = TRUE;
	else
		this->mConfigured = FALSE;

	mDefaultValue = mValue;
	mDefaultConfigured = mConfigured;

	SetClean();
}

StringProperty::StringProperty(
	const char * pKeyName, 
	const Configuration* pConf,
	PropertyChangeListener* pListener) 
: Property(pKeyName, pListener) 
{
	SetFrom(pConf);

	mDefaultValue = mValue;
	mDefaultConfigured = mConfigured;
}

StringProperty::StringProperty(
	const char * pKeyName, PropertyChangeListener* pListener) 
: Property(pKeyName, pListener) 
{
	this->mConfigured = FALSE;

	SetClean();

	mDefaultValue = mValue;
	mDefaultConfigured = mConfigured;
}

void StringProperty::SetFrom(const Configuration* pConf)	
{
	if (pConf->KeyExists(mKeyName.c_str())) {
		this->mValue = pConf->GetKeyValue(mKeyName.c_str());
		this->mConfigured = TRUE;
	} else {
		this->mConfigured = FALSE;
	}
	SetClean();
}

const std::string* StringProperty::Get() 
{ 
	return mConfigured ? &mValue : NULL; 
}

void StringProperty::Set(const wxString& rNewValue) 
{
	wxString oldValueString(mValue.c_str(), wxConvLibc);
	bool changed = (rNewValue != oldValueString || mConfigured == FALSE);
	wxCharBuffer buf = rNewValue.mb_str(wxConvLibc);
	mValue = buf.data(); 
	mConfigured = TRUE; 
	if (changed && mpListener) mpListener->OnPropertyChange(this);
}

void StringProperty::Set(const std::string& rNewValue) 
{ 
	bool changed = (rNewValue != mValue || mConfigured == FALSE);
	mValue = rNewValue; 
	mConfigured = TRUE; 
	if (changed && mpListener) mpListener->OnPropertyChange(this);
}

void StringProperty::Set(const char * pNewValue) 
{	
	bool changed = (pNewValue != mValue || mConfigured == FALSE);
	mValue = pNewValue; 
	mConfigured = TRUE; 
	if (changed && mpListener) mpListener->OnPropertyChange(this);
}

void StringProperty::Clear() 
{ 
	bool changed = (mConfigured == TRUE);
	mConfigured = FALSE;
	if (changed && mpListener) mpListener->OnPropertyChange(this);
}

void StringProperty::Reset()
{
	mValue = mDefaultValue;
	mConfigured = mDefaultConfigured;
	SetClean();
}

bool StringProperty::GetInto(std::string& rDest) 
{
	if (mConfigured) 
		rDest = mValue; 
	return mConfigured;
}

bool StringProperty::GetInto(wxString& rDest) 
{
	if (mConfigured) 
		rDest = wxString(mValue.c_str(), wxConvLibc); 
	return mConfigured;
}

void StringProperty::SetClean() 
{ 
	mWasConfigured = mConfigured; 
	mOriginalValue = mValue; 
}

bool StringProperty::IsClean()  
{ 
	if (mConfigured != mWasConfigured) return FALSE;
	if (!mConfigured) return TRUE;
	return (mOriginalValue == mValue);
}
