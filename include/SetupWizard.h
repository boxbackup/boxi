/***************************************************************************
 *            SetupWizardPanel.h
 *
 *  Sun Dec  4 20:39:00 2005
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
 
#ifndef _SETUPWIZARDPANEL_H
#define _SETUPWIZARDPANEL_H

#include <wx/wx.h>
#include <wx/wizard.h>

#include "ClientConfig.h"

class SetupWizard : public wxWizard 
{
	public:
	SetupWizard(ClientConfig *config,
		wxWindow* parent, wxWindowID id = -1);
	
	private:
	ClientConfig* mpConfig;
};

#endif /* _SETUPWIZARDPANEL_H */
