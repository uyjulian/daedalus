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



#ifndef SYSPS2_DYNAREC_ASSEMBLYWRITERPS2_H_
#define SYSPS2_DYNAREC_ASSEMBLYWRITERPS2_H_

#include "DynaRec/AssemblyBuffer.h"
#include "DynarecTargetPS2.h"

#include "Core/R4300OpCode.h"

class CAssemblyWriterPS2
{
	public:
		CAssemblyWriterPS2( CAssemblyBuffer * p_buffer_a, CAssemblyBuffer * p_buffer_b )
			:	mpCurrentBuffer( p_buffer_a )
			,	mpAssemblyBufferA( p_buffer_a )
			,	mpAssemblyBufferB( p_buffer_b )
		{
		}

	public:
		CAssemblyBuffer *	GetAssemblyBuffer() const		{ return mpCurrentBuffer; }
		void				Finalise()
		{
			mpCurrentBuffer = nullptr;
			mpAssemblyBufferA = nullptr;
			mpAssemblyBufferB = nullptr;
		}

		void		SetBufferA()
		{
			mpCurrentBuffer = mpAssemblyBufferA;
		}
		void		SetBufferB()
		{
			mpCurrentBuffer = mpAssemblyBufferB;
		}
		bool		IsBufferA() const
		{
			return mpCurrentBuffer == mpAssemblyBufferA;
		}

	// XXXX
	private:
	public:
		void				LoadConstant( EPs2Reg reg, s32 value );

		void				LoadRegister( EPs2Reg reg_dst, OpCodeValue load_op, EPs2Reg reg_base, s16 offset );
		void				StoreRegister( EPs2Reg reg_src, OpCodeValue store_op, EPs2Reg reg_base, s16 offset );

		void				NOP();
		void				LUI( EPs2Reg reg, u16 value );

		void				SW( EPs2Reg reg_src, EPs2Reg reg_base, s16 offset );
		void				SH( EPs2Reg reg_src, EPs2Reg reg_base, s16 offset );
		void				SB( EPs2Reg reg_src, EPs2Reg reg_base, s16 offset );

		void				LB( EPs2Reg reg_dst, EPs2Reg reg_base, s16 offset );
		void				LBU( EPs2Reg reg_dst, EPs2Reg reg_base, s16 offset );
		void				LH( EPs2Reg reg_dst, EPs2Reg reg_base, s16 offset );
		void				LHU( EPs2Reg reg_dst, EPs2Reg reg_base, s16 offset );
		void				LW( EPs2Reg reg_dst, EPs2Reg reg_base, s16 offset );

		void				LWC1( EPs2FloatReg reg_dst, EPs2Reg reg_base, s16 offset );
		void				SWC1( EPs2FloatReg reg_src, EPs2Reg reg_base, s16 offset );

		CJumpLocation		JAL( CCodeLabel target, bool insert_delay );
		CJumpLocation		J( CCodeLabel target, bool insert_delay );

		void 				JR( EPs2Reg reg_link, bool insert_delay );

		CJumpLocation		BNE( EPs2Reg a, EPs2Reg b, CCodeLabel target, bool insert_delay );
		CJumpLocation		BEQ( EPs2Reg a, EPs2Reg b, CCodeLabel target, bool insert_delay );
		CJumpLocation		BNEL( EPs2Reg a, EPs2Reg b, CCodeLabel target, bool insert_delay );
		CJumpLocation		BEQL( EPs2Reg a, EPs2Reg b, CCodeLabel target, bool insert_delay );
		CJumpLocation		BLEZ( EPs2Reg a, CCodeLabel target, bool insert_delay );
		CJumpLocation		BGTZ( EPs2Reg a, CCodeLabel target, bool insert_delay );

		CJumpLocation		BLTZ( EPs2Reg a, CCodeLabel target, bool insert_delay );
		CJumpLocation		BGEZ( EPs2Reg a, CCodeLabel target, bool insert_delay );
		CJumpLocation		BLTZL( EPs2Reg a, CCodeLabel target, bool insert_delay );
		CJumpLocation		BGEZL( EPs2Reg a, CCodeLabel target, bool insert_delay );

		//void				EXT( EPs2Reg reg_dst, EPs2Reg reg_src, u32 size, u32 lsb );
		//void				INS( EPs2Reg reg_dst, EPs2Reg reg_src, u32 msb, u32 lsb );
		void				ADDI( EPs2Reg reg_dst, EPs2Reg reg_src, s16 value );
		void				ADDIU( EPs2Reg reg_dst, EPs2Reg reg_src, s16 value );
		void				SLTI( EPs2Reg reg_dst, EPs2Reg reg_src, s16 value );
		void				SLTIU( EPs2Reg reg_dst, EPs2Reg reg_src, s16 value );

		void				ANDI( EPs2Reg reg_dst, EPs2Reg reg_src, u16 immediate );
		void				ORI( EPs2Reg reg_dst, EPs2Reg reg_src, u16 immediate );
		void				XORI( EPs2Reg reg_dst, EPs2Reg reg_src, u16 immediate );

		void				SLL( EPs2Reg reg_dst, EPs2Reg reg_src, u32 shift );
		void				SRL( EPs2Reg reg_dst, EPs2Reg reg_src, u32 shift );
		void				SRA( EPs2Reg reg_dst, EPs2Reg reg_src, u32 shift );
		void				SLLV( EPs2Reg rd, EPs2Reg rs, EPs2Reg rt );
		void				SRLV( EPs2Reg rd, EPs2Reg rs, EPs2Reg rt );
		void				SRAV( EPs2Reg rd, EPs2Reg rs, EPs2Reg rt );

		void				MFLO( EPs2Reg rd );
		void				MFHI( EPs2Reg rd );
		void				MULT( EPs2Reg rs, EPs2Reg rt );
		void				MULTU( EPs2Reg rs, EPs2Reg rt );
		void				DIV( EPs2Reg rs, EPs2Reg rt );
		void				DIVU( EPs2Reg rs, EPs2Reg rt );

		void				ADD( EPs2Reg rd, EPs2Reg rs, EPs2Reg rt );
		void				ADDU( EPs2Reg rd, EPs2Reg rs, EPs2Reg rt );
		void				SUB( EPs2Reg rd, EPs2Reg rs, EPs2Reg rt );
		void				SUBU( EPs2Reg rd, EPs2Reg rs, EPs2Reg rt );
		void				AND( EPs2Reg rd, EPs2Reg rs, EPs2Reg rt );
		void				OR( EPs2Reg rd, EPs2Reg rs, EPs2Reg rt );
		void				XOR( EPs2Reg rd, EPs2Reg rs, EPs2Reg rt );
		void				NOR( EPs2Reg rd, EPs2Reg rs, EPs2Reg rt );

		void				SLT( EPs2Reg rd, EPs2Reg rs, EPs2Reg rt );
		void				SLTU( EPs2Reg rd, EPs2Reg rs, EPs2Reg rt );

		void				Cop1Op( ECop1Op cop1_op, EPs2FloatReg fd, EPs2FloatReg fs, ECop1OpFunction cop1_funct, EPs2FloatReg ft );
		void				Cop1Op( ECop1Op cop1_op, EPs2FloatReg fd, EPs2FloatReg fs, ECop1OpFunction cop1_funct );

		void				CFC1( EPs2Reg rt, EPs2FloatReg fs );

		void				MFC1( EPs2Reg rt, EPs2FloatReg fs );
		void				MTC1( EPs2FloatReg fs, EPs2Reg rt );

		void				ADD_S( EPs2FloatReg fd, EPs2FloatReg fs, EPs2FloatReg ft );
		void				SUB_S( EPs2FloatReg fd, EPs2FloatReg fs, EPs2FloatReg ft );
		void				MUL_S( EPs2FloatReg fd, EPs2FloatReg fs, EPs2FloatReg ft );
		void				DIV_S( EPs2FloatReg fd, EPs2FloatReg fs, EPs2FloatReg ft );

		void				SQRT_S( EPs2FloatReg fd, EPs2FloatReg fs );
		void				ABS_S( EPs2FloatReg fd, EPs2FloatReg fs );
		void				MOV_S( EPs2FloatReg fd, EPs2FloatReg fs );
		void				NEG_S( EPs2FloatReg fd, EPs2FloatReg fs );

		//void				TRUNC_W_S( EPs2FloatReg fd, EPs2FloatReg fs );
		//void				FLOOR_W_S( EPs2FloatReg fd, EPs2FloatReg fs );
		void				CVT_W_S( EPs2FloatReg fd, EPs2FloatReg fs );

		void				CMP_S( EPs2FloatReg fs, ECop1OpFunction	cmp_op, EPs2FloatReg ft );

		void				CVT_S_W( EPs2FloatReg fd, EPs2FloatReg fs );

		CJumpLocation		BranchCop1( ECop1BCOp bc_op, CCodeLabel target, bool insert_delay );
		CJumpLocation		BC1F( CCodeLabel target, bool insert_delay );
		CJumpLocation		BC1T( CCodeLabel target, bool insert_delay );

		static	void		GetLoadConstantOps( EPs2Reg reg, s32 value, Ps2OpCode * p_op1, Ps2OpCode * p_op2 );

		inline void AppendOp( Ps2OpCode op )
		{
			mpCurrentBuffer->EmitDWORD( op._u32 );
		}

	private:
		CJumpLocation		BranchOp( EPs2Reg a, OpCodeValue op, EPs2Reg b, CCodeLabel target, bool insert_delay );
		CJumpLocation		BranchRegImmOp( EPs2Reg a, ERegImmOp op, CCodeLabel target, bool insert_delay );
		void				SpecOpLogical( EPs2Reg rd, EPs2Reg rs, ESpecOp op, EPs2Reg rt );

	private:
		CAssemblyBuffer *				mpCurrentBuffer;
		CAssemblyBuffer *				mpAssemblyBufferA;
		CAssemblyBuffer *				mpAssemblyBufferB;
};

#endif // SYSPSP_DYNAREC_ASSEMBLYWRITERPSP_H_
