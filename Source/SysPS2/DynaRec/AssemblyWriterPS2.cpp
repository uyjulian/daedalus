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

#include "stdafx.h"
#include "AssemblyWriterPS2.h"

#include "Core/Registers.h"

#include "OSHLE/ultra_R4300.h"

#include <limits.h>


//	Get the opcodes for loading a 32 bit constant into the specified
//	register. If this can be performed in a single op, the second opcode is NOP.
//	This is useful for splitting a constant load and using the branch delay slot.

void	CAssemblyWriterPS2::GetLoadConstantOps( EPs2Reg reg, s32 value, Ps2OpCode * p_op1, Ps2OpCode * p_op2 )
{
	Ps2OpCode	op1;
	Ps2OpCode	op2;

	if ( value >= SHRT_MIN && value <= SHRT_MAX )
	{
		// ORI
		op1.op = OP_ADDIU;
		op1.rt = reg;
		op1.rs = Ps2Reg_R0;
		op1.immediate = u16( value );

		// NOP
		op2._u32 = 0;
	}
	else
	{
		// It's too large - we need to load in two parts
		op1.op = OP_LUI;
		op1.rt = reg;
		op1.rs = Ps2Reg_R0;
		op1.immediate = u16( value >> 16 );

		if ( u16( value ) != 0 )
		{
			op2.op = OP_ORI;
			op2.rt = reg;
			op2.rs = reg;
			op2.immediate = u16( value );
		}
		else
		{
			// NOP
			op2._u32 = 0;
		}
	}

	*p_op1 = op1;
	*p_op2 = op2;
}


//

void	CAssemblyWriterPS2::LoadConstant( EPs2Reg reg, s32 value )
{
	Ps2OpCode	op1;
	Ps2OpCode	op2;

	GetLoadConstantOps( reg, value, &op1, &op2 );

	AppendOp( op1 );

	// There may not be a second op if the low bits are 0, or the constant is small
	if( op2._u32 != 0 )
	{
		AppendOp( op2 );
	}
}


//

void	CAssemblyWriterPS2::LoadConstant64( EPs2Reg reg, s64 value )
{
	Ps2OpCode	op1;
	Ps2OpCode	op2;
	REG64 tmp;

	if (value >= INT_MIN && value <= INT_MAX)
	{
		LoadConstant( reg, value );
	} 
	else
	{
		tmp._s64 = value;
		LoadConstant(reg, tmp._s32_1);

		op1.mmi_op = MMI_PCPYLD;
		op1.rs = reg;
		op1.rt = Ps2Reg_R0;
		op1.rd = reg;
		op1.op = OP_MMI;

		AppendOp( op1 );

		LoadConstant(reg, tmp._s32_0);

		op2._u32 = 0;
		op2.mmi_op = MMI_PEXCW;
		op2.rt = reg;
		op2.rd = reg;
		op2.op = OP_MMI;

		AppendOp( op2 );
	}
}


//

void	CAssemblyWriterPS2::LoadRegister( EPs2Reg reg_dst, OpCodeValue load_op, EPs2Reg reg_base, s16 offset )
{
	Ps2OpCode		op_code;
	op_code._u32 = 0;

	op_code.op = load_op;
	op_code.rt = reg_dst;
	op_code.base = reg_base;
	op_code.immediate = offset;

	AppendOp( op_code );
}


//

void	CAssemblyWriterPS2::StoreRegister( EPs2Reg reg_src, OpCodeValue store_op, EPs2Reg reg_base, s16 offset )
{
	Ps2OpCode		op_code;
	op_code._u32 = 0;

	op_code.op = store_op;
	op_code.rt = reg_src;
	op_code.base = reg_base;
	op_code.immediate = offset;

	AppendOp( op_code );
}


//

void	CAssemblyWriterPS2::Exchange64( EPs2Reg reg )
{
	Ps2OpCode		op_code;
	
	op_code._u32 = 0;
	op_code.mmi_op = MMI_PEXTLW;
	op_code.rs = reg;
	op_code.rt = reg;
	op_code.rd = reg;
	op_code.op = OP_MMI;

	AppendOp( op_code );

	op_code._u32 = 0;
	op_code.mmi_op = MMI_PEXEW;
	op_code.rs = Ps2Reg_R0;
	op_code.rt = reg;
	op_code.rd = reg;
	op_code.op = OP_MMI;

	AppendOp( op_code );
}


//

void	CAssemblyWriterPS2::NOP()
{
	Ps2OpCode		op_code;
	op_code._u32 = 0;
	AppendOp( op_code );
}


//

void	CAssemblyWriterPS2::SD( EPs2Reg reg_src, EPs2Reg reg_base, s16 offset )
{
	StoreRegister( reg_src, OP_SD, reg_base, offset );
}


//

void	CAssemblyWriterPS2::SW( EPs2Reg reg_src, EPs2Reg reg_base, s16 offset )
{
	StoreRegister( reg_src, OP_SW, reg_base, offset );
}


//

void	CAssemblyWriterPS2::SH( EPs2Reg reg_src, EPs2Reg reg_base, s16 offset )
{
	StoreRegister( reg_src, OP_SH, reg_base, offset );
}


//

void	CAssemblyWriterPS2::SB( EPs2Reg reg_src, EPs2Reg reg_base, s16 offset )
{
	StoreRegister( reg_src, OP_SB, reg_base, offset );
}


//

void	CAssemblyWriterPS2::LB( EPs2Reg reg_dst, EPs2Reg reg_base, s16 offset )
{
	LoadRegister( reg_dst, OP_LB, reg_base, offset );
}


//

void	CAssemblyWriterPS2::LBU( EPs2Reg reg_dst, EPs2Reg reg_base, s16 offset )
{
	LoadRegister( reg_dst, OP_LBU, reg_base, offset );
}


//

void	CAssemblyWriterPS2::LH( EPs2Reg reg_dst, EPs2Reg reg_base, s16 offset )
{
	LoadRegister( reg_dst, OP_LH, reg_base, offset );
}


//

void	CAssemblyWriterPS2::LHU( EPs2Reg reg_dst, EPs2Reg reg_base, s16 offset )
{
	LoadRegister( reg_dst, OP_LHU, reg_base, offset );
}


//

void	CAssemblyWriterPS2::LW( EPs2Reg reg_dst, EPs2Reg reg_base, s16 offset )
{
	LoadRegister( reg_dst, OP_LW, reg_base, offset );
}


//

void	CAssemblyWriterPS2::LD( EPs2Reg reg_dst, EPs2Reg reg_base, s16 offset )
{
	LoadRegister( reg_dst, OP_LD, reg_base, offset );
}

//

void	CAssemblyWriterPS2::LWC1( EPs2FloatReg reg_dst, EPs2Reg reg_base, s16 offset )
{
	Ps2OpCode		op_code;
	op_code._u32 = 0;

	op_code.op = OP_LWC1;
	op_code.ft = reg_dst;
	op_code.base = reg_base;
	op_code.immediate = offset;

	AppendOp( op_code );
}


//

void	CAssemblyWriterPS2::SWC1( EPs2FloatReg reg_src, EPs2Reg reg_base, s16 offset )
{
	Ps2OpCode		op_code;
	op_code._u32 = 0;

	op_code.op = OP_SWC1;
	op_code.ft = reg_src;
	op_code.base = reg_base;
	op_code.immediate = offset;
	AppendOp( op_code );
}


//

void	CAssemblyWriterPS2::LUI( EPs2Reg reg, u16 value )
{
	Ps2OpCode	op_code;
	op_code.op = OP_LUI;
	op_code.rt = reg;
	op_code.rs = Ps2Reg_R0;
	op_code.immediate = value;
	AppendOp( op_code );
}


//

CJumpLocation	CAssemblyWriterPS2::JAL( CCodeLabel target, bool insert_delay )
{
	CJumpLocation	jump_location( mpCurrentBuffer->GetJumpLocation() );

	Ps2OpCode	op_code;
	op_code._u32 = 0;
	op_code.op = OP_JAL;
	op_code.target = target.GetTargetU32() >> 2;
	AppendOp( op_code );

	if(insert_delay)
	{
		// Stuff a nop in the delay slot
		op_code._u32 = 0;
		AppendOp( op_code );
	}

	return jump_location;
}


//

CJumpLocation	CAssemblyWriterPS2::J( CCodeLabel target, bool insert_delay )
{
	CJumpLocation	jump_location( mpCurrentBuffer->GetJumpLocation() );

	Ps2OpCode	op_code;
	op_code._u32 = 0;
	op_code.op = OP_J;
	op_code.target = target.GetTargetU32() >> 2;
	AppendOp( op_code );

	if(insert_delay)
	{
		// Stuff a nop in the delay slot
		op_code._u32 = 0;
		AppendOp( op_code );
	}

	return jump_location;
}


//

void 	CAssemblyWriterPS2::JR( EPs2Reg reg_link, bool insert_delay )
{
	Ps2OpCode	op_code;
	op_code._u32 = 0;
	op_code.op = OP_SPECOP;
	op_code.spec_op = SpecOp_JR;
	op_code.rs = reg_link;
	AppendOp( op_code );

	if(insert_delay)
	{
		// Stuff a nop in the delay slot
		op_code._u32 = 0;
		AppendOp( op_code );
	}
}


//

CJumpLocation	CAssemblyWriterPS2::BranchOp( EPs2Reg a, OpCodeValue op, EPs2Reg b, CCodeLabel target, bool insert_delay )
{
	CJumpLocation	branch_location( mpCurrentBuffer->GetJumpLocation() );
	s32				offset( branch_location.GetOffset( target ) );

	Ps2OpCode	op_code;
	op_code._u32 = 0;
	op_code.op = op;
	op_code.rs = a;
	op_code.rt = b;
	op_code.offset = s16((offset - 4) >> 2);	// Adjust for incremented PC and ignore lower bits
	AppendOp( op_code );

	if(insert_delay)
	{
		// Stuff a nop in the delay slot
		op_code._u32 = 0;
		AppendOp( op_code );
	}

	return branch_location;
}


//

CJumpLocation	CAssemblyWriterPS2::BNE( EPs2Reg a, EPs2Reg b, CCodeLabel target, bool insert_delay )
{
	return BranchOp( a, OP_BNE, b, target, insert_delay );
}


//

CJumpLocation	CAssemblyWriterPS2::BEQ( EPs2Reg a, EPs2Reg b, CCodeLabel target, bool insert_delay )
{
	return BranchOp( a, OP_BEQ, b, target, insert_delay );
}


//

CJumpLocation	CAssemblyWriterPS2::BNEL( EPs2Reg a, EPs2Reg b, CCodeLabel target, bool insert_delay )
{
	return BranchOp( a, OP_BNEL, b, target, insert_delay );
}


//

CJumpLocation	CAssemblyWriterPS2::BEQL( EPs2Reg a, EPs2Reg b, CCodeLabel target, bool insert_delay )
{
	return BranchOp( a, OP_BEQL, b, target, insert_delay );
}


//

CJumpLocation	CAssemblyWriterPS2::BLEZ( EPs2Reg a, CCodeLabel target, bool insert_delay )
{
	return BranchOp( a, OP_BLEZ, Ps2Reg_R0, target, insert_delay );
}


//

CJumpLocation	CAssemblyWriterPS2::BGTZ( EPs2Reg a, CCodeLabel target, bool insert_delay )
{
	return BranchOp( a, OP_BGTZ, Ps2Reg_R0, target, insert_delay );
}


//

CJumpLocation	CAssemblyWriterPS2::BranchRegImmOp( EPs2Reg a, ERegImmOp op, CCodeLabel target, bool insert_delay )
{
	CJumpLocation	branch_location( mpCurrentBuffer->GetJumpLocation() );
	s32				offset( branch_location.GetOffset( target ) );

	Ps2OpCode	op_code;
	op_code._u32 = 0;
	op_code.op = OP_REGIMM;
	op_code.rs = a;
	op_code.regimm_op = op;
	op_code.offset = s16((offset - 4) >> 2);	// Adjust for incremented PC and ignore lower bits
	AppendOp( op_code );

	if(insert_delay)
	{
		// Stuff a nop in the delay slot
		op_code._u32 = 0;
		AppendOp( op_code );
	}

	return branch_location;
}


//

CJumpLocation	CAssemblyWriterPS2::BLTZ( EPs2Reg a, CCodeLabel target, bool insert_delay )
{
	return BranchRegImmOp( a, RegImmOp_BLTZ, target, insert_delay );
}


//

CJumpLocation	CAssemblyWriterPS2::BGEZ( EPs2Reg a, CCodeLabel target, bool insert_delay )
{
	return BranchRegImmOp( a, RegImmOp_BGEZ, target, insert_delay );
}


//

CJumpLocation	CAssemblyWriterPS2::BLTZL( EPs2Reg a, CCodeLabel target, bool insert_delay )
{
	return BranchRegImmOp( a, RegImmOp_BLTZL, target, insert_delay );
}


//

CJumpLocation	CAssemblyWriterPS2::BGEZL( EPs2Reg a, CCodeLabel target, bool insert_delay )
{
	return BranchRegImmOp( a, RegImmOp_BGEZL, target, insert_delay );
}



//

void	CAssemblyWriterPS2::ADDI( EPs2Reg reg_dst, EPs2Reg reg_src, s16 value )
{
	Ps2OpCode	op_code;
	op_code._u32 = 0;
	op_code.op = OP_ADDI;
	op_code.rt = reg_dst;
	op_code.rs = reg_src;
	op_code.immediate = value;
	AppendOp( op_code );
}


//

void	CAssemblyWriterPS2::ADDIU( EPs2Reg reg_dst, EPs2Reg reg_src, s16 value )
{
	Ps2OpCode	op_code;
	op_code._u32 = 0;
	op_code.op = OP_ADDIU;
	op_code.rt = reg_dst;
	op_code.rs = reg_src;
	op_code.immediate = value;
	AppendOp( op_code );
}


//

void	CAssemblyWriterPS2::SLTI( EPs2Reg reg_dst, EPs2Reg reg_src, s16 value )
{
	Ps2OpCode	op_code;
	op_code._u32 = 0;
	op_code.op = OP_SLTI;
	op_code.rt = reg_dst;
	op_code.rs = reg_src;
	op_code.immediate = value;
	AppendOp( op_code );
}


//

void	CAssemblyWriterPS2::SLTIU( EPs2Reg reg_dst, EPs2Reg reg_src, s16 value )
{
	Ps2OpCode	op_code;
	op_code._u32 = 0;
	op_code.op = OP_SLTIU;
	op_code.rt = reg_dst;
	op_code.rs = reg_src;
	op_code.immediate = value;
	AppendOp( op_code );
}


//

void	CAssemblyWriterPS2::DADDI(EPs2Reg reg_dst, EPs2Reg reg_src, s16 value)
{
	Ps2OpCode	op_code;
	op_code._u32 = 0;
	op_code.op = OP_DADDI;
	op_code.rt = reg_dst;
	op_code.rs = reg_src;
	op_code.immediate = value;
	AppendOp(op_code);
}


//

void	CAssemblyWriterPS2::DADDIU(EPs2Reg reg_dst, EPs2Reg reg_src, s16 value)
{
	Ps2OpCode	op_code;
	op_code._u32 = 0;
	op_code.op = OP_DADDIU;
	op_code.rt = reg_dst;
	op_code.rs = reg_src;
	op_code.immediate = value;
	AppendOp(op_code);
}


//

void	CAssemblyWriterPS2::ANDI( EPs2Reg reg_dst, EPs2Reg reg_src, u16 immediate )
{
	Ps2OpCode	op_code;
	op_code._u32 = 0;
	op_code.op = OP_ANDI;
	op_code.rt = reg_dst;
	op_code.rs = reg_src;
	op_code.immediate = immediate;
	AppendOp( op_code );
}


//

void	CAssemblyWriterPS2::ORI( EPs2Reg reg_dst, EPs2Reg reg_src, u16 immediate )
{
	Ps2OpCode	op_code;
	op_code._u32 = 0;
	op_code.op = OP_ORI;
	op_code.rt = reg_dst;
	op_code.rs = reg_src;
	op_code.immediate = immediate;
	AppendOp( op_code );
}


//

void	CAssemblyWriterPS2::XORI( EPs2Reg reg_dst, EPs2Reg reg_src, u16 immediate )
{
	Ps2OpCode	op_code;
	op_code._u32 = 0;
	op_code.op = OP_XORI;
	op_code.rt = reg_dst;
	op_code.rs = reg_src;
	op_code.immediate = immediate;
	AppendOp( op_code );
}


//

void	CAssemblyWriterPS2::SLL( EPs2Reg reg_dst, EPs2Reg reg_src, u32 shift )
{
	Ps2OpCode	op_code;
	op_code._u32 = 0;
	op_code.op = OP_SPECOP;
	op_code.rd = reg_dst;
	op_code.rt = reg_src;
	op_code.sa = shift;
	op_code.spec_op = SpecOp_SLL;
	AppendOp( op_code );
}


//

void	CAssemblyWriterPS2::SRL( EPs2Reg reg_dst, EPs2Reg reg_src, u32 shift )
{
	Ps2OpCode	op_code;
	op_code._u32 = 0;
	op_code.op = OP_SPECOP;
	op_code.rd = reg_dst;
	op_code.rt = reg_src;
	op_code.sa = shift;
	op_code.spec_op = SpecOp_SRL;
	AppendOp( op_code );
}


//

void	CAssemblyWriterPS2::SRA( EPs2Reg reg_dst, EPs2Reg reg_src, u32 shift )
{
	Ps2OpCode	op_code;
	op_code._u32 = 0;
	op_code.op = OP_SPECOP;
	op_code.rd = reg_dst;
	op_code.rt = reg_src;
	op_code.sa = shift;
	op_code.spec_op = SpecOp_SRA;
	AppendOp( op_code );
}


//

void	CAssemblyWriterPS2::DSLL( EPs2Reg reg_dst, EPs2Reg reg_src, u32 shift )
{
	Ps2OpCode	op_code;
	op_code._u32 = 0;
	op_code.op = OP_SPECOP;
	op_code.rd = reg_dst;
	op_code.rt = reg_src;
	op_code.sa = shift;
	op_code.spec_op = SpecOp_DSLL;
	AppendOp( op_code );
}


//

void	CAssemblyWriterPS2::DSLL32( EPs2Reg reg_dst, EPs2Reg reg_src, u32 shift )
{
	Ps2OpCode	op_code;
	op_code._u32 = 0;
	op_code.op = OP_SPECOP;
	op_code.rd = reg_dst;
	op_code.rt = reg_src;
	op_code.sa = shift;
	op_code.spec_op = SpecOp_DSLL32;
	AppendOp( op_code );
}


//

void	CAssemblyWriterPS2::DSRL( EPs2Reg reg_dst, EPs2Reg reg_src, u32 shift )
{
	Ps2OpCode	op_code;
	op_code._u32 = 0;
	op_code.op = OP_SPECOP;
	op_code.rd = reg_dst;
	op_code.rt = reg_src;
	op_code.sa = shift;
	op_code.spec_op = SpecOp_DSRL;
	AppendOp( op_code );
}


//

void	CAssemblyWriterPS2::DSRL32( EPs2Reg reg_dst, EPs2Reg reg_src, u32 shift )
{
	Ps2OpCode	op_code;
	op_code._u32 = 0;
	op_code.op = OP_SPECOP;
	op_code.rd = reg_dst;
	op_code.rt = reg_src;
	op_code.sa = shift;
	op_code.spec_op = SpecOp_DSRL32;
	AppendOp( op_code );
}


//

void	CAssemblyWriterPS2::DSRA( EPs2Reg reg_dst, EPs2Reg reg_src, u32 shift )
{
	Ps2OpCode	op_code;
	op_code._u32 = 0;
	op_code.op = OP_SPECOP;
	op_code.rd = reg_dst;
	op_code.rt = reg_src;
	op_code.sa = shift;
	op_code.spec_op = SpecOp_DSRA;
	AppendOp( op_code );
}


//

void	CAssemblyWriterPS2::DSRA32( EPs2Reg reg_dst, EPs2Reg reg_src, u32 shift )
{
	Ps2OpCode	op_code;
	op_code._u32 = 0;
	op_code.op = OP_SPECOP;
	op_code.rd = reg_dst;
	op_code.rt = reg_src;
	op_code.sa = shift;
	op_code.spec_op = SpecOp_DSRA32;
	AppendOp( op_code );
}


//

void	CAssemblyWriterPS2::SpecOpLogical( EPs2Reg rd, EPs2Reg rs, ESpecOp op, EPs2Reg rt )
{
	Ps2OpCode	op_code;
	op_code._u32 = 0;
	op_code.op = OP_SPECOP;
	op_code.rd = rd;
	op_code.rs = rs;
	op_code.rt = rt;
	op_code.spec_op = op;
	AppendOp( op_code );
}


//RD = RT << RS

void	CAssemblyWriterPS2::SLLV( EPs2Reg rd, EPs2Reg rs, EPs2Reg rt )
{
	SpecOpLogical( rd, rs, SpecOp_SLLV, rt );
}


//RD = RT >> RS

void	CAssemblyWriterPS2::SRLV( EPs2Reg rd, EPs2Reg rs, EPs2Reg rt )
{
	SpecOpLogical( rd, rs, SpecOp_SRLV, rt );
}


//RD = RT >> RS

void	CAssemblyWriterPS2::SRAV( EPs2Reg rd, EPs2Reg rs, EPs2Reg rt )
{
	SpecOpLogical( rd, rs, SpecOp_DSRAV, rt );
}


//RD = RT << RS

void	CAssemblyWriterPS2::DSLLV( EPs2Reg rd, EPs2Reg rs, EPs2Reg rt )
{
	SpecOpLogical( rd, rs, SpecOp_DSLLV, rt );
}


//RD = RT >> RS

void	CAssemblyWriterPS2::DSRLV( EPs2Reg rd, EPs2Reg rs, EPs2Reg rt )
{
	SpecOpLogical( rd, rs, SpecOp_DSRLV, rt );
}


//RD = RT >> RS

void	CAssemblyWriterPS2::DSRAV( EPs2Reg rd, EPs2Reg rs, EPs2Reg rt )
{
	SpecOpLogical( rd, rs, SpecOp_DSRAV, rt );
}


//

void	CAssemblyWriterPS2::MFLO( EPs2Reg rd )
{
	SpecOpLogical( rd, Ps2Reg_R0, SpecOp_MFLO, Ps2Reg_R0 );
}


//

void	CAssemblyWriterPS2::MFHI( EPs2Reg rd )
{
	SpecOpLogical( rd, Ps2Reg_R0, SpecOp_MFHI, Ps2Reg_R0 );
}


//

void	CAssemblyWriterPS2::MULT( EPs2Reg rs, EPs2Reg rt )
{
	SpecOpLogical( Ps2Reg_R0, rs, SpecOp_MULT, rt );
}


//

void	CAssemblyWriterPS2::MULTU( EPs2Reg rs, EPs2Reg rt )
{
	SpecOpLogical( Ps2Reg_R0, rs, SpecOp_MULTU, rt );
}


//

void	CAssemblyWriterPS2::DIV( EPs2Reg rs, EPs2Reg rt )
{
	SpecOpLogical( Ps2Reg_R0, rs, SpecOp_DIV, rt );
}


//

void	CAssemblyWriterPS2::DIVU( EPs2Reg rs, EPs2Reg rt )
{
	SpecOpLogical( Ps2Reg_R0, rs, SpecOp_DIVU, rt );
}


//

void	CAssemblyWriterPS2::ADD( EPs2Reg rd, EPs2Reg rs, EPs2Reg rt )
{
	SpecOpLogical( rd, rs, SpecOp_ADD, rt );
}


//

void	CAssemblyWriterPS2::ADDU( EPs2Reg rd, EPs2Reg rs, EPs2Reg rt )
{
	SpecOpLogical( rd, rs, SpecOp_ADDU, rt );
}


//

void	CAssemblyWriterPS2::DADD( EPs2Reg rd, EPs2Reg rs, EPs2Reg rt )
{
	SpecOpLogical(rd, rs, SpecOp_DADD, rt);
}


//

void	CAssemblyWriterPS2::DADDU( EPs2Reg rd, EPs2Reg rs, EPs2Reg rt )
{
	SpecOpLogical( rd, rs, SpecOp_DADDU, rt );
}


//

void	CAssemblyWriterPS2::SUB( EPs2Reg rd, EPs2Reg rs, EPs2Reg rt )
{
	SpecOpLogical( rd, rs, SpecOp_SUB, rt );
}


//

void	CAssemblyWriterPS2::SUBU( EPs2Reg rd, EPs2Reg rs, EPs2Reg rt )
{
	SpecOpLogical( rd, rs, SpecOp_SUBU, rt );
}


//

void	CAssemblyWriterPS2::DSUB( EPs2Reg rd, EPs2Reg rs, EPs2Reg rt )
{
	SpecOpLogical( rd, rs, SpecOp_DSUB, rt );
}


//

void	CAssemblyWriterPS2::DSUBU( EPs2Reg rd, EPs2Reg rs, EPs2Reg rt )
{
	SpecOpLogical( rd, rs, SpecOp_DSUBU, rt );
}


//

void	CAssemblyWriterPS2::AND( EPs2Reg rd, EPs2Reg rs, EPs2Reg rt )
{
	SpecOpLogical( rd, rs, SpecOp_AND, rt );
}


//

void	CAssemblyWriterPS2::OR( EPs2Reg rd, EPs2Reg rs, EPs2Reg rt )
{
	SpecOpLogical( rd, rs, SpecOp_OR, rt );
}


//

void	CAssemblyWriterPS2::XOR( EPs2Reg rd, EPs2Reg rs, EPs2Reg rt )
{
	SpecOpLogical( rd, rs, SpecOp_XOR, rt );
}


//

void	CAssemblyWriterPS2::NOR( EPs2Reg rd, EPs2Reg rs, EPs2Reg rt )
{
	SpecOpLogical( rd, rs, SpecOp_NOR, rt );
}


//

void	CAssemblyWriterPS2::SLT( EPs2Reg rd, EPs2Reg rs, EPs2Reg rt )
{
	SpecOpLogical( rd, rs, SpecOp_SLT, rt );
}


//

void	CAssemblyWriterPS2::SLTU( EPs2Reg rd, EPs2Reg rs, EPs2Reg rt )
{
	SpecOpLogical( rd, rs, SpecOp_SLTU, rt );
}


//

void	CAssemblyWriterPS2::Cop1Op( ECop1Op cop1_op, EPs2FloatReg fd, EPs2FloatReg fs, ECop1OpFunction cop1_funct, EPs2FloatReg ft )
{
	Ps2OpCode	op_code;
	op_code._u32 = 0;
	op_code.op = OP_COPRO1;
	op_code.fd = fd;
	op_code.fs = fs;
	op_code.ft = ft;
	op_code.cop1_op = cop1_op;
	op_code.cop1_funct = cop1_funct;
	AppendOp( op_code );
}


//	Unary

void	CAssemblyWriterPS2::Cop1Op( ECop1Op cop1_op, EPs2FloatReg fd, EPs2FloatReg fs, ECop1OpFunction cop1_funct )
{
	Ps2OpCode	op_code;
	op_code._u32 = 0;
	op_code.op = OP_COPRO1;
	op_code.fd = fd;
	op_code.fs = fs;
	op_code.cop1_op = cop1_op;
	op_code.cop1_funct = cop1_funct;
	AppendOp( op_code );
}


//

void	CAssemblyWriterPS2::CFC1( EPs2Reg rt, EPs2FloatReg fs )
{
	Ps2OpCode	op_code;
	op_code._u32 = 0;
	op_code.op = OP_COPRO1;
	op_code.rt = rt;
	op_code.fs = fs;
	op_code.cop1_op = Cop1Op_CFC1;
	AppendOp( op_code );
}


//

void	CAssemblyWriterPS2::MFC1( EPs2Reg rt, EPs2FloatReg fs )
{
	Ps2OpCode	op_code;
	op_code._u32 = 0;
	op_code.op = OP_COPRO1;
	op_code.rt = rt;
	op_code.fs = fs;
	op_code.cop1_op = Cop1Op_MFC1;
	AppendOp( op_code );
}


//

void	CAssemblyWriterPS2::MTC1( EPs2FloatReg fs, EPs2Reg rt )
{
	Ps2OpCode	op_code;
	op_code._u32 = 0;
	op_code.op = OP_COPRO1;
	op_code.rt = rt;
	op_code.fs = fs;
	op_code.cop1_op = Cop1Op_MTC1;
	AppendOp( op_code );
}


//

void	CAssemblyWriterPS2::ADD_S( EPs2FloatReg fd, EPs2FloatReg fs, EPs2FloatReg ft )
{
	Cop1Op( Cop1Op_SInstr, fd, fs, Cop1OpFunc_ADD, ft );
}


//

void	CAssemblyWriterPS2::SUB_S( EPs2FloatReg fd, EPs2FloatReg fs, EPs2FloatReg ft )
{
	Cop1Op( Cop1Op_SInstr, fd, fs, Cop1OpFunc_SUB, ft );
}


//

void	CAssemblyWriterPS2::MUL_S( EPs2FloatReg fd, EPs2FloatReg fs, EPs2FloatReg ft )
{
	Cop1Op( Cop1Op_SInstr, fd, fs, Cop1OpFunc_MUL, ft );
}


//

void	CAssemblyWriterPS2::DIV_S( EPs2FloatReg fd, EPs2FloatReg fs, EPs2FloatReg ft )
{
	Cop1Op( Cop1Op_SInstr, fd, fs, Cop1OpFunc_DIV, ft );
}


//

void	CAssemblyWriterPS2::SQRT_S( EPs2FloatReg fd, EPs2FloatReg fs )
{
	Cop1Op( Cop1Op_SInstr, fd, Ps2FloatReg_F00, Cop1OpFunc_SQRT, fs);
}


//

void	CAssemblyWriterPS2::ABS_S( EPs2FloatReg fd, EPs2FloatReg fs )
{
	Cop1Op( Cop1Op_SInstr, fd, fs, Cop1OpFunc_ABS );
}


//

void	CAssemblyWriterPS2::MOV_S( EPs2FloatReg fd, EPs2FloatReg fs )
{
	Cop1Op( Cop1Op_SInstr, fd, fs, Cop1OpFunc_MOV );
}


//

void	CAssemblyWriterPS2::NEG_S( EPs2FloatReg fd, EPs2FloatReg fs )
{
	Cop1Op( Cop1Op_SInstr, fd, fs, Cop1OpFunc_NEG );
}


//

void	CAssemblyWriterPS2::CVT_W_S( EPs2FloatReg fd, EPs2FloatReg fs )
{
	Cop1Op( Cop1Op_SInstr, fd, fs, Cop1OpFunc_CVT_W );
}


//

void	CAssemblyWriterPS2::CMP_S( EPs2FloatReg fs, ECop1OpFunction	cmp_op, EPs2FloatReg ft )
{
	Cop1Op( Cop1Op_SInstr, Ps2FloatReg_F00, fs, cmp_op, ft );
}


//

void	CAssemblyWriterPS2::CVT_S_W( EPs2FloatReg fd, EPs2FloatReg fs )
{
	Cop1Op( Cop1Op_WInstr, fd, fs, Cop1OpFunc_CVT_S );
}


//

CJumpLocation	CAssemblyWriterPS2::BranchCop1( ECop1BCOp bc_op, CCodeLabel target, bool insert_delay )
{
	CJumpLocation	branch_location( mpCurrentBuffer->GetJumpLocation() );
	s32				offset( branch_location.GetOffset( target ) );

	Ps2OpCode	op_code;
	op_code._u32 = 0;
	op_code.op = OP_COPRO1;
	op_code.cop1_op = Cop1Op_BCInstr;
	op_code.cop1_bc = bc_op;
	op_code.offset = s16((offset - 4) >> 2);	// Adjust for incremented PC and ignore lower bits
	AppendOp( op_code );

	if(insert_delay)
	{
		// Stuff a nop in the delay slot
		op_code._u32 = 0;
		AppendOp( op_code );
	}

	return branch_location;
}


//

CJumpLocation	CAssemblyWriterPS2::BC1F( CCodeLabel target, bool insert_delay )
{
	return BranchCop1( Cop1BCOp_BC1F, target, insert_delay );
}


//

CJumpLocation	CAssemblyWriterPS2::BC1T( CCodeLabel target, bool insert_delay )
{
	return BranchCop1( Cop1BCOp_BC1T, target, insert_delay );
}
