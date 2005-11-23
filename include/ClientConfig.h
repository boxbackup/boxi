/***************************************************************************
 *            ClientConfig.h
 *
 *  Mon Feb 28 22:52:06 2005
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
 
#ifndef _CLIENTCONFIG_H
#define _CLIENTCONFIG_H

#include "main.h"
#include "Property.h"
#include "Location.h"

class ClientConfig : public PropertyChangeListener {
	private:
	ClientConfig(const ClientConfig& forbidden);

	public:
	ClientConfig();
	ClientConfig(const wxString& configFileName);
	virtual ~ClientConfig() { }
	
	const wxString& GetConfigFileName() { return mConfigFileName; }
	
	#define STR_PROP(name) StringProperty name;
	#define STR_PROP_SUBCONF(name, conf) StringProperty name;
	#define INT_PROP(name) IntProperty name;
	#define BOOL_PROP(name) BoolProperty name;
	ALL_PROPS
	#undef BOOL_PROP
	#undef INT_PROP
	#undef STR_PROP_SUBCONF
	#undef STR_PROP

	void SetClean();
	bool IsClean();

	void Load(const wxString& rConfigFileName);
	bool Save();
	bool Save(const wxString& rConfigFileName);
	
	const std::vector<Location*>& GetLocations() { return mLocations; }
	void AddLocation    (Location* newEntry);
	void ReplaceLocation(int index, Location* newValues);
	void RemoveLocation (int index);
	void RemoveLocation (Location* oldLocation);
	
	void AddListener   (ConfigChangeListener* newListener);
	void RemoveListener(ConfigChangeListener* oldListener);
	void NotifyListeners();
	
	// implement PropertyChangeListener
	void OnPropertyChange(Property* pProp);
	
	bool Check(wxString& rMsg);
	
	private:
	wxString mConfigFileName;
	std::auto_ptr<Configuration> mapConfig;
	std::vector<Location*> mLocations;
	std::vector<ConfigChangeListener*> mListeners;
	
	wxString ConvertCygwinPathToWindows(const char *cygPath);
	wxString ConvertWindowsPathToCygwin(const char *winPath);
};

#endif /* _CLIENTCONFIG_H */
