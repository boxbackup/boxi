// distribution boxbackup-0.09
// 
//  
// Copyright (c) 2003, 2004
//      Ben Summers.  All rights reserved.
//  
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
// 3. All use of this software and associated advertising materials must 
//    display the following acknowledgement:
//        This product includes software developed by Ben Summers.
// 4. The names of the Authors may not be used to endorse or promote
//    products derived from this software without specific prior written
//    permission.
// 
// [Where legally impermissible the Authors do not disclaim liability for 
// direct physical injury or death caused solely by defects in the software 
// unless it is modified by a third party.]
// 
// THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT,
// INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
// ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//  
//  
//  
// --------------------------------------------------------------------------
//
// File
//		Name:    Configuration.cpp
//		Purpose: Reading configuration files
//		Created: 2003/07/23
//
// --------------------------------------------------------------------------

#include "Box.h"

#include <stdlib.h>
#include <limits.h>

#include "Configuration.h"
#include "CommonException.h"
#include "Guards.h"
#include "FdGetLine.h"

#include "MemLeakFindOn.h"

// utility whitespace function
inline bool iw(int c)
{
	return (c == ' ' || c == '\t' || c == '\v' || c == '\f'); // \r, \n are already excluded
}

// boolean values
static const char *sValueBooleanStrings[] = {"yes", "true", "no", "false", 0};
static const bool sValueBooleanValue[] = {true, true, false, false};



// --------------------------------------------------------------------------
//
// Function
//		Name:    Configuration::Configuration(const std::string &)
//		Purpose: Constructor
//		Created: 2003/07/23
//
// --------------------------------------------------------------------------
Configuration::Configuration(const std::string &rName)
	: mName(rName)
{
}


// --------------------------------------------------------------------------
//
// Function
//		Name:    Configuration::Configuration(const Configuration &)
//		Purpose: Copy constructor
//		Created: 2003/07/23
//
// --------------------------------------------------------------------------
Configuration::Configuration(const Configuration &rToCopy)
	: mName(rToCopy.mName),
	  mSubConfigurations(rToCopy.mSubConfigurations),
	  mKeys(rToCopy.mKeys)
{
}


// --------------------------------------------------------------------------
//
// Function
//		Name:    Configuration::~Configuration()
//		Purpose: Destructor
//		Created: 2003/07/23
//
// --------------------------------------------------------------------------
Configuration::~Configuration()
{
}


// --------------------------------------------------------------------------
//
// Function
//		Name:    Configuration::LoadAndVerify(const std::string &, const ConfigurationVerify *, std::string &)
//		Purpose: Loads a configuration file from disc, checks it. Returns NULL if it was faulting, in which
//				 case they'll be an error message.
//		Created: 2003/07/23
//
// --------------------------------------------------------------------------
std::auto_ptr<Configuration> Configuration::LoadAndVerify(const char *Filename, const ConfigurationVerify *pVerify, std::string &rErrorMsg)
{
	// Check arguments
	if(Filename == 0)
	{
		THROW_EXCEPTION(CommonException, BadArguments)
	}
	
	// Just to make sure
	rErrorMsg.erase();
	
	// Open the file
	FileHandleGuard<O_RDONLY> file(Filename);
	
	// GetLine object
	FdGetLine getline(file);
	
	// Object to create
	Configuration *pconfig = new Configuration(std::string("<root>"));
	
	try
	{
		// Load
		LoadInto(*pconfig, getline, rErrorMsg, true);

		if(!rErrorMsg.empty())
		{
			// An error occured, return now
			//TRACE1("Error message from LoadInto: %s", rErrorMsg.c_str());
			TRACE0("Error at Configuration::LoadInfo\n");
			delete pconfig;
			pconfig = 0;
			return std::auto_ptr<Configuration>(0);
		}

		// Verify?
		if(pVerify)
		{
			if(!Verify(*pconfig, *pVerify, std::string(), rErrorMsg))
			{
				//TRACE1("Error message from Verify: %s", rErrorMsg.c_str());
				TRACE0("Error at Configuration::Verify\n");
				delete pconfig;
				pconfig = 0;
				return std::auto_ptr<Configuration>(0);
			}
		}
	}
	catch(...)
	{
		// Clean up
		delete pconfig;
		pconfig = 0;
		throw;
	}
	
	// Success. Return result.
	return std::auto_ptr<Configuration>(pconfig);
}


// --------------------------------------------------------------------------
//
// Function
//		Name:    LoadInto(Configuration &, FdGetLine &, std::string &, bool)
//		Purpose: Private. Load configuration information from the file into the config object.
//				 Returns 'abort' flag, if error, will be appended to rErrorMsg.
//		Created: 2003/07/24
//
// --------------------------------------------------------------------------
bool Configuration::LoadInto(Configuration &rConfig, FdGetLine &rGetLine, std::string &rErrorMsg, bool RootLevel)
{
	bool startBlockExpected = false;
	std::string blockName;

	//TRACE1("BLOCK: |%s|\n", rConfig.mName.c_str());

	while(!rGetLine.IsEOF())
	{
		std::string line(rGetLine.GetLine(true));	/* preprocess out whitespace and comments */
		
		if(line.empty())
		{
			// Ignore blank lines
			continue;
		}
		
		// Line an open block string?
		if(line == "{")
		{
			if(startBlockExpected)
			{
				// New config object
				Configuration config(blockName);
				
				// Continue processing into this block
				if(!LoadInto(config, rGetLine, rErrorMsg, false))
				{
					// Abort error
					return false;
				}

				startBlockExpected = false;

				// Store...
				rConfig.mSubConfigurations.push_back(std::pair<std::string, Configuration>(blockName, config));
			}
			else
			{
				rErrorMsg += "Unexpected start block in " + rConfig.mName + "\n";
			}
		}
		else
		{
			// Close block?
			if(line == "}")
			{
				if(RootLevel)
				{
					// error -- root level doesn't have a close
					rErrorMsg += "Root level has close block -- forget to terminate subblock?\n";
					// but otherwise ignore
				}
				else
				{
					//TRACE0("ENDBLOCK\n");
					return true;		// All very good and nice
				}
			}
			// Either a key, or a sub block beginning
			else
			{
				// Can't be a start block
				if(startBlockExpected)
				{
					rErrorMsg += "Block " + blockName + " wasn't started correctly (no '{' on line of it's own)\n";
					startBlockExpected = false;
				}

				// Has the line got an = in it?
				unsigned int equals = 0;
				for(; equals < line.size(); ++equals)
				{
					if(line[equals] == '=')
					{
						// found!
						break;
					}
				}
				if(equals < line.size())
				{
					// Make key value pair
					unsigned int keyend = equals;
					while(keyend > 0 && iw(line[keyend-1]))
					{
						keyend--;
					}
					unsigned int valuestart = equals+1;
					while(valuestart < line.size() && iw(line[valuestart]))
					{
						valuestart++;
					}
					if(keyend > 0 && valuestart < line.size())
					{
						std::string key(line.substr(0, keyend));
						std::string value(line.substr(valuestart));
						//TRACE2("KEY: |%s|=|%s|\n", key.c_str(), value.c_str());

						// Check for duplicate values
						if(rConfig.mKeys.find(key) != rConfig.mKeys.end())
						{
							// Multi-values allowed here, but checked later on
							rConfig.mKeys[key] += MultiValueSeparator;
							rConfig.mKeys[key] += value;
						}
						else
						{
							// Store
							rConfig.mKeys[key] = value;
						}
					}
					else
					{
						rErrorMsg += "Invalid key in block "+rConfig.mName+"\n";
					}
				}
				else
				{
					// Start of sub block
					blockName = line;
					startBlockExpected = true;
				}
			}
		}	
	}

	// End of file?
	if(!RootLevel && rGetLine.IsEOF())
	{
		// Error if EOF and this isn't the root level
		rErrorMsg += "File ended without terminating all subblocks\n";
	}
	
	return true;
}


// --------------------------------------------------------------------------
//
// Function
//		Name:    Configuration::KeyExists(const char *)
//		Purpose: Checks to see if a key exists
//		Created: 2003/07/23
//
// --------------------------------------------------------------------------
bool Configuration::KeyExists(const char *pKeyName) const
{
	if(pKeyName == 0) {THROW_EXCEPTION(CommonException, BadArguments)}

	return mKeys.find(pKeyName) != mKeys.end();
}


// --------------------------------------------------------------------------
//
// Function
//		Name:    Configuration::GetKeyValue(const char *)
//		Purpose: Returns the value of a configuration variable
//		Created: 2003/07/23
//
// --------------------------------------------------------------------------
const std::string &Configuration::GetKeyValue(const char *pKeyName) const
{
	if(pKeyName == 0) {THROW_EXCEPTION(CommonException, BadArguments)}

	std::map<std::string, std::string>::const_iterator i(mKeys.find(pKeyName));
	
	if(i == mKeys.end())
	{
		THROW_EXCEPTION(CommonException, ConfigNoKey)
	}
	else
	{
		return i->second;
	}
}


// --------------------------------------------------------------------------
//
// Function
//		Name:    Configuration::GetKeyValueInt(const char *)
//		Purpose: Gets a key value as an integer
//		Created: 2003/07/23
//
// --------------------------------------------------------------------------
int Configuration::GetKeyValueInt(const char *pKeyName) const
{
	if(pKeyName == 0) {THROW_EXCEPTION(CommonException, BadArguments)}

	std::map<std::string, std::string>::const_iterator i(mKeys.find(pKeyName));
	
	if(i == mKeys.end())
	{
		THROW_EXCEPTION(CommonException, ConfigNoKey)
	}
	else
	{
		long value = ::strtol((i->second).c_str(), NULL, 0 /* C style handling */);
		if(value == LONG_MAX || value == LONG_MIN)
		{
			THROW_EXCEPTION(CommonException, ConfigBadIntValue)
		}
		return (int)value;
	}
}


// --------------------------------------------------------------------------
//
// Function
//		Name:    Configuration::GetKeyValueBool(const char *) const
//		Purpose: Gets a key value as a boolean
//		Created: 17/2/04
//
// --------------------------------------------------------------------------
bool Configuration::GetKeyValueBool(const char *pKeyName) const
{
	if(pKeyName == 0) {THROW_EXCEPTION(CommonException, BadArguments)}

	std::map<std::string, std::string>::const_iterator i(mKeys.find(pKeyName));
	
	if(i == mKeys.end())
	{
		THROW_EXCEPTION(CommonException, ConfigNoKey)
	}
	else
	{
		bool value = false;
		
		// Anything this is called for should have been verified as having a correct
		// string in the verification section. However, this does default to false
		// if it isn't in the string table.
		
		for(int l = 0; sValueBooleanStrings[l] != 0; ++l)
		{
			if(::strcasecmp((i->second).c_str(), sValueBooleanStrings[l]) == 0)
			{
				// Found.
				value = sValueBooleanValue[l];
				break;
			}
		}
		
		return value;
	}

}



// --------------------------------------------------------------------------
//
// Function
//		Name:    Configuration::GetKeyNames()
//		Purpose: Returns list of key names
//		Created: 2003/07/24
//
// --------------------------------------------------------------------------
std::vector<std::string> Configuration::GetKeyNames() const
{
	std::map<std::string, std::string>::const_iterator i(mKeys.begin());
	
	std::vector<std::string> r;
	
	for(; i != mKeys.end(); ++i)
	{
		r.push_back(i->first);
	}
	
	return r;
}


// --------------------------------------------------------------------------
//
// Function
//		Name:    Configuration::SubConfigurationExists(const char *)
//		Purpose: Checks to see if a sub configuration exists
//		Created: 2003/07/23
//
// --------------------------------------------------------------------------
bool Configuration::SubConfigurationExists(const char *pSubName) const
{
	if(pSubName == 0) {THROW_EXCEPTION(CommonException, BadArguments)}

	// Attempt to find it...
	std::list<std::pair<std::string, Configuration> >::const_iterator i(mSubConfigurations.begin());
	
	for(; i != mSubConfigurations.end(); ++i)
	{
		// This the one?
		if(i->first == pSubName)
		{
			// Yes.
			return true;
		}
	}

	// didn't find it.
	return false;
}


// --------------------------------------------------------------------------
//
// Function
//		Name:    Configuration::GetSubConfiguration(const char *)
//		Purpose: Gets a sub configuration
//		Created: 2003/07/23
//
// --------------------------------------------------------------------------
const Configuration &Configuration::GetSubConfiguration(const char *pSubName) const
{
	if(pSubName == 0) {THROW_EXCEPTION(CommonException, BadArguments)}

	// Attempt to find it...
	std::list<std::pair<std::string, Configuration> >::const_iterator i(mSubConfigurations.begin());
	
	for(; i != mSubConfigurations.end(); ++i)
	{
		// This the one?
		if(i->first == pSubName)
		{
			// Yes.
			return i->second;
		}
	}

	THROW_EXCEPTION(CommonException, ConfigNoSubConfig)
}


// --------------------------------------------------------------------------
//
// Function
//		Name:    Configuration::GetSubConfigurationNames()
//		Purpose: Return list of sub configuration names
//		Created: 2003/07/24
//
// --------------------------------------------------------------------------
std::vector<std::string> Configuration::GetSubConfigurationNames() const
{
	std::list<std::pair<std::string, Configuration> >::const_iterator i(mSubConfigurations.begin());
	
	std::vector<std::string> r;
	
	for(; i != mSubConfigurations.end(); ++i)
	{
		r.push_back(i->first);
	}
	
	return r;
}


// --------------------------------------------------------------------------
//
// Function
//		Name:    Configuration::Verify(const Configuration &, const ConfigurationVerify &, const std::string &, std::string &)
//		Purpose: Return list of sub configuration names
//		Created: 2003/07/24
//
// --------------------------------------------------------------------------
bool Configuration::Verify(Configuration &rConfig, const ConfigurationVerify &rVerify, const std::string &rLevel, std::string &rErrorMsg)
{
	bool ok = true;

	// First... check the keys
	if(rVerify.mpKeys != 0)
	{
		const ConfigurationVerifyKey *pvkey = rVerify.mpKeys;
		
		bool todo = true;
		do
		{
			// Can the key be found?
			ASSERT(pvkey->mpName);
			if(rConfig.KeyExists(pvkey->mpName))
			{
				// Get value
				const std::string &rval = rConfig.GetKeyValue(pvkey->mpName);
				const char *val = rval.c_str();

				// Check it's a number?
				if((pvkey->Tests & ConfigTest_IsInt) == ConfigTest_IsInt)
				{					
					// Test it...
					char *end;
					long r = ::strtol(val, &end, 0);
					if(r == LONG_MIN || r == LONG_MAX || end != (val + rval.size()))
					{
						// not a good value
						ok = false;
						rErrorMsg += rLevel + rConfig.mName +"." + pvkey->mpName + " (key) is not a valid integer.\n";
					}
				}
				
				// Check it's a bool?
				if((pvkey->Tests & ConfigTest_IsBool) == ConfigTest_IsBool)
				{				
					// See if it's one of the allowed strings.
					bool found = false;
					for(int l = 0; sValueBooleanStrings[l] != 0; ++l)
					{
						if(::strcasecmp(val, sValueBooleanStrings[l]) == 0)
						{
							// Found.
							found = true;
							break;
						}
					}
					
					// Error if it's not one of them.
					if(!found)
					{
						ok = false;
						rErrorMsg += rLevel + rConfig.mName +"." + pvkey->mpName + " (key) is not a valid boolean value.\n";
					}
				}
				
				// Check for multi valued statments where they're not allowed
				if((pvkey->Tests & ConfigTest_MultiValueAllowed) == 0)
				{
					// Check to see if this key is a multi-value -- it shouldn't be
					if(rval.find(MultiValueSeparator) != rval.npos)
					{
						ok = false;
						rErrorMsg += rLevel + rConfig.mName +"." + pvkey->mpName + " (key) multi value not allowed (duplicated key?).\n";
					}
				}				
			}
			else
			{
				// Is it required to exist?
				if((pvkey->Tests & ConfigTest_Exists) == ConfigTest_Exists)
				{
					// Should exist, but doesn't.
					ok = false;
					rErrorMsg += rLevel + rConfig.mName + "." + pvkey->mpName + " (key) is missing.\n";
				}
				else if(pvkey->mpDefaultValue)
				{
					rConfig.mKeys[std::string(pvkey->mpName)] = std::string(pvkey->mpDefaultValue);
				}
			}
		
			if((pvkey->Tests & ConfigTest_LastEntry) == ConfigTest_LastEntry)
			{
				// No more!
				todo = false;
			}
			
			// next
			pvkey++;
			
		} while(todo);

		// Check for additional keys
		for(std::map<std::string, std::string>::const_iterator i = rConfig.mKeys.begin();
			i != rConfig.mKeys.end(); ++i)
		{
			// Is the name in the list?
			const ConfigurationVerifyKey *scan = rVerify.mpKeys;
			bool found = false;
			while(scan)
			{
				if(scan->mpName == i->first)
				{
					found = true;
					break;
				}
				
				// Next?
				if((scan->Tests & ConfigTest_LastEntry) == ConfigTest_LastEntry)
				{
					break;
				}
				scan++;
			}
			
			if(!found)
			{
				// Shouldn't exist, but does.
				ok = false;
				rErrorMsg += rLevel + rConfig.mName + "." + i->first + " (key) is not a known key. Check spelling and placement.\n";
			}
		}
	}
	
	// Then the sub configurations
	if(rVerify.mpSubConfigurations)
	{
		// Find the wildcard entry, if it exists, and check that required subconfigs are there
		const ConfigurationVerify *wildcardverify = 0;
	
		const ConfigurationVerify *scan = rVerify.mpSubConfigurations;
		while(scan)
		{
			ASSERT(scan->mpName);
			if(scan->mpName[0] == '*')
			{
				wildcardverify = scan;
			}
			
			// Required?
			if((scan->Tests & ConfigTest_Exists) == ConfigTest_Exists)
			{
				if(scan->mpName[0] == '*')
				{
					// Check something exists
					if(rConfig.mSubConfigurations.size() < 1)
					{
						// A sub config should exist, but doesn't.
						ok = false;
						rErrorMsg += rLevel + rConfig.mName + ".* (block) is missing (a block must be present).\n";
					}
				}
				else
				{
					// Check real thing exists
					if(!rConfig.SubConfigurationExists(scan->mpName))
					{
						// Should exist, but doesn't.
						ok = false;
						rErrorMsg += rLevel + rConfig.mName + "." + scan->mpName + " (block) is missing.\n";
					}
				}
			}

			// Next?
			if((scan->Tests & ConfigTest_LastEntry) == ConfigTest_LastEntry)
			{
				break;
			}
			scan++;
		}
		
		// Go through the sub configurations, one by one
		for(std::list<std::pair<std::string, Configuration> >::const_iterator i(rConfig.mSubConfigurations.begin());
			i != rConfig.mSubConfigurations.end(); ++i)
		{
			// Can this be found?
			const ConfigurationVerify *subverify = 0;
			
			const ConfigurationVerify *scan = rVerify.mpSubConfigurations;
			const char *name = i->first.c_str();
			ASSERT(name);
			while(scan)
			{
				if(strcmp(scan->mpName, name) == 0)
				{
					// found it!
					subverify = scan;
				}
			
				// Next?
				if((scan->Tests & ConfigTest_LastEntry) == ConfigTest_LastEntry)
				{
					break;
				}
				scan++;
			}
			
			// Use wildcard?
			if(subverify == 0)
			{
				subverify = wildcardverify;
			}
			
			// Verify
			if(subverify)
			{
				// override const-ness here...
				if(!Verify((Configuration&)i->second, *subverify, rConfig.mName + '.', rErrorMsg))
				{
					ok = false;
				}
			}
		}
	}
	
	return ok;
}


