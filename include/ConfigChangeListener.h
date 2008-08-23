/***************************************************************************
 *            ConfigChangeListener.h
 *
 *  Created 2008-08-23
 *  Copyright 2008 Chris Wilson
 *  Email chris-boxisource@qwirx.com
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
 
#ifndef _CONFIGCHANGELISTENER_H
#define _CONFIGCHANGELISTENER_H

class ConfigChangeListener 
{
	public:
	virtual ~ConfigChangeListener() { }
	virtual void NotifyChange() { }
};

#endif /* _CONFIGCHANGELISTENER_H */

