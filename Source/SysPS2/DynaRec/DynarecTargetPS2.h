/*
Copyright (C) 2005 StrmnNrmn

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



#ifndef SYSPS2_DYNAREC_DYNARECTARGETPS2_H_
#define SYSPS2_DYNAREC_DYNARECTARGETPS2_H_

#include "Core/R4300OpCode.h"

enum EPs2Reg
{
	Ps2Reg_R0 = 0, Ps2Reg_AT, Ps2Reg_V0, Ps2Reg_V1,
	Ps2Reg_A0, Ps2Reg_A1, Ps2Reg_A2, Ps2Reg_A3,
	Ps2Reg_T0, Ps2Reg_T1, Ps2Reg_T2, Ps2Reg_T3,
	Ps2Reg_T4, Ps2Reg_T5, Ps2Reg_T6, Ps2Reg_T7,
	Ps2Reg_S0, Ps2Reg_S1, Ps2Reg_S2, Ps2Reg_S3,
	Ps2Reg_S4, Ps2Reg_S5, Ps2Reg_S6, Ps2Reg_S7,
	Ps2Reg_T8, Ps2Reg_T9, Ps2Reg_K0, Ps2Reg_K1,
	Ps2Reg_GP, Ps2Reg_SP, Ps2Reg_S8, Ps2Reg_RA,
};

enum EPs2FloatReg
{
	Ps2FloatReg_F00 = 0,Ps2FloatReg_F01, Ps2FloatReg_F02, Ps2FloatReg_F03, Ps2FloatReg_F04, Ps2FloatReg_F05, Ps2FloatReg_F06, Ps2FloatReg_F07,
	Ps2FloatReg_F08,	Ps2FloatReg_F09, Ps2FloatReg_F10, Ps2FloatReg_F11, Ps2FloatReg_F12, Ps2FloatReg_F13, Ps2FloatReg_F14, Ps2FloatReg_F15,
	Ps2FloatReg_F16,	Ps2FloatReg_F17, Ps2FloatReg_F18, Ps2FloatReg_F19, Ps2FloatReg_F20, Ps2FloatReg_F21, Ps2FloatReg_F22, Ps2FloatReg_F23,
	Ps2FloatReg_F24,	Ps2FloatReg_F25, Ps2FloatReg_F26, Ps2FloatReg_F27, Ps2FloatReg_F28, Ps2FloatReg_F29, Ps2FloatReg_F30, Ps2FloatReg_F31,

};

// Return true if this register is temporary (i.e. not saved across function calls)
inline bool	Ps2Reg_IsTemporary( EPs2Reg ps2_reg )	{ return (0xB300FFFF >> ps2_reg) & 1;}

// Return true if this register dont need sign extension //Corn
inline bool	N64Reg_DontNeedSign( EN64Reg n64_reg )	{ return (0x30000001 >> n64_reg) & 1;}

struct Ps2OpCode
{
	union
	{
		u32 _u32;

		struct
		{
			unsigned offset : 16;
			unsigned rt : 5;
			unsigned rs : 5;
			unsigned op : 6;
		};

		struct
		{
			unsigned immediate : 16;
			unsigned : 5;
			unsigned base : 5;
			unsigned : 6;
		};

		struct
		{
			unsigned target : 26;
			unsigned : 6;
		};

		// SPECIAL
		struct
		{
			unsigned spec_op : 6;
			unsigned sa : 5;
			unsigned rd : 5;
			unsigned : 5;
			unsigned : 5;
			unsigned : 6;
		};

		// REGIMM
		struct
		{
			unsigned : 16;
			unsigned regimm_op : 5;
			unsigned : 11;
		};

		// COP0 op
		struct
		{
			unsigned : 21;
			unsigned cop0_op : 5;
			unsigned : 6;
		};

		// COP1 op
		struct
		{
			unsigned cop1_funct : 6;
			unsigned fd : 5;		// sa
			unsigned fs : 5;		// rd
			unsigned ft : 5;		// rt
			unsigned cop1_op : 5;
			unsigned : 6;
		};

		struct
		{
			unsigned : 16;
			unsigned cop1_bc : 2;
			unsigned : 14;
		};
	};
};


#endif // SYSPS2_DYNAREC_DYNARECTARGETPS2_H_
