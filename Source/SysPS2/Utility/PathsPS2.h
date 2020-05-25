/*
Copyright (C) 2008 StrmnNrmn

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/



#ifndef SYSPS2_UTILITY_PATHSPS2_H_
#define SYSPS2_UTILITY_PATHSPS2_H_

/*#ifdef DAEDALUS_SILENT
#define DAEDALUS_PS2_PATH(p)				p
#else
#ifdef DAEDALUS_PS2_ALT
#define DAEDALUS_PS2_PATH(p)				"host0:/" p
#else
#define DAEDALUS_PS2_PATH(p)				p
#endif
#endif*/

extern char* PathsPS2(char* p);

//#define DAEDALUS_PS2_PATH(p)				"host:" p
#define DAEDALUS_PS2_PATH(p)				PathsPS2(p)

#endif // SYSPS2_UTILITY_PATHSPS2_H_
