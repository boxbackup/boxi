/***************************************************************************
 *            ClientConfig.h
 *
 *  Mon Feb 28 22:52:06 2005
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
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
 
#ifndef _CLIENTCONFIG_H
#define _CLIENTCONFIG_H

#ifdef HAVE_PCREPOSIX_H
	#include <pcreposix.h>
#else
	#include <regex.h> // needed by ExcludeList
#endif

#include "Configuration.h"

#include "main.h"
#include "ConfigChangeListener.h"
#include "Location.h"
#include "Property.h"

class ClientConfig : 
	public PropertyChangeListener, 
	public LocationChangeListener 
{
	private:
	ClientConfig(const ClientConfig& forbidden);
	ClientConfig operator=(const ClientConfig& forbidden);

	public:
	ClientConfig();
	ClientConfig(const wxString& configFileName);
	virtual ~ClientConfig() { }
	
	const wxString& GetFileName() { return mConfigFileName; }
	void SetFileName(const wxString& rFilename) 
	{ mConfigFileName = rFilename; }

	// unsaved property
	StringProperty CertRequestFile;
	
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
	void Load(const Configuration& rBoxConfig);
	bool Save();
	bool Save(const wxString& rConfigFileName);
	void Reset();
	Configuration GetBoxConfig();

	const BoxiLocation::List& GetLocations() { return mLocations; }
	void AddLocation    (const BoxiLocation& rNewLocation);
	void ReplaceLocation(int index, const BoxiLocation& rNewLocation);
	void RemoveLocation (int index);
	void RemoveLocation (const BoxiLocation& rOldLocation);
	BoxiLocation* GetLocation(const BoxiLocation& rConstLocation);
	BoxiLocation* GetLocation(const wxString& rName);
	
	void AddListener   (ConfigChangeListener* pNewListener);
	void RemoveListener(ConfigChangeListener* pOldListener);
	void NotifyListeners();
	
	// implement PropertyChangeListener
	virtual void OnPropertyChange   (Property*      pProp);
	// implement LocationChangeListener
	virtual void OnLocationChange   (BoxiLocation*  pLocation);
	virtual void OnExcludeListChange(BoxiExcludeList* pExcludeList);
	
	bool Check(wxString& rMsg);
	
	bool CheckAccountNumber  (wxString* pMsgOut);
	bool CheckStoreHostname  (wxString* pMsgOut);
	bool CheckPrivateKeyFile (wxString* pMsgOut);
	bool CheckCertificateFile(wxString* pMsgOut);
	bool CheckTrustedCAsFile (wxString* pMsgOut);
	bool CheckCryptoKeysFile (wxString* pMsgOut);
	
	private:
	wxString mConfigFileName;
	std::auto_ptr<Configuration> mapOriginalConfig;
	BoxiLocation::List mLocations;
	std::vector<ConfigChangeListener*> mListeners;
	
	static BoxiLocation::List GetConfigurationLocations
		(const Configuration& conf);
	
	void AddToConfig(StringProperty& rProperty, Configuration& rConfig,
		std::string rSubBlockName);
	void AddToConfig(StringProperty& rProperty, Configuration& rConfig);
	void AddToConfig(IntProperty& rProperty,    Configuration& rConfig);
	void AddToConfig(BoolProperty& rProperty,   Configuration& rConfig);
};

#endif /* _CLIENTCONFIG_H */
