/***************************************************************************
 *            SandBox.h
 *  Safe way to include Box.h without redefining PACKAGE_NAME, etc.
 *  Mon Apr  4 20:35:39 2005
 *  Copyright  2005  Chris Wilson
 *  Email <boxi_BackupProgressPanel.h@qwirx.com>
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
 
#ifndef _SAND_BOX_H
#define _SAND_BOX_H

#ifdef BOXPLATFORM__H
	// error reports from gcc help to figure out how
	// BoxPlatform.h was included before
	#define BOXPLATFORM__H foo
	#error You must include SandBox.h BEFORE including Box.h or BoxPlatform.h
#endif

#include <wx/wx.h>
#include <wx/thread.h>

// Box Backup's BoxConfig.h tries to redefine PACKAGE_NAME, etc.
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_BUGREPORT
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION

#define NDEBUG
#include "Box.h"
#undef NDEBUG

// now get our old #defines back
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_BUGREPORT
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION

#ifndef BOXPLATFORM__H
	#error BoxPlatform.h did not define BOXPLATFORM__H, fix this file!
#endif

#endif // _SAND_BOX_H