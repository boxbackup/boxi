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
	SetClean();
}
	
BoolProperty::BoolProperty(
	const char * pKeyName, const Configuration* pConf,
	PropertyChangeListener* pListener) 
: Property(pKeyName, pListener) 
{
	SetFrom(pConf);
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
	this->mValue = value;
	this->mConfigured = TRUE;
	SetClean();
}

IntProperty::IntProperty(
	const char * pKeyName, 
	const Configuration* pConf,
	PropertyChangeListener* pListener) 
: Property(pKeyName, pListener)
{
	SetFrom(pConf);
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

void IntProperty::Clear() 
{ 
	bool changed = (mConfigured == TRUE);
	mConfigured = FALSE;
	if (changed && mpListener) mpListener->OnPropertyChange(this);
}

bool IntProperty::GetInto(std::string& dest) 
{ 
	if (!mConfigured) return FALSE;
	wxString formatted;
	formatted.Printf(wxT("%d"), mValue);
	wxCharBuffer buf = formatted.mb_str(wxConvLibc);
	dest = buf.data();
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
	SetClean();
}

StringProperty::StringProperty(
	const char * pKeyName, 
	const Configuration* pConf,
	PropertyChangeListener* pListener) 
: Property(pKeyName, pListener) 
{
	SetFrom(pConf);
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

void StringProperty::Set(const std::string& newValue) 
{ 
	bool changed = (newValue != mValue || mConfigured == FALSE);
	mValue = newValue; 
	mConfigured = TRUE; 
	if (changed && mpListener) mpListener->OnPropertyChange(this);
}

void StringProperty::Set(const char * newValue) 
{	
	bool changed = (newValue != mValue || mConfigured == FALSE);
	mValue = newValue; 
	mConfigured = TRUE; 
	if (changed && mpListener) mpListener->OnPropertyChange(this);
}

void StringProperty::Clear() 
{ 
	bool changed = (mConfigured == TRUE);
	mConfigured = FALSE;
	if (changed && mpListener) mpListener->OnPropertyChange(this);
}

bool StringProperty::GetInto(std::string& dest) 
{
	if (mConfigured) 
		dest = mValue; 
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
