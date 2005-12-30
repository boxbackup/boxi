/***************************************************************************
 *            SslConfig.h
 *
 *  Wed Dec 28 01:29:03 2005
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
 
#ifndef _SSLCONFIG_H
#define _SSLCONFIG_H

#include <openssl/conf.h>
#include <openssl/x509.h>

class SslConfig
{
	private:
	CONF* mpConfig;
	wxString mConfigFileName;

	SslConfig(CONF* pConfig, const wxString& name) 
	: mpConfig(pConfig), mConfigFileName(name) { }
	
	public:
	~SslConfig() { NCONF_free(mpConfig); }
	CONF* GetConf() { return mpConfig; }
	const wxString& GetFileName() { return mConfigFileName; }
	static std::auto_ptr<SslConfig> Get(wxString* pMsgOut);
};

bool GetCommonName(X509_NAME* pX509SubjectName, wxString* pTarget, 
	wxString* pMsgOut);
EVP_PKEY* LoadKey(const wxString& rKeyFileName, wxString* pMsgOut);
void FreeKey(EVP_PKEY* pKey);

#endif /* _SSLCONFIG_H */
