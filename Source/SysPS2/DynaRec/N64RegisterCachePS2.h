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



#ifndef SYSPS2_DYNAREC_N64REGISTERCACHEPS2_H_
#define SYSPS2_DYNAREC_N64REGISTERCACHEPS2_H_

#include <stdlib.h>

#include "DynarecTargetPS2.h"

//*************************************************************************************
//
//*************************************************************************************
class CN64RegisterCachePS2
{
public:
		CN64RegisterCachePS2();

		void				Reset();

		inline void	SetCachedReg( EN64Reg n64_reg, EPs2Reg ps2_reg )
		{
			mRegisterCacheInfo[ n64_reg ].Ps2Register = ps2_reg;
		}

		inline bool	IsCached( EN64Reg reg ) const
		{
			return mRegisterCacheInfo[ reg ].Ps2Register != Ps2Reg_R0;
		}

		inline bool	IsValid( EN64Reg reg ) const
		{
			#ifdef DAEDALUS_ENABLE_ASSERTS
			DAEDALUS_ASSERT( !mRegisterCacheInfo[ reg ].Valid || IsCached( reg ), "Checking register is valid but uncached?" );
			#endif
			return mRegisterCacheInfo[ reg ].Valid;
		}

		inline bool	IsDirty( EN64Reg reg ) const
		{
#ifdef DAEDALUS_ENABLE_ASSERTS
			bool	is_dirty( mRegisterCacheInfo[ reg ].Dirty );

			if( is_dirty )
			{
				DAEDALUS_ASSERT( IsKnownValue( reg ) || IsCached( reg ), "Checking dirty flag on unknown/uncached register?" );
			}

			return is_dirty;
#else
			return mRegisterCacheInfo[ reg ].Dirty;
#endif
		}

		inline bool	IsTemporary( EN64Reg reg ) const
		{
			if( IsCached( reg ) )
			{
				return Ps2Reg_IsTemporary( mRegisterCacheInfo[ reg ].Ps2Register );
			}

			return false;
		}

		inline EPs2Reg	GetCachedReg( EN64Reg reg ) const
		{
			#ifdef DAEDALUS_ENABLE_ASSERTS
			DAEDALUS_ASSERT( IsCached( reg ), "Trying to retreive an uncached register" );
			#endif
			return mRegisterCacheInfo[ reg ].Ps2Register;
		}

		inline void	MarkAsValid( EN64Reg reg, bool valid )
		{
			#ifdef DAEDALUS_ENABLE_ASSERTS
			DAEDALUS_ASSERT( IsCached( reg ), "Changing valid flag on uncached register?" );
			#endif
			mRegisterCacheInfo[ reg ].Valid = valid;
		}

		inline void	MarkAsDirty( EN64Reg reg, bool dirty )
		{
#ifdef DAEDALUS_ENABLE_ASSERTS
			if( dirty )
			{
				 DAEDALUS_ASSERT( IsKnownValue( reg ) || IsCached( reg ), "Setting dirty flag on unknown/uncached register?" );
			}
#endif
			mRegisterCacheInfo[ reg ].Dirty = dirty;
		}


		inline bool	IsKnownValue( EN64Reg reg ) const
		{
			return mRegisterCacheInfo[ reg ].Known;
		}

		inline void	SetKnownValue( EN64Reg reg, u64 value )
		{
			mRegisterCacheInfo[ reg ].Known = true;
			mRegisterCacheInfo[ reg ].KnownValue._u64 = value;
		}

		inline void	ClearKnownValue( EN64Reg reg )
		{
			mRegisterCacheInfo[ reg ].Known = false;
		}

		inline REG64	GetKnownValue( EN64Reg reg ) const
		{
			return mRegisterCacheInfo[ reg ].KnownValue;
		}

		inline bool	IsFPValid( EN64FloatReg reg ) const
		{
			return mFPRegisterCacheInfo[ reg ].Valid;
		}

		inline bool	IsFPDirty( EN64FloatReg reg ) const
		{
			return mFPRegisterCacheInfo[ reg ].Dirty;
		}

		inline bool	IsFPSim( EN64FloatReg reg ) const
		{
			return mFPRegisterCacheInfo[ reg ].Sim;
		}

		inline void	MarkFPAsValid( EN64FloatReg reg, bool valid )
		{
			mFPRegisterCacheInfo[ reg ].Valid = valid;
		}

		inline void	MarkFPAsDirty( EN64FloatReg reg, bool dirty )
		{
			mFPRegisterCacheInfo[ reg ].Dirty = dirty;
		}

		inline void	MarkFPAsSim( EN64FloatReg reg, bool Sim )
		{
			mFPRegisterCacheInfo[ reg ].Sim = Sim;
		}

		void		ClearCachedReg( EN64Reg n64_reg );

private:

		struct RegisterCacheInfoPS2
		{
			REG64			KnownValue;			// The contents (if known)
			EPs2Reg			Ps2Register;		// If cached, this is the ps2 register we're using
			bool			Valid;				// Is the contents of the register valid?
			bool			Dirty;				// Is the contents of the register modified?
			bool			Known;				// Is the contents of the known?
			//bool			SignExtended;		// Is this (high) register just sign extension of low reg?
		};

		// PSP fp registers are stored in a 1:1 mapping with the n64 counterparts
		struct FPRegisterCacheInfoPS2
		{
			bool			Valid;
			bool			Dirty;
			bool			Sim;
		};

		RegisterCacheInfoPS2	mRegisterCacheInfo[ NUM_N64_REGS ];
		FPRegisterCacheInfoPS2	mFPRegisterCacheInfo[ NUM_N64_FP_REGS ];
};

#endif // SYSPS2_DYNAREC_N64REGISTERCACHEPS2_H_
