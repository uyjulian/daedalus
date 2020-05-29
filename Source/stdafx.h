/*
Copyright (C) 2001-2007 StrmnNrmn

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

#pragma once

#ifndef STDAFX_H_
#define STDAFX_H_


#ifndef MAKE_UNCACHED_PTR
#ifdef DAEDALUS_PSP
#define MAKE_UNCACHED_PTR(x)	(reinterpret_cast< void * >( reinterpret_cast<u32>( (x) ) | 0x40000000 ))
#else
#define MAKE_UNCACHED_PTR(x)	(x)
#endif
#endif

// Pure is a function attribute which says that a function does not modify any global memory.
// Const is a function attribute which says that a function does not read/modify any global memory.

// Given that information, the compiler can do some additional optimisations.

#ifndef DAEDALUS_ATTRIBUTE_PURE
#define DAEDALUS_ATTRIBUTE_PURE
#endif

#ifndef DAEDALUS_ATTRIBUTE_CONST
#define DAEDALUS_ATTRIBUTE_CONST
#endif

#if defined(DAEDALUS_CONFIG_RELEASE)
#include "Base/Release/BuildConfig.h"
#elif defined(DAEDALUS_CONFIG_PROFILE)
#include "Base/Profile/BuildConfig.h"
#elif defined(DAEDALUS_CONFIG_DEV)
#include "Base/Dev/BuildConfig.h"
#else
#error Unknown compilation mode
#endif


// Platform specifc #includes, externs, #defines etc
#ifdef DAEDALUS_W32
#include "Platform/Windows/DaedalusW32.h"
#endif

#include "Base/Types.h"

#endif // STDAFX_H_
