/*
Copyright (C) 2006 StrmnNrmn

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

#include "stdafx.h"
#include "N64RegisterCachePS2.h"

CN64RegisterCachePS2::CN64RegisterCachePS2()
{
	Reset();
}


void	CN64RegisterCachePS2::Reset()
{
	for( u32 lo_hi_idx = 0; lo_hi_idx < 2; ++lo_hi_idx )
	{
		for( u32 i = 0; i < NUM_N64_REGS; ++i )
		{
			mRegisterCacheInfo[ i ][ lo_hi_idx ].Ps2Register = Ps2Reg_R0;
			mRegisterCacheInfo[ i ][ lo_hi_idx ].Valid = false;
			mRegisterCacheInfo[ i ][ lo_hi_idx ].Dirty = false;
			mRegisterCacheInfo[ i ][ lo_hi_idx ].Known = false;
		}
	}

	for( u32 i = 0; i < NUM_N64_FP_REGS; ++i )
	{
		mFPRegisterCacheInfo[ i ].Valid = false;
		mFPRegisterCacheInfo[ i ].Dirty = false;
		mFPRegisterCacheInfo[ i ].Sim = false;
	}
}


//

void	CN64RegisterCachePS2::ClearCachedReg( EN64Reg n64_reg, u32 lo_hi_idx )
{
	#ifdef DAEDALUS_ENABLE_ASSERTS
	DAEDALUS_ASSERT( IsCached( n64_reg, lo_hi_idx ), "This register is not currently cached" );
	DAEDALUS_ASSERT( !IsDirty( n64_reg, lo_hi_idx ), "This register is being cleared while still dirty" );
#endif
	mRegisterCacheInfo[ n64_reg ][ lo_hi_idx ].Ps2Register = Ps2Reg_R0;
	mRegisterCacheInfo[ n64_reg ][ lo_hi_idx ].Valid = false;
	mRegisterCacheInfo[ n64_reg ][ lo_hi_idx ].Dirty = false;
	mRegisterCacheInfo[ n64_reg ][ lo_hi_idx ].Known = false;
}
