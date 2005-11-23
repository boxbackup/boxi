/***************************************************************************
 *            StaticImage.h
 *
 *  Sat Apr  2 16:46:42 2005
 *  Copyright  2005  Chris Wilson
 *  Email <boxi_StaticImage.h@qwirx.com>
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
 
#ifndef _STATICIMAGE_H
#define _STATICIMAGE_H

#ifdef __cplusplus
extern "C"
{
#endif

struct StaticImage {
	const int size;
	const unsigned char * data;
};

extern const struct StaticImage tick16_png, tickgrey16_png, 
	cross16_png, crossgrey16_png, plus16_png, plusgrey16_png, equal16_png, 
	notequal16_png, hourglass16_png, sametime16_png, unknown16_png,
	oldfile16_png, exclam16_png, folder16_png, partial16_png, alien16_png,
	tick16a_png, cross16a_png, empty16_png, partialtick16_png;

#ifdef __cplusplus
}
#endif

#endif /* _STATICIMAGE_H */
