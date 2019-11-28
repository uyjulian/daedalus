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
#include "CodeGeneratorPS2.h"

#include <limits.h>
#include <stdio.h>

#include <algorithm>

#include "N64RegisterCachePS2.h"

#include "Config/ConfigOptions.h"
#include "Core/CPU.h"
#include "Core/Memory.h"
#include "Core/R4300.h"
#include "Core/Registers.h"
#include "Core/ROM.h"
#include "Debug/DBGConsole.h"
#include "DynaRec/AssemblyUtils.h"
#include "DynaRec/Trace.h"
#include "Math/MathUtil.h"
#include "OSHLE/ultra_R4300.h"
#include "Utility/Macros.h"
#include "Utility/PrintOpCode.h"
#include "Utility/Profiler.h"

using namespace AssemblyUtils;

//Enable unaligned load/store(used in CBFD, OOT, Rayman2 and PD) //Corn
#define ENABLE_LWR_LWL
//#define ENABLE_SWR_SWL

//Enable to load/store floats directly to/from FPU //Corn
#define ENABLE_LWC1
#define ENABLE_SWC1
//#define ENABLE_LDC1
//#define ENABLE_SDC1

//Define to handle full 64bit checks for SLT,SLTU,SLTI & SLTIU //Corn
//#define ENABLE_64BIT

//Define to check for DIV / 0 //Corn
//#define DIVZEROCHK

//Define to enable exceptions for interpreter calls from DYNAREC
//#define ALLOW_INTERPRETER_EXCEPTIONS



#define NOT_IMPLEMENTED( x )	DAEDALUS_ERROR( x )

extern "C" { const void * g_MemoryLookupTableReadForDynarec = g_MemoryLookupTableRead; }	//Important pointer for Dynarec see DynaRecStubs.s

extern "C" { void _DDIV( s64 Num, s32 Div ); }	//signed 64bit division  //Corn
extern "C" { void _DDIVU( u64 Num, u32 Div ); }	//unsigned 64bit division  //Corn
extern "C" { void _DMULTU( u64 A, u64 B ); }	//unsigned 64bit multiply  //Corn
extern "C" { void _DMULT( s64 A, s64 B ); }	//signed 64bit multiply (result is 64bit not 128bit!)  //Corn

extern "C" { u64 _FloatToDouble( u32 _float); }	//Uses CPU to pass f64/32 thats why its maskerading as u64/32 //Corn
extern "C" { u32 _DoubleToFloat( u64 _double); }	//Uses CPU to pass f64/32 thats why its maskerading as u64/32 //Corn

extern "C" { void _ReturnFromDynaRec(); }
extern "C" { void _DirectExitCheckNoDelay( u32 instructions_executed, u32 exit_pc ); }
extern "C" { void _DirectExitCheckDelay( u32 instructions_executed, u32 exit_pc, u32 target_pc ); }
extern "C" { void _IndirectExitCheck( u32 instructions_executed, CIndirectExitMap * p_map, u32 target_pc ); }

extern "C"
{
	u32 _ReadBitsDirect_u8( u32 address, u32 current_pc );
	u32 _ReadBitsDirect_s8( u32 address, u32 current_pc );
	u32 _ReadBitsDirect_u16( u32 address, u32 current_pc );
	u32 _ReadBitsDirect_s16( u32 address, u32 current_pc );
	u32 _ReadBitsDirect_u32( u32 address, u32 current_pc );

	u32 _ReadBitsDirectBD_u8( u32 address, u32 current_pc );
	u32 _ReadBitsDirectBD_s8( u32 address, u32 current_pc );
	u32 _ReadBitsDirectBD_u16( u32 address, u32 current_pc );
	u32 _ReadBitsDirectBD_s16( u32 address, u32 current_pc );
	u32 _ReadBitsDirectBD_u32( u32 address, u32 current_pc );

	// Dynarec calls this for simplicity
	void Write32BitsForDynaRec( u32 address, u32 value )
	{
		Write32Bits_NoSwizzle( address, value );
	}
	void Write16BitsForDynaRec( u32 address, u16 value )
	{
		Write16Bits_NoSwizzle( address, value );
	}
	void Write8BitsForDynaRec( u32 address, u8 value )
	{
		Write8Bits_NoSwizzle( address, value );
	}

	void _WriteBitsDirect_u32( u32 address, u32 value, u32 current_pc );
	void _WriteBitsDirect_u16( u32 address, u32 value, u32 current_pc );		// Value in low 16 bits
	void _WriteBitsDirect_u8( u32 address, u32 value, u32 current_pc );			// Value in low 8 bits

	void _WriteBitsDirectBD_u32( u32 address, u32 value, u32 current_pc );
	void _WriteBitsDirectBD_u16( u32 address, u32 value, u32 current_pc );		// Value in low 16 bits
	void _WriteBitsDirectBD_u8( u32 address, u32 value, u32 current_pc );			// Value in low 8 bits
}

#define ReadBitsDirect_u8 _ReadBitsDirect_u8
#define ReadBitsDirect_s8 _ReadBitsDirect_s8
#define ReadBitsDirect_u16 _ReadBitsDirect_u16
#define ReadBitsDirect_s16 _ReadBitsDirect_s16
#define ReadBitsDirect_u32 _ReadBitsDirect_u32

#define ReadBitsDirectBD_u8 _ReadBitsDirectBD_u8
#define ReadBitsDirectBD_s8 _ReadBitsDirectBD_s8
#define ReadBitsDirectBD_u16 _ReadBitsDirectBD_u16
#define ReadBitsDirectBD_s16 _ReadBitsDirectBD_s16
#define ReadBitsDirectBD_u32 _ReadBitsDirectBD_u32


#define WriteBitsDirect_u32 _WriteBitsDirect_u32
#define WriteBitsDirect_u16 _WriteBitsDirect_u16
#define WriteBitsDirect_u8 _WriteBitsDirect_u8

#define WriteBitsDirectBD_u32 _WriteBitsDirectBD_u32
#define WriteBitsDirectBD_u16 _WriteBitsDirectBD_u16
#define WriteBitsDirectBD_u8 _WriteBitsDirectBD_u8



//Used to print value from ASM just add these two OPs (all caller saved regs will to be saved including RA and A0) //Corn
//	jal		_printf_asm
//	lw		$a0, _Delay($fp)	#ex: prints Delay value
// OR ADD
//	JAL( CCodeLabel( (void*)_printf_asm ), false );
//	OR( Ps2Reg_A0, dst_reg, Ps2Reg_R0 );
extern "C" { void _printf_asm( u32 val ); }
extern "C" { void output_extern( u32 val ) { printf("%d\n", val); } }

extern "C" { void _ReturnFromDynaRecIfStuffToDo( u32 register_mask ); }
extern "C" { void _DaedalusICacheInvalidate( const void * address, u32 length ); }

bool			gHaveSavedPatchedOps = false;
Ps2OpCode		gOriginalOps[2];
Ps2OpCode		gReplacementOps[2];

#define URO_HI_SIGN_EXTEND 0	// Sign extend from src
#define URO_HI_CLEAR	   1	// Clear hi bits

void Dynarec_ClearedCPUStuffToDo()
{
	// Replace first two ops of _ReturnFromDynaRecIfStuffToDo with 'jr ra, nop'
	u8 *			p_void_function( (u8 *)( _ReturnFromDynaRecIfStuffToDo ) );
	Ps2OpCode *		p_function_address = reinterpret_cast< Ps2OpCode * >( MAKE_UNCACHED_PTR( p_void_function ) );

	if(!gHaveSavedPatchedOps)
	{
		gOriginalOps[0] = p_function_address[0];
		gOriginalOps[1] = p_function_address[1];

		Ps2OpCode	op_code;
		op_code._u32 = 0;
		op_code.op = OP_SPECOP;
		op_code.spec_op = SpecOp_JR;
		op_code.rs = Ps2Reg_RA;
		gReplacementOps[0] = op_code;		// JR RA
		gReplacementOps[1]._u32 = 0;		// NOP

		gHaveSavedPatchedOps = true;
	}

	p_function_address[0] = gReplacementOps[0];
	p_function_address[1] = gReplacementOps[1];

	const u8 * p_lower( RoundPointerDown( p_void_function, 64 ) );
	const u8 * p_upper( RoundPointerUp( p_void_function + 8, 64 ) );
	const u32  size( p_upper - p_lower);
	//sceKernelDcacheWritebackRange( p_lower, size );
	//sceKernelIcacheInvalidateRange( p_lower, size );

	_DaedalusICacheInvalidate( p_lower, size );
}

void Dynarec_SetCPUStuffToDo()
{
	// Restore first two ops of _ReturnFromDynaRecIfStuffToDo

	u8 *			p_void_function( ( u8 *)( _ReturnFromDynaRecIfStuffToDo ) );
	Ps2OpCode *		p_function_address = reinterpret_cast< Ps2OpCode * >( MAKE_UNCACHED_PTR( p_void_function ) );

	p_function_address[0] = gOriginalOps[0];
	p_function_address[1] = gOriginalOps[1];

	const u8 * p_lower( RoundPointerDown( p_void_function, 64 ) );
	const u8 * p_upper( RoundPointerUp( p_void_function + 8, 64 ) );
	const u32  size( p_upper - p_lower);
	//sceKernelDcacheWritebackRange( p_lower, size );
	//sceKernelIcacheInvalidateRange( p_lower, size );

	_DaedalusICacheInvalidate( p_lower, size );
}

extern "C"
{
void HandleException_extern()
{
	switch (gCPUState.Delay)
	{
		case DO_DELAY:
			gCPUState.Delay = EXEC_DELAY;	//fall through to PC +=4
		case NO_DELAY:
			gCPUState.CurrentPC += 4;
			break;
		case EXEC_DELAY:
			gCPUState.CurrentPC = gCPUState.TargetPC;
			gCPUState.Delay = NO_DELAY;
			break;
		default:
			NODEFAULT;
	}
}
}

namespace
{
const EPs2Reg	gMemUpperBoundReg = Ps2Reg_S6;
const EPs2Reg	gMemoryBaseReg = Ps2Reg_S7;

const EPs2Reg	gRegistersToUseForCaching[] =
{
//	Ps2Reg_V0,		// Used as calculation temp
//	Ps2Reg_V1,		// Used as calculation temp
//	Ps2Reg_A0,		// Used as calculation temp
//	Ps2Reg_A1,		// Used as calculation temp
//	Ps2Reg_A2,		// Used as calculation temp
//	Ps2Reg_A3,		// Used as calculation temp
	Ps2Reg_AT,
	Ps2Reg_T0,
	Ps2Reg_T1,
	Ps2Reg_T2,
	Ps2Reg_T3,
	Ps2Reg_T4,
	Ps2Reg_T5,
	Ps2Reg_T6,
	Ps2Reg_T7,
	Ps2Reg_T8,
	Ps2Reg_T9,
	Ps2Reg_S0,
	Ps2Reg_S1,
	Ps2Reg_S2,
	Ps2Reg_S3,
	Ps2Reg_S4,
//	Ps2Reg_S5,		//Used as load base register in ps2, K0 is probably trashed by interupts :(
//	Ps2Reg_S6,		// Memory upper bound
//	Ps2Reg_S7,		// Used for g_pu8RamBase - 0x80000000
//	Ps2Reg_S8,		// Used for base register (&gCPUState)
//	Ps2Reg_K0,		//Used as load base register. Normally it is reserved for Kernel but seems to work if we borrow it...(could come back and bite us if we use kernel stuff?) //Corn  //Can't use it in ps2
//	Ps2Reg_K1,		//Used as store base register. Normally it is reserved for Kernel but seems to work if we borrow it...(could come back and bite us if we use kernel stuff?)
//	Ps2Reg_GP,		//This needs saving to work?
};
}

//u32		gTotalRegistersCached = 0;
//u32		gTotalRegistersUncached = 0;


//

CCodeGeneratorPS2::CCodeGeneratorPS2( CAssemblyBuffer * p_buffer_a, CAssemblyBuffer * p_buffer_b )
:	CCodeGenerator( )
,	CAssemblyWriterPS2( p_buffer_a, p_buffer_b )
,	mpBasePointer( nullptr )
,	mBaseRegister( Ps2Reg_S8 )		// TODO
,	mEntryAddress( 0 )
,	mLoopTop( nullptr )
,	mUseFixedRegisterAllocation( false )
{
}


//

void	CCodeGeneratorPS2::Initialise( u32 entry_address, u32 exit_address, u32 * hit_counter, const void * p_base, const SRegisterUsageInfo & register_usage )
{
	mEntryAddress = entry_address;

	mpBasePointer = reinterpret_cast< const u8 * >( p_base );
	SetRegisterSpanList( register_usage, entry_address == exit_address );

	mPreviousLoadBase = N64Reg_R0;	//Invalidate
	mPreviousStoreBase = N64Reg_R0;	//Invalidate
	mFloatCMPIsValid = false;
	mMultIsValid = false;

	if( hit_counter != nullptr )
	{
		GetVar( Ps2Reg_V0, hit_counter );
		ADDIU( Ps2Reg_V0, Ps2Reg_V0, 1 );
		SetVar( hit_counter, Ps2Reg_V0 );
	}
}


//

void	CCodeGeneratorPS2::Finalise( ExceptionHandlerFn p_exception_handler_fn, const std::vector< CJumpLocation > & exception_handler_jumps )
{
	// We handle exceptions directly with _ReturnFromDynaRecIfStuffToDo - we should never get here on the psp
	#ifdef DAEDALUS_ENABLE_ASSERTS
	DAEDALUS_ASSERT( exception_handler_jumps.empty(), "Not expecting to have any exception handler jumps to process" );
	#endif
	GenerateAddressCheckFixups();

	CAssemblyWriterPS2::Finalise();
}


//

void	CCodeGeneratorPS2::SetRegisterSpanList( const SRegisterUsageInfo & register_usage, bool loops_to_self )
{
	mRegisterSpanList = register_usage.SpanList;

	// Sort in order of increasing start point
	std::sort( mRegisterSpanList.begin(), mRegisterSpanList.end(), SAscendingSpanStartSort() );

	const u32 NUM_CACHE_REGS( sizeof(gRegistersToUseForCaching) / sizeof(gRegistersToUseForCaching[0]) );

	// Push all the available registers in reverse order (i.e. use temporaries later)
	// Use temporaries first so we can avoid flushing them in case of a funcion call //Corn
		#ifdef DAEDALUS_ENABLE_ASSERTS
	DAEDALUS_ASSERT( mAvailableRegisters.empty(), "Why isn't the available register list empty?" );
	#endif
	for( u32 i = 0; i < NUM_CACHE_REGS; i++ )
	{
		mAvailableRegisters.push( gRegistersToUseForCaching[ i ] );
	}

	// Optimization for self looping code
	if( gDynarecLoopOptimisation & loops_to_self )
	{
		mUseFixedRegisterAllocation = true;
		u32		cache_reg_idx( 0 );
		u32		HiLo = 0;
		while ( HiLo<2 )		// If there are still unused registers, assign to high part of reg
		{
			RegisterSpanList::const_iterator span_it = mRegisterSpanList.begin();
			while( span_it < mRegisterSpanList.end() )
			{
				const SRegisterSpan &	span( *span_it );
				if( cache_reg_idx < NUM_CACHE_REGS )
				{
					EPs2Reg		cachable_reg( gRegistersToUseForCaching[cache_reg_idx] );
					mRegisterCache.SetCachedReg( span.Register, HiLo, cachable_reg );
					cache_reg_idx++;
				}
				++span_it;
			}
			++HiLo;
		}
		//
		//	Pull all the cached registers into memory
		//
		// Skip r0
		u32 i = 1;
		while( i < NUM_N64_REGS )
		{
			EN64Reg	n64_reg = EN64Reg( i );
			u32 lo_hi_idx = 0;
			while( lo_hi_idx < 2)
			{
				if( mRegisterCache.IsCached( n64_reg, lo_hi_idx ) )
				{
					PrepareCachedRegister( n64_reg, lo_hi_idx );

					//
					//	If the register is modified anywhere in the fragment, we need
					//	to mark it as dirty so it's flushed correctly on exit.
					//
					if( register_usage.IsModified( n64_reg ) )
					{
						mRegisterCache.MarkAsDirty( n64_reg, lo_hi_idx, true );
					}
				}
				++lo_hi_idx;
			}
			++i;
		}
		mLoopTop = GetAssemblyBuffer()->GetLabel();
	} //End of Loop optimization code
}


//

void	CCodeGeneratorPS2::ExpireOldIntervals( u32 instruction_idx )
{
	// mActiveIntervals is held in order of increasing end point
	for(RegisterSpanList::iterator span_it = mActiveIntervals.begin(); span_it < mActiveIntervals.end(); ++span_it )
	{
		const SRegisterSpan &	span( *span_it );

		if( span.SpanEnd >= instruction_idx )
		{
			break;
		}

		// This interval is no longer active - flush the register and return it to the list of available regs
		EPs2Reg		ps2_reg( mRegisterCache.GetCachedReg( span.Register, 0 ) );

		FlushRegister( mRegisterCache, span.Register, 0, true );

		mRegisterCache.ClearCachedReg( span.Register, 0 );

		mAvailableRegisters.push( ps2_reg );

		span_it = mActiveIntervals.erase( span_it );
	}
}


//

void	CCodeGeneratorPS2::SpillAtInterval( const SRegisterSpan & live_span )
{
	#ifdef DAEDALUS_ENABLE_ASSERTS
	DAEDALUS_ASSERT( !mActiveIntervals.empty(), "There are no active intervals" );
	#endif
	const SRegisterSpan &	last_span( mActiveIntervals.back() );		// Spill the last active interval (it has the greatest end point)

	if( last_span.SpanEnd > live_span.SpanEnd )
	{
		// Uncache the old span
		EPs2Reg		ps2_reg( mRegisterCache.GetCachedReg( last_span.Register, 0 ) );
		FlushRegister( mRegisterCache, last_span.Register, 0, true );
		mRegisterCache.ClearCachedReg( last_span.Register, 0 );

		// Cache the new span
		mRegisterCache.SetCachedReg( live_span.Register, 0, ps2_reg );

		mActiveIntervals.pop_back();				// Remove the last span
		mActiveIntervals.push_back( live_span );	// Insert in order of increasing end point

		std::sort( mActiveIntervals.begin(), mActiveIntervals.end(), SAscendingSpanEndSort() );		// XXXX - will be quicker to insert in the correct place rather than sorting each time
	}
	else
	{
		// There is no space for this register - we just don't update the register cache info, so we save/restore it from memory as needed
	}
}


//

void	CCodeGeneratorPS2::UpdateRegisterCaching( u32 instruction_idx )
{
	if( !mUseFixedRegisterAllocation )
	{
		ExpireOldIntervals( instruction_idx );

		for(RegisterSpanList::const_iterator span_it = mRegisterSpanList.begin(); span_it < mRegisterSpanList.end(); ++span_it )
		{
			const SRegisterSpan &	span( *span_it );

			// As we keep the intervals sorted in order of SpanStart, we can exit as soon as we encounter a SpanStart in the future
			if( instruction_idx < span.SpanStart )
			{
				break;
			}

			// Only process live intervals
			if( (instruction_idx >= span.SpanStart) & (instruction_idx <= span.SpanEnd) )
			{
				if( !mRegisterCache.IsCached( span.Register, 0 ) )
				{
					if( mAvailableRegisters.empty() )
					{
						SpillAtInterval( span );
					}
					else
					{
						// Use this register for caching
						mRegisterCache.SetCachedReg( span.Register, 0, mAvailableRegisters.top() );

						// Pop this register from the available list
						mAvailableRegisters.pop();
						mActiveIntervals.push_back( span );		// Insert in order of increasing end point

						std::sort( mActiveIntervals.begin(), mActiveIntervals.end(), SAscendingSpanEndSort() );		// XXXX - will be quicker to insert in the correct place rather than sorting each time
					}
				}
			}
		}
	}
}


//

RegisterSnapshotHandle	CCodeGeneratorPS2::GetRegisterSnapshot()
{
	RegisterSnapshotHandle	handle( mRegisterSnapshots.size() );

	mRegisterSnapshots.push_back( mRegisterCache );

	return handle;
}


//

CCodeLabel	CCodeGeneratorPS2::GetEntryPoint() const
{
	return GetAssemblyBuffer()->GetStartAddress();
}


//

CCodeLabel	CCodeGeneratorPS2::GetCurrentLocation() const
{
	return GetAssemblyBuffer()->GetLabel();
}


//

u32	CCodeGeneratorPS2::GetCompiledCodeSize() const
{
	return GetAssemblyBuffer()->GetSize();
}


//Get a (cached) N64 register mapped to a PSP register(usefull for dst register)

EPs2Reg	CCodeGeneratorPS2::GetRegisterNoLoad( EN64Reg n64_reg, u32 lo_hi_idx, EPs2Reg scratch_reg )
{
	if( mRegisterCache.IsCached( n64_reg, lo_hi_idx ) )
	{
		return mRegisterCache.GetCachedReg( n64_reg, lo_hi_idx );
	}
	else
	{
		return scratch_reg;
	}
}


//Load value from an emulated N64 register or known value to a PSP register

void	CCodeGeneratorPS2::GetRegisterValue( EPs2Reg ps2_reg, EN64Reg n64_reg, u32 lo_hi_idx )
{
	if( mRegisterCache.IsKnownValue( n64_reg, lo_hi_idx ) )
	{
		//printf( "Loading %s[%d] <- %08x\n", RegNames[ n64_reg ], lo_hi_idx, mRegisterCache.GetKnownValue( n64_reg, lo_hi_idx ) );
		LoadConstant( ps2_reg, mRegisterCache.GetKnownValue( n64_reg, lo_hi_idx )._s32 );
		if( mRegisterCache.IsCached( n64_reg, lo_hi_idx ) )
		{
			mRegisterCache.MarkAsValid( n64_reg, lo_hi_idx, true );
			mRegisterCache.MarkAsDirty( n64_reg, lo_hi_idx, true );
			mRegisterCache.ClearKnownValue( n64_reg, lo_hi_idx );
		}
	}
	else
	{
		GetVar( ps2_reg, lo_hi_idx ? &gGPR[ n64_reg ]._u32_1 : &gGPR[ n64_reg ]._u32_0 );
	}
}


//Get (cached) N64 register value mapped to a PSP register (or scratch reg)
//and also load the value if not loaded yet(usefull for src register)

EPs2Reg	CCodeGeneratorPS2::GetRegisterAndLoad( EN64Reg n64_reg, u32 lo_hi_idx, EPs2Reg scratch_reg )
{
	EPs2Reg		reg;
	bool		need_load( false );

	if( mRegisterCache.IsCached( n64_reg, lo_hi_idx ) )
	{
//		gTotalRegistersCached++;
		reg = mRegisterCache.GetCachedReg( n64_reg, lo_hi_idx );

		// We're loading it below, so set the valid flag
		if( !mRegisterCache.IsValid( n64_reg, lo_hi_idx ) )
		{
			need_load = true;
			mRegisterCache.MarkAsValid( n64_reg, lo_hi_idx, true );
		}
	}
	else if( n64_reg == N64Reg_R0 )
	{
		reg = Ps2Reg_R0;
	}
	else
	{
//		gTotalRegistersUncached++;
		reg = scratch_reg;
		need_load = true;
	}

	if( need_load )
	{
		GetRegisterValue( reg, n64_reg, lo_hi_idx );
	}

	return reg;
}


//	Similar to GetRegisterAndLoad, but ALWAYS loads into the specified psp register

void CCodeGeneratorPS2::LoadRegister( EPs2Reg ps2_reg, EN64Reg n64_reg, u32 lo_hi_idx )
{
	if( mRegisterCache.IsCached( n64_reg, lo_hi_idx ) )
	{
		EPs2Reg	cached_reg( mRegisterCache.GetCachedReg( n64_reg, lo_hi_idx ) );

//		gTotalRegistersCached++;

		// Load the register if it's currently invalid
		if( !mRegisterCache.IsValid( n64_reg,lo_hi_idx ) )
		{
			GetRegisterValue( cached_reg, n64_reg, lo_hi_idx );
			mRegisterCache.MarkAsValid( n64_reg, lo_hi_idx, true );
		}

		// Copy the register if necessary
		if( ps2_reg != cached_reg )
		{
			OR( ps2_reg, cached_reg, Ps2Reg_R0 );
		}
	}
	else if( n64_reg == N64Reg_R0 )
	{
		OR( ps2_reg, Ps2Reg_R0, Ps2Reg_R0 );
	}
	else
	{
//		gTotalRegistersUncached++;
		GetRegisterValue( ps2_reg, n64_reg, lo_hi_idx );
	}
}


//	This function pulls in a cached register so that it can be used at a later point.
//	This is usally done when we have a branching instruction - it guarantees that
//	the register is valid regardless of whether or not the branch is taken.

void	CCodeGeneratorPS2::PrepareCachedRegister( EN64Reg n64_reg, u32 lo_hi_idx )
{
	if( mRegisterCache.IsCached( n64_reg, lo_hi_idx ) )
	{
		EPs2Reg	cached_reg( mRegisterCache.GetCachedReg( n64_reg, lo_hi_idx ) );

		// Load the register if it's currently invalid
		if( !mRegisterCache.IsValid( n64_reg,lo_hi_idx ) )
		{
			GetRegisterValue( cached_reg, n64_reg, lo_hi_idx );
			mRegisterCache.MarkAsValid( n64_reg, lo_hi_idx, true );
		}
	}
}


//

void CCodeGeneratorPS2::StoreRegister( EN64Reg n64_reg, u32 lo_hi_idx, EPs2Reg ps2_reg )
{
	mRegisterCache.ClearKnownValue( n64_reg, lo_hi_idx );

	if( mRegisterCache.IsCached( n64_reg, lo_hi_idx ) )
	{
		EPs2Reg	cached_reg( mRegisterCache.GetCachedReg( n64_reg, lo_hi_idx ) );

//		gTotalRegistersCached++;

		// Update our copy as necessary
		if( ps2_reg != cached_reg )
		{
			OR( cached_reg, ps2_reg, Ps2Reg_R0 );
		}
		mRegisterCache.MarkAsDirty( n64_reg, lo_hi_idx, true );
		mRegisterCache.MarkAsValid( n64_reg, lo_hi_idx, true );
	}
	else
	{
//		gTotalRegistersUncached++;

		SetVar( lo_hi_idx ? &gGPR[ n64_reg ]._u32_1 : &gGPR[ n64_reg ]._u32_0, ps2_reg );

		mRegisterCache.MarkAsDirty( n64_reg, lo_hi_idx, false );
	}
}


//

void CCodeGeneratorPS2::SetRegister64( EN64Reg n64_reg, s32 lo_value, s32 hi_value )
{
	SetRegister( n64_reg, 0, lo_value );
	SetRegister( n64_reg, 1, hi_value );
}


//	Set the low 32 bits of a register to a known value (and hence the upper
//	32 bits are also known though sign extension)

inline void CCodeGeneratorPS2::SetRegister32s( EN64Reg n64_reg, s32 value )
{
	//SetRegister64( n64_reg, value, value >= 0 ? 0 : 0xffffffff );
	SetRegister64( n64_reg, value, value >> 31 );
}


//

inline void CCodeGeneratorPS2::SetRegister( EN64Reg n64_reg, u32 lo_hi_idx, u32 value )
{
	mRegisterCache.SetKnownValue( n64_reg, lo_hi_idx, value );
	mRegisterCache.MarkAsDirty( n64_reg, lo_hi_idx, true );
	if( mRegisterCache.IsCached( n64_reg, lo_hi_idx ) )
	{
		mRegisterCache.MarkAsValid( n64_reg, lo_hi_idx, false );		// The actual cache is invalid though!
	}
}


//

void CCodeGeneratorPS2::UpdateRegister( EN64Reg n64_reg, EPs2Reg ps2_reg, bool options )
{
	//if(n64_reg == N64Reg_R0) return;	//Try to modify R0!!!

	StoreRegisterLo( n64_reg, ps2_reg );

	//Skip storing sign extension on some regs //Corn
	if( N64Reg_DontNeedSign( n64_reg ) ) return;

	if( options == URO_HI_SIGN_EXTEND )
	{
		EPs2Reg scratch_reg = Ps2Reg_V0;
		if( mRegisterCache.IsCached( n64_reg, 1 ) )
		{
			scratch_reg = mRegisterCache.GetCachedReg( n64_reg, 1 );
		}
		SRA( scratch_reg, ps2_reg, 0x1f );		// Sign extend
		StoreRegisterHi( n64_reg, scratch_reg );
	}
	else	// == URO_HI_CLEAR
	{
		SetRegister( n64_reg, 1, 0 );
	}
}


//

EPs2FloatReg	CCodeGeneratorPS2::GetFloatRegisterAndLoad( EN64FloatReg n64_reg )
{
	EPs2FloatReg	ps2_reg = EPs2FloatReg( n64_reg );	// 1:1 mapping
	if( !mRegisterCache.IsFPValid( n64_reg ) )
	{
		GetFloatVar( ps2_reg, &gCPUState.FPU[n64_reg]._f32 );
		mRegisterCache.MarkFPAsValid( n64_reg, true );
	}

	mRegisterCache.MarkFPAsSim( n64_reg, false );

	return ps2_reg;
}


//

inline void CCodeGeneratorPS2::UpdateFloatRegister( EN64FloatReg n64_reg )
{
	mRegisterCache.MarkFPAsDirty( n64_reg, true );
	mRegisterCache.MarkFPAsValid( n64_reg, true );
	mRegisterCache.MarkFPAsSim( n64_reg, false );
}


//Get Double or SimDouble and add code to convert to simulated double if needed //Corn

#define SIMULATESIG 0x1234	//Reduce signature to load value with one OP
EPs2FloatReg	CCodeGeneratorPS2::GetSimFloatRegisterAndLoad( EN64FloatReg n64_reg )
{
	EPs2FloatReg ps2_reg_sig = EPs2FloatReg( n64_reg );	// 1:1 mapping
	EPs2FloatReg ps2_reg = EPs2FloatReg(n64_reg + 1);

	//This is Double Lo or signature
	if( !mRegisterCache.IsFPValid( n64_reg ) )
	{
		GetFloatVar( ps2_reg_sig, &gCPUState.FPU[n64_reg]._f32 );
		mRegisterCache.MarkFPAsValid( n64_reg, true );
	}

	//This is Double Hi or f32
	if( !mRegisterCache.IsFPValid( EN64FloatReg(n64_reg + 1) ) )
	{
		GetFloatVar( ps2_reg, &gCPUState.FPU[n64_reg+1]._f32 );
		mRegisterCache.MarkFPAsValid( EN64FloatReg(n64_reg+1), true );
	}

	//If register is not SimDouble yet or unknown add check and conversion routine
	if( !mRegisterCache.IsFPSim( n64_reg ) )
	{
		MFC1( Ps2Reg_A0, ps2_reg_sig );	//Get lo part of double
		LoadConstant( Ps2Reg_A1, SIMULATESIG );	//Get signature
		CJumpLocation test_reg( BEQ( Ps2Reg_A0, Ps2Reg_A1, CCodeLabel(nullptr), false ) );	//compare float to signature
		MTC1( ps2_reg_sig , Ps2Reg_A1 );	//Always write back signature to float reg

		JAL( CCodeLabel( ( const void * )( _DoubleToFloat ) ), false );	//Convert Double to Float
		MFC1( Ps2Reg_A1, ps2_reg );	//Get hi part of double

		MTC1( ps2_reg , Ps2Reg_V0 ); //store converted float
		PatchJumpLong( test_reg, GetAssemblyBuffer()->GetLabel() );

		mRegisterCache.MarkFPAsDirty( n64_reg, true );
		mRegisterCache.MarkFPAsSim( n64_reg, true );
		mRegisterCache.MarkFPAsDirty( EN64FloatReg(n64_reg + 1), true );
	}

	return ps2_reg;
}


//

inline void CCodeGeneratorPS2::UpdateSimDoubleRegister( EN64FloatReg n64_reg )
{
	mRegisterCache.MarkFPAsValid( n64_reg, true );
	mRegisterCache.MarkFPAsDirty( n64_reg, true );
	mRegisterCache.MarkFPAsSim( n64_reg, true );
	mRegisterCache.MarkFPAsValid( EN64FloatReg(n64_reg + 1), true );
	mRegisterCache.MarkFPAsDirty( EN64FloatReg(n64_reg + 1), true );
}


//

const CN64RegisterCachePS2 & CCodeGeneratorPS2::GetRegisterCacheFromHandle( RegisterSnapshotHandle snapshot ) const
{
	#ifdef DAEDALUS_ENABLE_ASSERTS
	DAEDALUS_ASSERT( snapshot.Handle < mRegisterSnapshots.size(), "Invalid snapshot handle" );
	#endif
	return mRegisterSnapshots[ snapshot.Handle ];
}


//	Flush a specific register back to memory if dirty.
//	Clears the dirty flag and invalidates the contents if specified

void CCodeGeneratorPS2::FlushRegister( CN64RegisterCachePS2 & cache, EN64Reg n64_reg, u32 lo_hi_idx, bool invalidate )
{
	if( cache.IsDirty( n64_reg, lo_hi_idx ) )
	{
		if( cache.IsKnownValue( n64_reg, lo_hi_idx ) )
		{
			s32		known_value( cache.GetKnownValue( n64_reg, lo_hi_idx )._s32 );

			SetVar( lo_hi_idx ? &gGPR[ n64_reg ]._u32_1 : &gGPR[ n64_reg ]._u32_0, known_value );
		}
		else if( cache.IsCached( n64_reg, lo_hi_idx ) )
		{
			#ifdef DAEDALUS_ENABLE_ASSERTS
			DAEDALUS_ASSERT( cache.IsValid( n64_reg, lo_hi_idx ), "Register is dirty but not valid?" );
			#endif
			EPs2Reg	cached_reg( cache.GetCachedReg( n64_reg, lo_hi_idx ) );

			SetVar( lo_hi_idx ? &gGPR[ n64_reg ]._u32_1 : &gGPR[ n64_reg ]._u32_0, cached_reg );
		}
		#ifdef DAEDALUS_DEBUG_CONSOLE
		else
		{
			DAEDALUS_ERROR( "Register is dirty, but not known or cached" );
		}
		#endif
		// We're no longer dirty
		cache.MarkAsDirty( n64_reg, lo_hi_idx, false );
	}

	// Invalidate the register, so we pick up any values the function might have changed
	if( invalidate )
	{
		cache.ClearKnownValue( n64_reg, lo_hi_idx );
		if( cache.IsCached( n64_reg, lo_hi_idx ) )
		{
			cache.MarkAsValid( n64_reg, lo_hi_idx, false );
		}
	}
}


//	This function flushes all dirty registers back to memory
//	If the invalidate flag is set this also invalidates the known value/cached
//	register. This is primarily to ensure that we keep the register set
//	in a consistent set across calls to generic functions. Ideally we need
//	to reimplement generic functions with specialised code to avoid the flush.

void	CCodeGeneratorPS2::FlushAllRegisters( CN64RegisterCachePS2 & cache, bool invalidate )
{
	mFloatCMPIsValid = false;	//invalidate float compare register
	mMultIsValid = false;	//Mult hi/lo are invalid

	// Skip r0
	for( u32 i = 1; i < NUM_N64_REGS; i++ )
	{
		EN64Reg	n64_reg = EN64Reg( i );

		FlushRegister( cache, n64_reg, 0, invalidate );
		FlushRegister( cache, n64_reg, 1, invalidate );
	}

	FlushAllFloatingPointRegisters( cache, invalidate );
}


// Floating-point arguments are placed in $f12-$f15.
// Floating-point return values go into $f0-$f1.
// $f0-$f19 are caller-saved.
// $f20-$f31 are callee-saved.

void	CCodeGeneratorPS2::FlushAllFloatingPointRegisters( CN64RegisterCachePS2 & cache, bool invalidate )
{
	for( u32 i = 0; i < NUM_N64_FP_REGS; i++ )
	{
		EN64FloatReg	n64_reg = EN64FloatReg( i );
		if( cache.IsFPDirty( n64_reg ) )
		{
			#ifdef DAEDALUS_ENABLE_ASSERTS
			DAEDALUS_ASSERT( cache.IsFPValid( n64_reg ), "Register is dirty but not valid?" );
			#endif
			EPs2FloatReg	ps2_reg = EPs2FloatReg( n64_reg );

			SetFloatVar( &gCPUState.FPU[n64_reg]._f32, ps2_reg );

			cache.MarkFPAsDirty( n64_reg, false );
		}

		if( invalidate )
		{
			// Invalidate the register, so we pick up any values the function might have changed
			cache.MarkFPAsValid( n64_reg, false );
			cache.MarkFPAsSim( n64_reg, false );
		}
	}
}


//	This function flushes all dirty *temporary* registers back to memory, and
//	marks them as invalid.

void	CCodeGeneratorPS2::FlushAllTemporaryRegisters( CN64RegisterCachePS2 & cache, bool invalidate )
{
	// Skip r0
	for( u32 i = 1; i < NUM_N64_REGS; i++ )
	{
		EN64Reg	n64_reg = EN64Reg( i );

		if( cache.IsTemporary( n64_reg, 0 ) )
		{
			FlushRegister( cache, n64_reg, 0, invalidate );
		}
		if( cache.IsTemporary( n64_reg, 1 ) )
		{
			FlushRegister( cache, n64_reg, 1, invalidate );
		}

	}

	// XXXX some fp regs are preserved across function calls?
	FlushAllFloatingPointRegisters( cache, invalidate );
}


//

void	CCodeGeneratorPS2::RestoreAllRegisters( CN64RegisterCachePS2 & current_cache, CN64RegisterCachePS2 & new_cache )
{
	// Skip r0
	for( u32 i = 1; i < NUM_N64_REGS; i++ )
	{
		EN64Reg	n64_reg = EN64Reg( i );

		if( new_cache.IsValid( n64_reg, 0 ) && !current_cache.IsValid( n64_reg, 0 ) )
		{
			GetVar( new_cache.GetCachedReg( n64_reg, 0 ), &gGPR[ n64_reg ]._u32_0 );
		}
		if( new_cache.IsValid( n64_reg, 1 ) && !current_cache.IsValid( n64_reg, 1 ) )
		{
			GetVar( new_cache.GetCachedReg( n64_reg, 1 ), &gGPR[ n64_reg ]._u32_1 );
		}
	}

	// XXXX some fp regs are preserved across function calls?
	for( u32 i = 0; i < NUM_N64_FP_REGS; ++i )
	{
		EN64FloatReg	n64_reg = EN64FloatReg( i );
		if( new_cache.IsFPValid( n64_reg ) && !current_cache.IsFPValid( n64_reg ) )
		{
			EPs2FloatReg	ps2_reg = EPs2FloatReg( n64_reg );

			GetFloatVar( ps2_reg, &gCPUState.FPU[n64_reg]._f32 );
		}
	}
}


//

CJumpLocation CCodeGeneratorPS2::GenerateExitCode( u32 exit_address, u32 jump_address, u32 num_instructions, CCodeLabel next_fragment )
{
	#ifdef DAEDALUS_ENABLE_ASSERTS
	//DAEDALUS_ASSERT( exit_address != u32( ~0 ), "Invalid exit address" );
	DAEDALUS_ASSERT( !next_fragment.IsSet() || jump_address == 0, "Shouldn't be specifying a jump address if we have a next fragment?" );
	#endif
	if( (exit_address == mEntryAddress) & mLoopTop.IsSet() )
	{
		#ifdef DAEDALUS_ENABLE_ASSERTS
		DAEDALUS_ASSERT( mUseFixedRegisterAllocation, "Have mLoopTop but unfixed register allocation?" );
		#endif
		FlushAllFloatingPointRegisters( mRegisterCache, false );

		// Check if we're ok to continue, without flushing any registers
		GetVar( Ps2Reg_V0, &gCPUState.CPUControl[C0_COUNT]._u32 );
		GetVar( Ps2Reg_A0, (const u32*)&gCPUState.Events[0].mCount );

		//
		//	Pull in any registers which may have been flushed for whatever reason.
		//
		// Skip r0
		for( u32 i = 1; i < NUM_N64_REGS; i++ )
		{
			EN64Reg	n64_reg = EN64Reg( i );

			//if( mRegisterCache.IsDirty( n64_reg, 0 ) & mRegisterCache.IsKnownValue( n64_reg, 0 ) )
			//{
			//	FlushRegister( mRegisterCache, n64_reg, 0, false );
			//}
			//if( mRegisterCache.IsDirty( n64_reg, 1 ) & mRegisterCache.IsKnownValue( n64_reg, 1 ) )
			//{
			//	FlushRegister( mRegisterCache, n64_reg, 1, false );
			//}

			PrepareCachedRegister( n64_reg, 0 );
			PrepareCachedRegister( n64_reg, 1 );
		}

		// Assuming we don't need to set CurrentPC/Delay flags before we branch to the top..
		//
		ADDIU( Ps2Reg_V0, Ps2Reg_V0, num_instructions );
		SetVar( &gCPUState.CPUControl[C0_COUNT]._u32, Ps2Reg_V0 );

		//
		//	If the event counter is still positive, just jump directly to the top of our loop
		//
		ADDIU( Ps2Reg_A0, Ps2Reg_A0, -s16(num_instructions) );
		BGTZ( Ps2Reg_A0, mLoopTop, false );
		SetVar( (u32*)&gCPUState.Events[0].mCount, Ps2Reg_A0 );	// ASSUMES store is done in just a single op.

		FlushAllRegisters( mRegisterCache, true );

		SetVar( &gCPUState.CurrentPC, exit_address );
		JAL( CCodeLabel( ( const void * )( CPU_HANDLE_COUNT_INTERRUPT ) ), false );
		SetVar( &gCPUState.Delay, NO_DELAY );	// ASSUMES store is done in just a single op.

		J( CCodeLabel( ( const void * )( _ReturnFromDynaRec ) ), true );

		//
		//	Return an invalid jump location to indicate we've handled our own linking.
		//
		return CJumpLocation( nullptr );
	}

	FlushAllRegisters( mRegisterCache, true );

	CCodeLabel	exit_routine;

	// Use the branch delay slot to load the second argument
	Ps2OpCode	op1;
	Ps2OpCode	op2;

	if( jump_address != 0 )
	{
		LoadConstant( Ps2Reg_A0, num_instructions );
		LoadConstant( Ps2Reg_A1, exit_address );
		GetLoadConstantOps( Ps2Reg_A2, jump_address, &op1, &op2 );

		exit_routine = CCodeLabel( ( const void * )( _DirectExitCheckDelay ) );
	}
	else
	{
		LoadConstant( Ps2Reg_A0, num_instructions );
		GetLoadConstantOps( Ps2Reg_A1, exit_address, &op1, &op2 );

		exit_routine = CCodeLabel( ( const void * )( _DirectExitCheckNoDelay ) );
	}

	if( op2._u32 == 0 )
	{
		JAL( exit_routine, false );		// No branch delay
		AppendOp( op1 );
	}
	else
	{
		AppendOp( op1 );
		JAL( exit_routine, false );		// No branch delay
		AppendOp( op2 );
	}

	// This jump may be nullptr, in which case we patch it below
	// This gets patched with a jump to the next fragment if the target is later found
	CJumpLocation jump_to_next_fragment( J( next_fragment, true ) );

	// Patch up the exit jump if the target hasn't been compiled yet
	if( !next_fragment.IsSet() )
	{
		PatchJumpLong( jump_to_next_fragment, CCodeLabel( ( const void * )( _ReturnFromDynaRec ) ) );
	}
	return jump_to_next_fragment;

}


// Handle branching back to the interpreter after an ERET

void CCodeGeneratorPS2::GenerateEretExitCode( u32 num_instructions, CIndirectExitMap * p_map )
{
	FlushAllRegisters( mRegisterCache, false );

	// Eret is a bit bodged so we exit at PC + 4
	LoadConstant( Ps2Reg_A0, num_instructions );
	LoadConstant( Ps2Reg_A1, reinterpret_cast< s32 >( p_map ) );
	GetVar( Ps2Reg_A2, &gCPUState.CurrentPC );

	J( CCodeLabel( ( const void * )( _IndirectExitCheck ) ), false );
	ADDIU( Ps2Reg_A2, Ps2Reg_A2, 4 );		// Delay slot
}


// Handle branching back to the interpreter after an indirect jump

void CCodeGeneratorPS2::GenerateIndirectExitCode( u32 num_instructions, CIndirectExitMap * p_map )
{
	FlushAllRegisters( mRegisterCache, false );

	// New return address is in gCPUState.TargetPC
	Ps2OpCode op1, op2;
	GetLoadConstantOps(Ps2Reg_A0, num_instructions, &op1, &op2);
	LoadConstant( Ps2Reg_A1, reinterpret_cast< s32 >( p_map ) );
	GetVar( Ps2Reg_A2, &gCPUState.TargetPC );

	if (op2._u32 == 0)
	{
		J( CCodeLabel( ( const void * )( _IndirectExitCheck ) ), false );
		AppendOp(op1);
	}
	else
	{
		AppendOp(op1);
		J( CCodeLabel( ( const void * )( _IndirectExitCheck ) ), false );
		AppendOp(op2);
	}
}


//

void	CCodeGeneratorPS2::GenerateBranchHandler( CJumpLocation branch_handler_jump, RegisterSnapshotHandle snapshot )
{
	#ifdef DAEDALUS_ENABLE_ASSERTS
	DAEDALUS_ASSERT( branch_handler_jump.IsSet(), "Why is the branch handler jump not set?" );
	#endif
	CCodeGeneratorPS2::SetBufferA();
	CCodeLabel	current_label( GetAssemblyBuffer()->GetLabel() );

	PatchJumpLong( branch_handler_jump, current_label );

	mRegisterCache = GetRegisterCacheFromHandle( snapshot );

	CJumpLocation	jump_to_b( J( CCodeLabel( nullptr ), true ) );
	CCodeGeneratorPS2::SetBufferB();
	current_label = GetAssemblyBuffer()->GetLabel();
	PatchJumpLong( jump_to_b, current_label );

}


//

CJumpLocation	CCodeGeneratorPS2::GenerateBranchAlways( CCodeLabel target )
{
	return J( target, true );
}


//

CJumpLocation	CCodeGeneratorPS2::GenerateBranchIfSet( const u32 * p_var, CCodeLabel target )
{
	GetVar( Ps2Reg_V0, p_var );
	return BNE( Ps2Reg_V0, Ps2Reg_R0, target, true );
}


//

CJumpLocation	CCodeGeneratorPS2::GenerateBranchIfNotSet( const u32 * p_var, CCodeLabel target )
{
	GetVar( Ps2Reg_V0, p_var );
	return BEQ( Ps2Reg_V0, Ps2Reg_R0, target, true );
}


//

CJumpLocation	CCodeGeneratorPS2::GenerateBranchIfEqual( const u32 * p_var, u32 value, CCodeLabel target )
{
	GetVar( Ps2Reg_V0, p_var );
	LoadConstant( Ps2Reg_A0, value );
	return BEQ( Ps2Reg_V0, Ps2Reg_A0, target, true );
}


//

CJumpLocation	CCodeGeneratorPS2::GenerateBranchIfNotEqual( const u32 * p_var, u32 value, CCodeLabel target )
{
	GetVar( Ps2Reg_V0, p_var );
	LoadConstant( Ps2Reg_A0, value );
	return BNE( Ps2Reg_V0, Ps2Reg_A0, target, true );
}


//

CJumpLocation	CCodeGeneratorPS2::GenerateBranchIfNotEqual( EPs2Reg reg_a, u32 value, CCodeLabel target )
{
	EPs2Reg	reg_b( reg_a == Ps2Reg_V0 ? Ps2Reg_A0 : Ps2Reg_V0 );		// Make sure we don't use the same reg

	LoadConstant( reg_b, value );
	return BNE( reg_a, reg_b, target, true );
}


//

void	CCodeGeneratorPS2::SetVar( u32 * p_var, EPs2Reg reg_src )
{
	//DAEDALUS_ASSERT( reg_src != Ps2Reg_AT, "Whoops, splattering source register" );

	EPs2Reg		reg_base;
	s16			base_offset;
	GetBaseRegisterAndOffset( p_var, &reg_base, &base_offset );

	SW( reg_src, reg_base, base_offset );
}


//

void	CCodeGeneratorPS2::SetVar( u32 * p_var, u32 value )
{
	EPs2Reg		reg_value;

	if(value == 0)
	{
		reg_value = Ps2Reg_R0;
	}
	else
	{
		LoadConstant( Ps2Reg_V0, value );
		reg_value = Ps2Reg_V0;
	}

	EPs2Reg		reg_base;
	s16			base_offset;
	GetBaseRegisterAndOffset( p_var, &reg_base, &base_offset );

	SW( reg_value, reg_base, base_offset );
}


//

void	CCodeGeneratorPS2::SetFloatVar( f32 * p_var, EPs2FloatReg reg_src )
{
	EPs2Reg		reg_base;
	s16			base_offset;
	GetBaseRegisterAndOffset( p_var, &reg_base, &base_offset );

	SWC1( reg_src, reg_base, base_offset );
}


//

void	CCodeGeneratorPS2::GetVar( EPs2Reg dst_reg, const u32 * p_var )
{
	EPs2Reg		reg_base;
	s16			base_offset;
	GetBaseRegisterAndOffset( p_var, &reg_base, &base_offset );

	LW( dst_reg, reg_base, base_offset );
}


//

void	CCodeGeneratorPS2::GetFloatVar( EPs2FloatReg dst_reg, const f32 * p_var )
{
	EPs2Reg		reg_base;
	s16			base_offset;
	GetBaseRegisterAndOffset( p_var, &reg_base, &base_offset );

	LWC1( dst_reg, reg_base, base_offset );
}


//

void	CCodeGeneratorPS2::GetBaseRegisterAndOffset( const void * p_address, EPs2Reg * p_reg, s16 * p_offset )
{
	s32		base_pointer_offset( reinterpret_cast< const u8 * >( p_address ) - mpBasePointer );
	if( (base_pointer_offset > SHRT_MIN) & (base_pointer_offset < SHRT_MAX) )
	{
		*p_reg = mBaseRegister;
		*p_offset = base_pointer_offset;
	}
	else
	{
		u32		address( reinterpret_cast< u32 >( p_address ) );
		u16		hi_bits( (u16)( address >> 16 ) );
		u16		lo_bits( (u16)( address ) );

		//
		//	Move up
		//
		/*if(lo_bits >= 0x8000)
		{
			hi_bits++;
		}*/

		hi_bits += lo_bits >> 15;

		s32		long_offset( address - ((s32)hi_bits<<16) );
		#ifdef DAEDALUS_ENABLE_ASSERTS
		DAEDALUS_ASSERT( long_offset >= SHRT_MIN && long_offset <= SHRT_MAX, "Offset is out of range!" );
		#endif
		s16		offset( (s16)long_offset );
		#ifdef DAEDALUS_ENABLE_ASSERTS
		DAEDALUS_ASSERT( ((s32)hi_bits<<16) + offset == (s32)address, "Incorrect address calculation" );
		#endif
		LUI( Ps2Reg_A0, hi_bits );
		*p_reg = Ps2Reg_A0;
		*p_offset = offset;
	}
}


//

/*void	CCodeGeneratorPS2::UpdateAddressAndDelay( u32 address, bool set_branch_delay )
{
	DAEDALUS_ASSERT( !set_branch_delay || (address != 0), "Have branch delay and no address?" );

	if( set_branch_delay )
	{
		SetVar( &gCPUState.Delay, EXEC_DELAY );
		mBranchDelaySet = true;
	}
	//else
	//{
	//	SetVar( &gCPUState.Delay, NO_DELAY );
	//}
	if( address != 0 )
	{
		SetVar( &gCPUState.CurrentPC, address );
	}
}*/


//	Generates instruction handler for the specified op code.
//	Returns a jump location if an exception handler is required

CJumpLocation	CCodeGeneratorPS2::GenerateOpCode( const STraceEntry& ti, bool branch_delay_slot, const SBranchDetails * p_branch, CJumpLocation * p_branch_jump )
{
	#ifdef DAEDALUS_PROFILE
	DAEDALUS_PROFILE( "CCodeGeneratorPS2::GenerateOpCode" );
	#endif
	u32 address = ti.Address;
	OpCode op_code = ti.OpCode;
	bool	handled( false );
	bool	is_nop( op_code._u32 == 0 );

	if( is_nop )
	{
		if( branch_delay_slot )
		{
			SetVar( &gCPUState.Delay, NO_DELAY );
		}
		return CJumpLocation();
	}

	if( branch_delay_slot ) mPreviousStoreBase = mPreviousLoadBase = N64Reg_R0;	//Invalidate

	mQuickLoad = ti.Usage.Access8000;

	const EN64Reg	rs = EN64Reg( op_code.rs );
	const EN64Reg	rt = EN64Reg( op_code.rt );
	const EN64Reg	rd = EN64Reg( op_code.rd );
	const EN64Reg	base = EN64Reg( op_code.base );
	const u32		ft = op_code.ft;
	const u32		sa = op_code.sa;
	//const u32		jump_target( (address&0xF0000000) | (op_code.target<<2) );
	//const u32		branch_target( address + ( ((s32)(s16)op_code.immediate)<<2 ) + 4);

	//
	//	Look for opcodes we can handle manually
	//
	switch( op_code.op )
	{
	case OP_SPECOP:
		switch( op_code.spec_op )
		{
		case SpecOp_SLL:	GenerateSLL( rd, rt, sa );	handled = true; break;
		case SpecOp_SRA:	GenerateSRA( rd, rt, sa );	handled = true; break;
		case SpecOp_SRL:	GenerateSRL( rd, rt, sa );	handled = true; break;
		case SpecOp_SLLV:	GenerateSLLV( rd, rs, rt );	handled = true; break;
		case SpecOp_SRAV:	GenerateSRAV( rd, rs, rt );	handled = true; break;
		case SpecOp_SRLV:	GenerateSRLV( rd, rs, rt );	handled = true; break;

		case SpecOp_JR:		GenerateJR( rs, p_branch, p_branch_jump );	handled = true; break;
		case SpecOp_JALR:	GenerateJALR( rs, rd, address, p_branch, p_branch_jump );	handled = true; break;

		case SpecOp_MFLO:	GenerateMFLO( rd );			handled = true; break;
		case SpecOp_MFHI:	GenerateMFHI( rd );			handled = true; break;
		case SpecOp_MTLO:	GenerateMTLO( rd );			handled = true; break;
		case SpecOp_MTHI:	GenerateMTHI( rd );			handled = true; break;

		case SpecOp_MULT:	GenerateMULT( rs, rt );		handled = true; break;
		case SpecOp_MULTU:	GenerateMULTU( rs, rt );	handled = true; break;
		case SpecOp_DIV:	GenerateDIV( rs, rt );		handled = true; break;
		case SpecOp_DIVU:	GenerateDIVU( rs, rt );		handled = true; break;

		case SpecOp_DMULT:	GenerateDMULT( rs, rt );	handled = true; break;
		case SpecOp_DMULTU:	GenerateDMULTU( rs, rt );	handled = true; break;
		case SpecOp_DDIVU:	GenerateDDIVU( rs, rt );	handled = true; break;
		case SpecOp_DDIV:	GenerateDDIV( rs, rt );	handled = true; break;

		case SpecOp_ADD:	GenerateADDU( rd, rs, rt );	handled = true; break;
		case SpecOp_ADDU:	GenerateADDU( rd, rs, rt );	handled = true; break;
		case SpecOp_SUB:	GenerateSUBU( rd, rs, rt );	handled = true; break;
		case SpecOp_SUBU:	GenerateSUBU( rd, rs, rt );	handled = true; break;

		case SpecOp_AND:	GenerateAND( rd, rs, rt );	handled = true; break;
		case SpecOp_OR:		GenerateOR( rd, rs, rt );	handled = true; break;
		case SpecOp_XOR:	GenerateXOR( rd, rs, rt );	handled = true; break;
		case SpecOp_NOR:	GenerateNOR( rd, rs, rt );	handled = true; break;

		case SpecOp_SLT:	GenerateSLT( rd, rs, rt );	handled = true; break;
		case SpecOp_SLTU:	GenerateSLTU( rd, rs, rt );	handled = true; break;

		case SpecOp_DADD:	GenerateDADDU( rd, rs, rt );	handled = true; break;
		case SpecOp_DADDU:	GenerateDADDU( rd, rs, rt );	handled = true; break;

		case SpecOp_DSRA32:	GenerateDSRA32( rd, rt, sa );	handled = true; break;
		case SpecOp_DSRA:	GenerateDSRA( rd, rt, sa );	handled = true; break;
		case SpecOp_DSLL32:	GenerateDSLL32( rd, rt, sa );	handled = true; break;
		case SpecOp_DSLL:	GenerateDSLL( rd, rt, sa );	handled = true; break;
		default:
			break;
		}
		break;

	case OP_REGIMM:
		switch( op_code.regimm_op )
		{
				// These can be handled by the same Generate function, as the 'likely' bit is handled elsewhere
		case RegImmOp_BLTZ:
		case RegImmOp_BLTZL:
			GenerateBLTZ( rs, p_branch, p_branch_jump ); handled = true; break;

		case RegImmOp_BGEZ:
		case RegImmOp_BGEZL:
			GenerateBGEZ( rs, p_branch, p_branch_jump ); handled = true; break;
		}
		break;

	case OP_J:			/* nothing to do */		handled = true; break;
	case OP_JAL:		GenerateJAL( address );	handled = true; break;

	case OP_BEQ:		GenerateBEQ( rs, rt, p_branch, p_branch_jump ); handled = true; break;
	case OP_BEQL:		GenerateBEQ( rs, rt, p_branch, p_branch_jump ); handled = true; break;
	case OP_BNE:		GenerateBNE( rs, rt, p_branch, p_branch_jump ); handled = true; break;
	case OP_BNEL:		GenerateBNE( rs, rt, p_branch, p_branch_jump ); handled = true; break;
	case OP_BLEZ:		GenerateBLEZ( rs, p_branch, p_branch_jump ); handled = true; break;
	case OP_BLEZL:		GenerateBLEZ( rs, p_branch, p_branch_jump ); handled = true; break;
	case OP_BGTZ:		GenerateBGTZ( rs, p_branch, p_branch_jump ); handled = true; break;
	case OP_BGTZL:		GenerateBGTZ( rs, p_branch, p_branch_jump ); handled = true; break;

	case OP_ADDI:		GenerateADDIU( rt, rs, s16( op_code.immediate ) );	handled = true; break;
	case OP_ADDIU:		GenerateADDIU( rt, rs, s16( op_code.immediate ) );	handled = true; break;
	case OP_SLTI:		GenerateSLTI( rt, rs, s16( op_code.immediate ) );	handled = true; break;
	case OP_SLTIU:		GenerateSLTIU( rt, rs, s16( op_code.immediate ) );	handled = true; break;

	case OP_DADDI:		GenerateDADDIU( rt, rs, s16( op_code.immediate ) );	handled = true; break;
	case OP_DADDIU:		GenerateDADDIU( rt, rs, s16( op_code.immediate ) );	handled = true; break;

	case OP_ANDI:		GenerateANDI( rt, rs, op_code.immediate );			handled = true; break;
	case OP_ORI:		GenerateORI( rt, rs, op_code.immediate );			handled = true; break;
	case OP_XORI:		GenerateXORI( rt, rs, op_code.immediate );			handled = true; break;
	case OP_LUI:		GenerateLUI( rt, s16( op_code.immediate ) );		handled = true; break;

	case OP_LB:			GenerateLB( address, branch_delay_slot, rt, base, s16( op_code.immediate ) );	handled = true; break;
	case OP_LBU:		GenerateLBU( address, branch_delay_slot, rt, base, s16( op_code.immediate ) );	handled = true; break;
	case OP_LH:			GenerateLH( address, branch_delay_slot, rt, base, s16( op_code.immediate ) );	handled = true; break;
	case OP_LHU:		GenerateLHU( address, branch_delay_slot, rt, base, s16( op_code.immediate ) );	handled = true; break;
	case OP_LW:			GenerateLW( address, branch_delay_slot, rt, base, s16( op_code.immediate ) );	handled = true; break;
	case OP_LD:			GenerateLD( address, branch_delay_slot, rt, base, s16( op_code.immediate ) );	handled = true; break;
	case OP_LWC1:		GenerateLWC1( address, branch_delay_slot, ft, base, s16( op_code.immediate ) );	handled = true; break;
#ifdef ENABLE_LDC1
	case OP_LDC1:		if( !branch_delay_slot & gMemoryAccessOptimisation & mQuickLoad ) { GenerateLDC1( address, branch_delay_slot, ft, base, s16( op_code.immediate ) );	handled = true; } break;
#endif

#ifdef ENABLE_LWR_LWL
	case OP_LWL:		GenerateLWL( address, branch_delay_slot, rt, base, s16( op_code.immediate ) );	handled = true; break;
	case OP_LWR:		GenerateLWR( address, branch_delay_slot, rt, base, s16( op_code.immediate ) );	handled = true; break;
#endif

	case OP_SB:			GenerateSB( address, branch_delay_slot, rt, base, s16( op_code.immediate ) );	handled = true; break;
	case OP_SH:			GenerateSH( address, branch_delay_slot, rt, base, s16( op_code.immediate ) );	handled = true; break;
	case OP_SW:			GenerateSW( address, branch_delay_slot, rt, base, s16( op_code.immediate ) );	handled = true; break;
	case OP_SD:			GenerateSD( address, branch_delay_slot, rt, base, s16( op_code.immediate ) );	handled = true; break;
	case OP_SWC1:		GenerateSWC1( address, branch_delay_slot, ft, base, s16( op_code.immediate ) );	handled = true; break;
#ifdef ENABLE_SDC1
	case OP_SDC1:		if( !branch_delay_slot & gMemoryAccessOptimisation & mQuickLoad ) { GenerateSDC1( address, branch_delay_slot, ft, base, s16( op_code.immediate ) );	handled = true; } break;
#endif

#ifdef ENABLE_SWR_SWL
	case OP_SWL:		GenerateSWL( address, branch_delay_slot, rt, base, s16( op_code.immediate ) );	handled = true; break;
	case OP_SWR:		GenerateSWR( address, branch_delay_slot, rt, base, s16( op_code.immediate ) );	handled = true; break;
#endif

	case OP_CACHE:		GenerateCACHE( base, op_code.immediate, rt ); handled = true; break;

	case OP_COPRO0:
		switch( op_code.cop0_op )
		{
		case Cop0Op_MFC0:	GenerateMFC0( rt, op_code.fs ); handled = true; break;
		//case Cop0Op_MTC0:	GenerateMTC0( rt, op_code.fs ); handled = true; break;	//1080 deg has issues with this
		default:
			break;
		}
		break;

	case OP_COPRO1:
		switch( op_code.cop1_op )
		{
		case Cop1Op_MFC1:	GenerateMFC1( rt, op_code.fs ); handled = true; break;
		case Cop1Op_MTC1:	GenerateMTC1( op_code.fs, rt ); handled = true; break;

		case Cop1Op_CFC1:	GenerateCFC1( rt, op_code.fs ); handled = true; break;
		case Cop1Op_CTC1:	GenerateCTC1( op_code.fs, rt ); handled = true; break;


		case Cop1Op_DInstr:
			if( gDynarecDoublesOptimisation )
			{
				switch( op_code.cop1_funct )
				{
				case Cop1OpFunc_ADD:	GenerateADD_D_Sim( op_code.fd, op_code.fs, op_code.ft ); handled = true; break;
				case Cop1OpFunc_SUB:	GenerateSUB_D_Sim( op_code.fd, op_code.fs, op_code.ft ); handled = true; break;
				case Cop1OpFunc_MUL:	GenerateMUL_D_Sim( op_code.fd, op_code.fs, op_code.ft ); handled = true; break;
				case Cop1OpFunc_DIV:	GenerateDIV_D_Sim( op_code.fd, op_code.fs, op_code.ft ); handled = true; break;
				case Cop1OpFunc_SQRT:	GenerateSQRT_D_Sim( op_code.fd, op_code.fs ); handled = true; break;
				case Cop1OpFunc_ABS:	GenerateABS_D_Sim( op_code.fd, op_code.fs ); handled = true; break;
				case Cop1OpFunc_MOV:	GenerateMOV_D_Sim( op_code.fd, op_code.fs ); handled = true; break;
				case Cop1OpFunc_NEG:	GenerateNEG_D_Sim( op_code.fd, op_code.fs ); handled = true; break;

				//case Cop1OpFunc_TRUNC_W:	GenerateTRUNC_W_D_Sim( op_code.fd, op_code.fs ); handled = true; break;

				case Cop1OpFunc_CVT_W:		GenerateCVT_W_D_Sim( op_code.fd, op_code.fs ); handled = true; break;
				case Cop1OpFunc_CVT_S:		GenerateCVT_S_D_Sim( op_code.fd, op_code.fs ); handled = true; break;

				case Cop1OpFunc_CMP_F:		GenerateCMP_D_Sim( op_code.fs, Cop1OpFunc_CMP_F, op_code.ft ); handled = true; break;
				//case Cop1OpFunc_CMP_UN:	GenerateCMP_D_Sim( op_code.fs, Cop1OpFunc_CMP_UN, op_code.ft ); handled = true; break;
				case Cop1OpFunc_CMP_EQ:		GenerateCMP_D_Sim( op_code.fs, Cop1OpFunc_CMP_EQ, op_code.ft ); handled = true; break;	//Conker has issues with this
				//case Cop1OpFunc_CMP_UEQ:	GenerateCMP_D_Sim( op_code.fs, Cop1OpFunc_CMP_UEQ, op_code.ft ); handled = true; break;
				case Cop1OpFunc_CMP_OLT:	GenerateCMP_D_Sim( op_code.fs, Cop1OpFunc_CMP_OLT, op_code.ft ); handled = true; break;
				//case Cop1OpFunc_CMP_ULT:	GenerateCMP_D_Sim( op_code.fs, Cop1OpFunc_CMP_ULT, op_code.ft ); handled = true; break;
				case Cop1OpFunc_CMP_OLE:	GenerateCMP_D_Sim( op_code.fs, Cop1OpFunc_CMP_OLE, op_code.ft ); handled = true; break;
				//case Cop1OpFunc_CMP_ULE:	GenerateCMP_D_Sim( op_code.fs, Cop1OpFunc_CMP_ULE, op_code.ft ); handled = true; break;

				case Cop1OpFunc_CMP_SF:		GenerateCMP_D_Sim( op_code.fs, Cop1OpFunc_CMP_F, op_code.ft ); handled = true; break;
				//case Cop1OpFunc_CMP_NGLE:	GenerateCMP_D_Sim( op_code.fs, Cop1OpFunc_CMP_NGLE, op_code.ft ); handled = true; break;
				case Cop1OpFunc_CMP_SEQ:	GenerateCMP_D_Sim( op_code.fs, Cop1OpFunc_CMP_EQ, op_code.ft ); handled = true; break;
				//case Cop1OpFunc_CMP_NGL:	GenerateCMP_D_Sim( op_code.fs, Cop1OpFunc_CMP_NGL, op_code.ft ); handled = true; break;
				case Cop1OpFunc_CMP_LT:		GenerateCMP_D_Sim( op_code.fs, Cop1OpFunc_CMP_OLT, op_code.ft ); handled = true; break;
				//case Cop1OpFunc_CMP_NGE:	GenerateCMP_D_Sim( op_code.fs, Cop1OpFunc_CMP_NGE, op_code.ft ); handled = true; break;
				case Cop1OpFunc_CMP_LE:		GenerateCMP_D_Sim( op_code.fs, Cop1OpFunc_CMP_OLE, op_code.ft ); handled = true; break;
				//case Cop1OpFunc_CMP_NGT:	GenerateCMP_D_Sim( op_code.fs, Cop1OpFunc_CMP_NGT, op_code.ft ); handled = true; break;
				default:
					break;	//call Generic4300
				}
			}
			else
			{
				GenerateGenericR4300( op_code, R4300_GetDInstructionHandler( op_code ) ); handled = true; break;	// Need branch delay?
			}
			break;

		case Cop1Op_SInstr:
			switch( op_code.cop1_funct )
			{
			case Cop1OpFunc_ADD:	GenerateADD_S( op_code.fd, op_code.fs, op_code.ft ); handled = true; break;
			case Cop1OpFunc_SUB:	GenerateSUB_S( op_code.fd, op_code.fs, op_code.ft ); handled = true; break;
			case Cop1OpFunc_MUL:	GenerateMUL_S( op_code.fd, op_code.fs, op_code.ft ); handled = true; break;
			case Cop1OpFunc_DIV:	GenerateDIV_S( op_code.fd, op_code.fs, op_code.ft ); handled = true; break;
			case Cop1OpFunc_SQRT:	GenerateSQRT_S( op_code.fd, op_code.fs ); handled = true; break;
			case Cop1OpFunc_ABS:	GenerateABS_S( op_code.fd, op_code.fs ); handled = true; break;
			case Cop1OpFunc_MOV:	GenerateMOV_S( op_code.fd, op_code.fs ); handled = true; break;
			case Cop1OpFunc_NEG:	GenerateNEG_S( op_code.fd, op_code.fs ); handled = true; break;

			//case Cop1OpFunc_TRUNC_W:	GenerateTRUNC_W_S( op_code.fd, op_code.fs ); handled = true; break;
			//case Cop1OpFunc_FLOOR_W:	GenerateFLOOR_W_S( op_code.fd, op_code.fs ); handled = true; break;

			case Cop1OpFunc_CVT_W:		GenerateCVT_W_S( op_code.fd, op_code.fs ); handled = true; break;
			case Cop1OpFunc_CVT_D:		if( gDynarecDoublesOptimisation & !g_ROM.DISABLE_SIM_CVT_D_S ) GenerateCVT_D_S_Sim( op_code.fd, op_code.fs );	//Sim has issues with EWJ/Tom&Jerry/PowerPuffGirls
										else GenerateCVT_D_S( op_code.fd, op_code.fs );
										handled = true;
										break;

			case Cop1OpFunc_CMP_F:		GenerateCMP_S( op_code.fs, Cop1OpFunc_CMP_F, op_code.ft ); handled = true; break;
			//case Cop1OpFunc_CMP_UN:	GenerateCMP_S( op_code.fs, Cop1OpFunc_CMP_UN, op_code.ft ); handled = true; break;
			case Cop1OpFunc_CMP_EQ:		GenerateCMP_S( op_code.fs, Cop1OpFunc_CMP_EQ, op_code.ft ); handled = true; break;
			//case Cop1OpFunc_CMP_UEQ:	GenerateCMP_S( op_code.fs, Cop1OpFunc_CMP_UEQ, op_code.ft ); handled = true; break;
			case Cop1OpFunc_CMP_OLT:	GenerateCMP_S( op_code.fs, Cop1OpFunc_CMP_OLT, op_code.ft ); handled = true; break;
			//case Cop1OpFunc_CMP_ULT:	GenerateCMP_S( op_code.fs, Cop1OpFunc_CMP_ULT, op_code.ft ); handled = true; break;
			case Cop1OpFunc_CMP_OLE:	GenerateCMP_S( op_code.fs, Cop1OpFunc_CMP_OLE, op_code.ft ); handled = true; break;
			//case Cop1OpFunc_CMP_ULE:	GenerateCMP_S( op_code.fs, Cop1OpFunc_CMP_ULE, op_code.ft ); handled = true; break;

			case Cop1OpFunc_CMP_SF:		GenerateCMP_S( op_code.fs, Cop1OpFunc_CMP_F, op_code.ft ); handled = true; break;
			//case Cop1OpFunc_CMP_NGLE:	GenerateCMP_S( op_code.fs, Cop1OpFunc_CMP_NGLE, op_code.ft ); handled = true; break;
			case Cop1OpFunc_CMP_SEQ:	GenerateCMP_S( op_code.fs, Cop1OpFunc_CMP_EQ, op_code.ft ); handled = true; break;
			//case Cop1OpFunc_CMP_NGL:	GenerateCMP_S( op_code.fs, Cop1OpFunc_CMP_NGL, op_code.ft ); handled = true; break;
			case Cop1OpFunc_CMP_LT:		GenerateCMP_S( op_code.fs, Cop1OpFunc_CMP_OLT, op_code.ft ); handled = true; break;
			//case Cop1OpFunc_CMP_NGE:	GenerateCMP_S( op_code.fs, Cop1OpFunc_CMP_NGE, op_code.ft ); handled = true; break;
			case Cop1OpFunc_CMP_LE:		GenerateCMP_S( op_code.fs, Cop1OpFunc_CMP_OLE, op_code.ft ); handled = true; break;
			//This breaks D64, but I think is a bug somewhere else since interpreter trows fp nan exception in Cop1_S_NGT
			//case Cop1OpFunc_CMP_NGT:	if(g_ROM.GameHacks != DK64)	{GenerateCMP_S( op_code.fs, Cop1OpFunc_CMP_NGT, op_code.ft ); handled = true;} break;
			}
			break;

		case Cop1Op_WInstr:
			switch( op_code.cop1_funct )
			{
			case Cop1OpFunc_CVT_S:	GenerateCVT_S_W( op_code.fd, op_code.fs ); handled = true; break;
			case Cop1OpFunc_CVT_D:	if( gDynarecDoublesOptimisation ) { GenerateCVT_D_W_Sim( op_code.fd, op_code.fs ); handled = true; } break;
			}
			break;

		case Cop1Op_BCInstr:
			switch( op_code.cop1_bc )
			{
				// These can be handled by the same Generate function, as the 'likely' bit is handled elsewhere
			case Cop1BCOp_BC1F:
			case Cop1BCOp_BC1FL:
				GenerateBC1F( p_branch, p_branch_jump ); handled = true; break;
			case Cop1BCOp_BC1T:
			case Cop1BCOp_BC1TL:
				GenerateBC1T( p_branch, p_branch_jump ); handled = true; break;
			}
			break;

		default:
			break;
		}
		break;
	}

	//Invalidate load/store base regs if they has been modified with current OPcode //Corn
	if( (ti.Usage.RegWrites >> mPreviousLoadBase) & 1 ) mPreviousLoadBase = N64Reg_R0;	//Invalidate
	if( (ti.Usage.RegWrites >> mPreviousStoreBase) & 1 ) mPreviousStoreBase = N64Reg_R0;	//Invalidate

	//	Default handling - call interpreting function
	//
	if( !handled )
	{

#if 0
//1->Show not handled OP codes (Require that DAEDALUS_SILENT flag is undefined)
	  // Note: Cop1Op_DInstr are handled elsewhere!
		char msg[128];
		SprintOpCodeInfo( msg, address, op_code );
		printf( "Unhandled: 0x%08x %s\n", address, msg );
#endif

		bool BranchDelaySet = false;

		if( R4300_InstructionHandlerNeedsPC( op_code ) )
		{
			//Generate exception handler
			//
			SetVar( &gCPUState.CurrentPC, address );
			if( branch_delay_slot )
			{
				SetVar( &gCPUState.Delay, EXEC_DELAY );
				BranchDelaySet = true;
			}

			GenerateGenericR4300( op_code, R4300_GetInstructionHandler( op_code ) );

#ifdef ALLOW_INTERPRETER_EXCEPTIONS
			// Make sure all dirty registers are flushed. NB - we don't invalidate them
			// to avoid reloading the contents if no exception was thrown.
			FlushAllRegisters( mRegisterCache, false );

			JAL( CCodeLabel( reinterpret_cast< const void * >( _ReturnFromDynaRecIfStuffToDo ) ), false );
			ORI( Ps2Reg_A0, Ps2Reg_R0, 0 );
#endif
		}
		else
		{
			GenerateGenericR4300( op_code, R4300_GetInstructionHandler( op_code ) );
		}

		// Check whether we want to invert the status of this branch
		if( p_branch != nullptr )
		{
			CCodeLabel		no_target( nullptr );
			//
			// Check if the branch has been taken
			//
			if( p_branch->Direct )
			{
				if( p_branch->ConditionalBranchTaken )
				{
					*p_branch_jump = GenerateBranchIfNotEqual( &gCPUState.Delay, DO_DELAY, no_target );
				}
				else
				{
					*p_branch_jump = GenerateBranchIfEqual( &gCPUState.Delay, DO_DELAY, no_target );
				}
			}
			else
			{
				// XXXX eventually just exit here, and skip default exit code below
				if( p_branch->Eret )
				{
					*p_branch_jump = GenerateBranchAlways( no_target );
				}
				else
				{
					*p_branch_jump = GenerateBranchIfNotEqual( &gCPUState.TargetPC, p_branch->TargetAddress, no_target );
				}
			}
		}

		if( BranchDelaySet )
		{
			SetVar( &gCPUState.Delay, NO_DELAY );
		}
	}

	return CJumpLocation();
}


//
//	NB - This assumes that the address and branch delay have been set up before
//	calling this function.
//

void	CCodeGeneratorPS2::GenerateGenericR4300( OpCode op_code, CPU_Instruction p_instruction )
{
	mPreviousLoadBase = N64Reg_R0;	//Invalidate
	mPreviousStoreBase = N64Reg_R0;	//Invalidate

	// Flush out the dirty registers, and mark them all as invalid
	// Theoretically we could just flush the registers referenced in the call
	FlushAllRegisters( mRegisterCache, true );

	Ps2OpCode	op1;
	Ps2OpCode	op2;

	// Load the opcode in to A0
	GetLoadConstantOps( Ps2Reg_A0, op_code._u32, &op1, &op2 );

	if( op2._u32 == 0 )
	{
		JAL( CCodeLabel( ( const void * )( p_instruction ) ), false );		// No branch delay
		AppendOp( op1 );
	}
	else
	{
		AppendOp( op1 );
		JAL( CCodeLabel( ( const void * )( p_instruction ) ), false );		// No branch delay
		AppendOp( op2 );
	}
}



//

CJumpLocation CCodeGeneratorPS2::ExecuteNativeFunction( CCodeLabel speed_hack, bool check_return )
{
	JAL( speed_hack, true );

	if( check_return )
		return BEQ(Ps2Reg_V0, Ps2Reg_R0, CCodeLabel(nullptr), true);
	else
		return CJumpLocation(nullptr);

}


//

inline bool	CCodeGeneratorPS2::GenerateDirectLoad( EPs2Reg ps2_dst, EN64Reg base, s16 offset, OpCodeValue load_op, u32 swizzle )
{
	if(mRegisterCache.IsKnownValue( base, 0 ))
	{
		u32		base_address( mRegisterCache.GetKnownValue( base, 0 )._u32 );
		u32		address( (base_address + s32( offset )) ^ swizzle );

		if( load_op == OP_LWL )
		{
			load_op = OP_LW;
			u32 shift = address & 3;
			address ^= shift;	//Zero low 2 bits in address
			ADDIU( Ps2Reg_A3, Ps2Reg_R0, shift);	//copy low 2 bits to A3
		}

		const MemFuncRead & m( g_MemoryLookupTableRead[ address >> 18 ] );
		if( m.pRead )
		{
			void * p_memory( (void*)( m.pRead + address ) );

			//printf( "Loading from %s %08x + %04x (%08x) op %d\n", RegNames[ base ], base_address, u16(offset), address, load_op );

			EPs2Reg		reg_base;
			s16			base_offset;
			GetBaseRegisterAndOffset( p_memory, &reg_base, &base_offset );

			CAssemblyWriterPS2::LoadRegister( ps2_dst, load_op, reg_base, base_offset );

			return true;
		}
		else
		{
			//printf( "Loading from %s %08x + %04x (%08x) - unhandled\n", RegNames[ base ], base_address, u16(offset), address );
		}
	}

	return false;
}


//

void	CCodeGeneratorPS2::GenerateSlowLoad( u32 current_pc, EPs2Reg ps2_dst, EPs2Reg reg_address, ReadMemoryFunction p_read_memory )
{
	// We need to flush all the regular registers AND invalidate all the temp regs.
	// NB: When we can flush registers through some kind of handle (i.e. when the
	// exception handling branch is taken), this won't be necessary

	CN64RegisterCachePS2 current_regs( mRegisterCache );
	FlushAllRegisters( mRegisterCache, false );
	FlushAllTemporaryRegisters( mRegisterCache, true );

	//
	//
	//
	if( reg_address != Ps2Reg_A0 )
	{
		OR( Ps2Reg_A0, reg_address, Ps2Reg_R0 );
	}
	Ps2OpCode	op1, op2;
	GetLoadConstantOps( Ps2Reg_A1, current_pc, &op1, &op2 );


	if( op2._u32 == 0 )
	{
		JAL( CCodeLabel( ( const void * )( p_read_memory ) ), false );
		AppendOp( op1 );
	}
	else
	{
		AppendOp( op1 );
		JAL( CCodeLabel( ( const void * )( p_read_memory ) ), false );
		AppendOp( op2 );
	}

	// Restore all registers BEFORE copying back final value
	RestoreAllRegisters( mRegisterCache, current_regs );

	// Copy return value to the destination register if we're not using V0
	if( ps2_dst != Ps2Reg_V0 )
	{
		OR( ps2_dst, Ps2Reg_V0, Ps2Reg_R0 );
	}

	// Restore the state of the registers
	mRegisterCache = current_regs;
}


//

void	CCodeGeneratorPS2::GenerateLoad( u32 current_pc,
										 EPs2Reg ps2_dst,
										 EN64Reg n64_base,
										 s16 offset,
										 OpCodeValue load_op,
										 u32 swizzle,
										 ReadMemoryFunction p_read_memory )
{
	//
	//	Check if the base pointer is a known value, in which case we can load directly.
	//
	if(GenerateDirectLoad( ps2_dst, n64_base, offset, load_op, swizzle ))
	{
		return;
	}

	EPs2Reg		reg_base( GetRegisterAndLoadLo( n64_base, Ps2Reg_A0 ) );
	EPs2Reg		reg_address( reg_base );

    if( load_op == OP_LWL )	//We handle both LWL & LWR here
    {
		if(offset != 0)
		{
			ADDIU( Ps2Reg_A0, reg_address, offset );    //base + offset
			reg_address = Ps2Reg_A0;
			offset = 0;
		}

		ANDI( Ps2Reg_A3, reg_address, 3 );				//copy two LSB bits to A3, used later for mask
		XOR( Ps2Reg_A0, reg_address, Ps2Reg_A3);		//zero two LSB bits in address

		//Dont cache the current pointer to K0 reg because the address is mangled by the XOR
		if( (n64_base == N64Reg_SP) | (gMemoryAccessOptimisation & mQuickLoad) )
		{
			ADDU( Ps2Reg_A0, Ps2Reg_A0, gMemoryBaseReg );
			CAssemblyWriterPS2::LoadRegister( ps2_dst, OP_LW, Ps2Reg_A0, 0 );
			return;
		}

		load_op = OP_LW;
		reg_address = Ps2Reg_A0;
	}
	else
	{
		if( (n64_base == N64Reg_SP) | (gMemoryAccessOptimisation & mQuickLoad) )
		{
			if( swizzle != 0 )
			{
				if( offset != 0 )
				{
					ADDIU( Ps2Reg_A0, reg_address, offset );
					reg_address = Ps2Reg_A0;
					offset = 0;
				}

				XORI( Ps2Reg_A0, reg_address, swizzle );
				ADDU( Ps2Reg_A0, Ps2Reg_A0, gMemoryBaseReg );
				CAssemblyWriterPS2::LoadRegister( ps2_dst, load_op, Ps2Reg_A0, offset );
				return;
			}
#if 0
			//Re use old base register if consegutive accesses from same base register //Corn
			if( n64_base == mPreviousLoadBase )
			{
				CAssemblyWriterPS2::LoadRegister( ps2_dst, load_op, Ps2Reg_K0, offset );
				return;
			}
			else
			{
				ADDU( Ps2Reg_K0, reg_address, gMemoryBaseReg );
				CAssemblyWriterPS2::LoadRegister( ps2_dst, load_op, Ps2Reg_K0, offset );
				mPreviousLoadBase = n64_base;
				return;
			}
#else
			//Re use old base register if consegutive accesses from same base register //Corn
			if (n64_base == mPreviousLoadBase)
			{
				CAssemblyWriterPS2::LoadRegister(ps2_dst, load_op, Ps2Reg_S5, offset);
				return;
			}
			else
			{
				ADDU(Ps2Reg_S5, reg_address, gMemoryBaseReg);
				CAssemblyWriterPS2::LoadRegister(ps2_dst, load_op, Ps2Reg_S5, offset);
				mPreviousLoadBase = n64_base;
				return;
			}
#endif
		}
	}

	if( swizzle != 0 )
	{
		if( offset != 0 )
		{
			ADDIU( Ps2Reg_A0, reg_address, offset );
			reg_address = Ps2Reg_A0;
			offset = 0;
		}

		XORI( Ps2Reg_A0, reg_address, swizzle );
		reg_address = Ps2Reg_A0;
	}

	//
	//	If the fast load failed, we need to perform a 'slow' load going through a handler routine
	//	We may already be performing this instruction in the second buffer, in which
	//	case we don't bother with the transitions
	//
	if( CAssemblyWriterPS2::IsBufferA() )
	{
		//
		//	This is a very quick check for 0x80000000 < address < 0x80r00000
		//	If it's in range, we add the base pointer (already offset by 0x80000000)
		//	and dereference (the branch likely ensures that we only dereference
		//	in the situations where the condition holds)
		//
		SLT( Ps2Reg_V1, reg_address, gMemUpperBoundReg );	// t1 = upper < address
		CJumpLocation branch( BEQ( Ps2Reg_V1, Ps2Reg_R0, CCodeLabel( nullptr ), false ) );		// branch to jump to handler
		ADDU( Ps2Reg_A1, reg_address, gMemoryBaseReg );
		CAssemblyWriterPS2::LoadRegister( ps2_dst, load_op, Ps2Reg_A1, offset );

		CCodeLabel		continue_location( GetAssemblyBuffer()->GetLabel() );

		//
		//	Generate the handler code in the secondary buffer
		//
		CAssemblyWriterPS2::SetBufferB();

		CCodeLabel		handler_label( GetAssemblyBuffer()->GetLabel() );
		if( offset != 0 )
		{
			ADDIU( Ps2Reg_A0, reg_address, offset );
			reg_address = Ps2Reg_A0;
		}
#ifdef ENABLE_LWC1
		if( load_op == OP_LWC1 )
		{
			GenerateSlowLoad( current_pc, Ps2Reg_V0, reg_address, p_read_memory );
			// Copy return value to the FPU destination register
			MTC1( (EPs2FloatReg)ps2_dst, Ps2Reg_V0 );
		}
		else
#endif
		{
			GenerateSlowLoad( current_pc, ps2_dst, reg_address, p_read_memory );
		}

		CJumpLocation	ret_handler( J( continue_location, true ) );
		CAssemblyWriterPS2::SetBufferA();

		//
		//	Keep track of the details we need to generate the jump to the distant handler function
		//
		mAddressCheckFixups.push_back( SAddressCheckFixup( branch, handler_label ) );
	}
	else
	{
		//
		//	This is a very quick check for 0x80000000 < address < 0x80r00000
		//	If it's in range, we add the base pointer (already offset by 0x80000000)
		//	and dereference (the branch likely ensures that we only dereference
		//	in the situations where the condition holds)
		//
		SLT( Ps2Reg_V1, reg_address, gMemUpperBoundReg );	// t1 = upper < address
		ADDU( Ps2Reg_A1, reg_address, gMemoryBaseReg );
		CJumpLocation branch( BNEL( Ps2Reg_V1, Ps2Reg_R0, CCodeLabel( nullptr ), false ) );
		CAssemblyWriterPS2::LoadRegister( ps2_dst, load_op, Ps2Reg_A1, offset );

		if( offset != 0 )
		{
			ADDIU( Ps2Reg_A0, reg_address, offset );
			reg_address = Ps2Reg_A0;
		}
#ifdef ENABLE_LWC1
		if( load_op == OP_LWC1 )
		{
			GenerateSlowLoad( current_pc, Ps2Reg_V0, reg_address, p_read_memory );
			// Copy return value to the FPU destination register
			MTC1( (EPs2FloatReg)ps2_dst, Ps2Reg_V0 );
		}
		else
#endif
		{
			GenerateSlowLoad( current_pc, ps2_dst, reg_address, p_read_memory );
		}

		PatchJumpLong( branch, GetAssemblyBuffer()->GetLabel() );
	}

}


//

inline void CCodeGeneratorPS2::GenerateAddressCheckFixups()
{
	for( u32 i = 0; i < mAddressCheckFixups.size(); ++i )
	{
		GenerateAddressCheckFixup( mAddressCheckFixups[ i ] );
	}
}


//

inline void CCodeGeneratorPS2::GenerateAddressCheckFixup( const SAddressCheckFixup & fixup )
{
	CAssemblyWriterPS2::SetBufferA();

	//
	//	Fixup the branch we emitted so that it jumps to the handler routine.
	//	If possible, we try to branch directly.
	//	If that fails, we try to emit a stub function which jumps to the handler. Hopefully that's a bit closer
	//	If *that* fails, we have to revert to an unconditional jump to the handler routine. Expensive!!
	//
/*
	if( PatchJumpLong( fixup.BranchToJump, fixup.HandlerAddress ) )
	{
		printf( "Wow, managed to jump directly to our handler\n" );
	}
	else
	{
		if( PatchJumpLong( fixup.BranchToJump, GetAssemblyBuffer()->GetLabel() ) )
		{
			//
			//	Emit a long jump to the handler function
			//
			J( fixup.HandlerAddress, true );
		}
		else
		{
			printf( "Warning, jump is too far for address handler function\n" );
			ReplaceBranchWithJump( fixup.BranchToJump, fixup.HandlerAddress );
		}
	}
*/
	// Optimized version of above code - Salvy
	// First condition which is the fastest, never happens
	// Second condition which still fast, always happens (we have an assert if otherwise)
	// Third condition (slowest) never happens and thus is ignored
	// ToDO: Optimized futher eliminating return value from PatchJumpLong
	//
	PatchJumpLong( fixup.BranchToJump, GetAssemblyBuffer()->GetLabel() );
	//
	//	Emit a long jump to the handler function
	//
	J( fixup.HandlerAddress, true );
}



//

inline bool	CCodeGeneratorPS2::GenerateDirectStore( EPs2Reg ps2_src, EN64Reg base, s16 offset, OpCodeValue store_op, u32 swizzle )
{
	if(mRegisterCache.IsKnownValue( base, 0 ))
	{
		u32		base_address( mRegisterCache.GetKnownValue( base, 0 )._u32 );
		u32		address( (base_address + s32( offset )) ^ swizzle );

		const MemFuncWrite & m( g_MemoryLookupTableWrite[ address >> 18 ] );
		if( m.pWrite )
		{
			void * p_memory( (void*)( m.pWrite + address ) );

			//printf( "Storing to %s %08x + %04x (%08x) op %d\n", RegNames[ base ], base_address, u16(offset), address, store_op );

			EPs2Reg		reg_base;
			s16			base_offset;
			GetBaseRegisterAndOffset( p_memory, &reg_base, &base_offset );

			CAssemblyWriterPS2::StoreRegister( ps2_src, store_op, reg_base, base_offset );

			return true;
		}
		else
		{
			//printf( "Storing to %s %08x + %04x (%08x) - unhandled\n", RegNames[ base ], base_address, u16(offset), address );
		}
	}

	return false;
}


//

inline void	CCodeGeneratorPS2::GenerateSlowStore( u32 current_pc, EPs2Reg ps2_src, EPs2Reg reg_address, WriteMemoryFunction p_write_memory )
{
	// We need to flush all the regular registers AND invalidate all the temp regs.
	// NB: When we can flush registers through some kind of handle (i.e. when the
	// exception handling branch is taken), this won't be necessary
	CN64RegisterCachePS2 current_regs( mRegisterCache );
	FlushAllRegisters( mRegisterCache, false );
	FlushAllTemporaryRegisters( mRegisterCache, true );

	if( reg_address != Ps2Reg_A0 )
	{
		OR( Ps2Reg_A0, reg_address, Ps2Reg_R0 );
	}
	if( ps2_src != Ps2Reg_A1 )
	{
		OR( Ps2Reg_A1, ps2_src, Ps2Reg_R0 );
	}
	Ps2OpCode	op1, op2;
	GetLoadConstantOps( Ps2Reg_A2, current_pc, &op1, &op2 );


	if( op2._u32 == 0 )
	{
		JAL( CCodeLabel( ( const void * )( p_write_memory ) ), false );		// Don't set branch delay slot
		AppendOp( op1 );
	}
	else
	{
		AppendOp( op1 );
		JAL( CCodeLabel( ( const void * )( p_write_memory ) ), false );		// Don't set branch delay slot
		AppendOp( op2 );
	}

	// Restore all registers BEFORE copying back final value
	RestoreAllRegisters( mRegisterCache, current_regs );

	// Restore the state of the registers
	mRegisterCache = current_regs;
}


//

void	CCodeGeneratorPS2::GenerateStore( u32 current_pc,
										  EPs2Reg ps2_src,
										  EN64Reg n64_base,
										  s32 offset,
										  OpCodeValue store_op,
										  u32 swizzle,
										  WriteMemoryFunction p_write_memory )
{
	//
	//	Check if the base pointer is a known value, in which case we can store directly.
	//
	if(GenerateDirectStore( ps2_src, n64_base, offset, store_op, swizzle ))
	{
		return;
	}

	EPs2Reg		reg_base( GetRegisterAndLoadLo( n64_base, Ps2Reg_A0 ) );
	EPs2Reg		reg_address( reg_base );

	if( (n64_base == N64Reg_SP) | (gMemoryAccessOptimisation & mQuickLoad) )
	{
		if( swizzle != 0 )
		{
			if( offset != 0 )
			{
				ADDIU( Ps2Reg_A0, reg_address, offset );
				reg_address = Ps2Reg_A0;
				offset = 0;
			}

			XORI( Ps2Reg_A0, reg_address, swizzle );
			ADDU( Ps2Reg_A0, Ps2Reg_A0, gMemoryBaseReg );
			CAssemblyWriterPS2::StoreRegister( ps2_src, store_op, Ps2Reg_A0, offset );
			return;
		}

		//Re use old base register if consegutive accesses from same base register //Corn
		if( n64_base == mPreviousStoreBase )
		{
			CAssemblyWriterPS2::StoreRegister( ps2_src, store_op, Ps2Reg_K1, offset );
			return;
		}
		else
		{
			ADDU( Ps2Reg_K1, reg_address, gMemoryBaseReg );
			CAssemblyWriterPS2::StoreRegister( ps2_src, store_op, Ps2Reg_K1, offset );
			mPreviousStoreBase = n64_base;
			return;
		}
	}

	if( swizzle != 0 )
	{
		if( offset != 0 )
		{
			ADDIU( Ps2Reg_A0, reg_address, offset );
			reg_address = Ps2Reg_A0;
			offset = 0;
		}

		XORI( Ps2Reg_A0, reg_address, swizzle );
		reg_address = Ps2Reg_A0;
	}

	if( CAssemblyWriterPS2::IsBufferA() )
	{
		SLT( Ps2Reg_V1, reg_address, gMemUpperBoundReg );	// V1 = upper < address
		CJumpLocation branch( BEQ( Ps2Reg_V1, Ps2Reg_R0, CCodeLabel( nullptr ), false ) );
		ADDU( Ps2Reg_V0, reg_address, gMemoryBaseReg );
		CAssemblyWriterPS2::StoreRegister( ps2_src, store_op, Ps2Reg_V0, offset );

		CCodeLabel		continue_location( GetAssemblyBuffer()->GetLabel() );

		//
		//	Generate the handler code in the secondary buffer
		//
		CAssemblyWriterPS2::SetBufferB();

		CCodeLabel		handler_label( GetAssemblyBuffer()->GetLabel() );
		if( offset != 0 )
		{
			ADDIU( Ps2Reg_A0, reg_address, offset );
			reg_address = Ps2Reg_A0;
		}
#ifdef ENABLE_SWC1
		if( store_op == OP_SWC1 )
		{	//Move value from FPU to CPU
			MFC1( Ps2Reg_A1, (EPs2FloatReg)ps2_src );
			ps2_src = Ps2Reg_A1;
		}
#endif
		GenerateSlowStore( current_pc, ps2_src, reg_address, p_write_memory );

		CJumpLocation	ret_handler( J( continue_location, true ) );
		CAssemblyWriterPS2::SetBufferA();


		//
		//	Keep track of the details we need to generate the jump to the distant handler function
		//
		mAddressCheckFixups.push_back( SAddressCheckFixup( branch, handler_label ) );
	}
	else
	{
		SLT( Ps2Reg_V1, reg_address, gMemUpperBoundReg );	// V1 = upper < address
		ADDU( Ps2Reg_V0, reg_address, gMemoryBaseReg );
		CJumpLocation branch( BNEL( Ps2Reg_V1, Ps2Reg_R0, CCodeLabel( nullptr ), false ) );
		CAssemblyWriterPS2::StoreRegister( ps2_src, store_op, Ps2Reg_V0, offset );

		if( offset != 0 )
		{
			ADDIU( Ps2Reg_A0, reg_address, offset );
			reg_address = Ps2Reg_A0;
		}
#ifdef ENABLE_SWC1
		if( store_op == OP_SWC1 )
		{	//Move value from FPU to CPU
			MFC1( Ps2Reg_A1, (EPs2FloatReg)ps2_src );
			ps2_src = Ps2Reg_A1;
		}
#endif
		GenerateSlowStore( current_pc, ps2_src, reg_address, p_write_memory );

		PatchJumpLong( branch, GetAssemblyBuffer()->GetLabel() );
	}
}


//

inline void	CCodeGeneratorPS2::GenerateCACHE( EN64Reg base, s16 offset, u32 cache_op )
{
	//u32 cache_op  = op_code.rt;
	//u32 address = (u32)( gGPR[op_code.base]._s32_0 + (s32)(s16)op_code.immediate );

	u32 cache = cache_op & 0x3;
	u32 action = (cache_op >> 2) & 0x7;

	// For instruction cache invalidation, make sure we let the CPU know so the whole
	// dynarec system can be invalidated
	//if(!(cache_op & 0x1F))
	if(cache == 0 && (action == 0 || action == 4))
	{
		FlushAllRegisters(mRegisterCache, true);
		LoadRegister(Ps2Reg_A0, base, 0);
		ADDI(Ps2Reg_A0, Ps2Reg_A0, offset);
		JAL( CCodeLabel( ( const void * )( CPU_InvalidateICacheRange ) ), false );
		ORI(Ps2Reg_A1, Ps2Reg_R0, 0x20);
	}
	//else
	//{
		// We don't care about data cache etc
	//}
}


//

inline void	CCodeGeneratorPS2::GenerateJAL( u32 address )
{
	//gGPR[REG_ra]._s64 = (s64)(s32)(gCPUState.CurrentPC + 8);		// Store return address
	//u32	new_pc( (gCPUState.CurrentPC & 0xF0000000) | (op_code.target<<2) );
	//CPU_TakeBranch( new_pc, CPU_BRANCH_DIRECT );

	//EPs2Reg	reg_lo( GetRegisterNoLoadLo( N64Reg_RA, Ps2Reg_V0 ) );
	//LoadConstant( reg_lo, address + 8 );
	//UpdateRegister( N64Reg_RA, reg_lo, URO_HI_SIGN_EXTEND, Ps2Reg_V0 );
	SetRegister32s(N64Reg_RA, address + 8);
}


//

inline void	CCodeGeneratorPS2::GenerateJR( EN64Reg rs, const SBranchDetails * p_branch, CJumpLocation * p_branch_jump )
{
	//u32	new_pc( gGPR[ op_code.rs ]._u32_0 );
	//CPU_TakeBranch( new_pc, CPU_BRANCH_INDIRECT );

	EPs2Reg reg_lo( GetRegisterAndLoadLo( rs, Ps2Reg_V0 ) );

	// Necessary? Could just directly compare reg_lo and constant p_branch->TargetAddress??
	//SetVar( &gCPUState.TargetPC, reg_lo );
	//SetVar( &gCPUState.Delay, DO_DELAY );
	//*p_branch_jump = GenerateBranchIfNotEqual( &gCPUState.TargetPC, p_branch->TargetAddress, CCodeLabel() );

	SetVar( &gCPUState.TargetPC, reg_lo );
	//SetVar( &gCPUState.Delay, DO_DELAY );
	*p_branch_jump = GenerateBranchIfNotEqual( reg_lo, p_branch->TargetAddress, CCodeLabel() );
}


//

inline void	CCodeGeneratorPS2::GenerateJALR( EN64Reg rs, EN64Reg rd, u32 address, const SBranchDetails * p_branch, CJumpLocation * p_branch_jump )
{
	// Jump And Link
	//u32	new_pc( gGPR[ op_code.rs ]._u32_0 );
	//gGPR[ op_code.rd ]._s64 = (s64)(s32)(gCPUState.CurrentPC + 8);		// Store return address
	//EPs2Reg	savereg_lo( GetRegisterNoLoadLo( rd, Ps2Reg_V0 ) );

	// Necessary? Could just directly compare reg_lo and constant p_branch->TargetAddress??
	//SetVar( &gCPUState.TargetPC, reg_lo );
	//SetVar( &gCPUState.Delay, DO_DELAY );
	//*p_branch_jump = GenerateBranchIfNotEqual( &gCPUState.TargetPC, p_branch->TargetAddress, CCodeLabel() );
	//LoadConstant( savereg_lo, address + 8 );
	//UpdateRegister( rd, savereg_lo, URO_HI_SIGN_EXTEND, Ps2Reg_V0 );
	SetRegister32s(rd, address + 8);

	EPs2Reg reg_lo( GetRegisterAndLoadLo( rs, Ps2Reg_V0 ) );
	SetVar( &gCPUState.TargetPC, reg_lo );
	*p_branch_jump = GenerateBranchIfNotEqual( reg_lo, p_branch->TargetAddress, CCodeLabel() );
}


//

inline void	CCodeGeneratorPS2::GenerateMFLO( EN64Reg rd )
{
	//gGPR[ op_code.rd ]._u64 = gCPUState.MultLo._u64;

	if( mMultIsValid )
	{
		EPs2Reg	reg_lo( GetRegisterNoLoadLo( rd, Ps2Reg_A0 ) );
		MFLO( reg_lo );
		UpdateRegister( rd, reg_lo, URO_HI_SIGN_EXTEND );
	}
	else
	{
		EPs2Reg	reg_lo( GetRegisterNoLoadLo( rd, Ps2Reg_V0 ) );
		GetVar( reg_lo, &gCPUState.MultLo._u32_0 );
		StoreRegisterLo( rd, reg_lo );

		EPs2Reg	reg_hi( GetRegisterNoLoadHi( rd, Ps2Reg_V0 ) );
		GetVar( reg_hi, &gCPUState.MultLo._u32_1 );
		StoreRegisterHi( rd, reg_hi );
	}
}


//

inline void	CCodeGeneratorPS2::GenerateMFHI( EN64Reg rd )
{
	//gGPR[ op_code.rd ]._u64 = gCPUState.MultHi._u64;

	if( mMultIsValid )
	{
		EPs2Reg	reg_lo( GetRegisterNoLoadLo( rd, Ps2Reg_V0 ) );
		MFHI( reg_lo );
		UpdateRegister( rd, reg_lo, URO_HI_SIGN_EXTEND );
	}
	else
	{
		EPs2Reg	reg_lo( GetRegisterNoLoadLo( rd, Ps2Reg_V0 ) );
		GetVar( reg_lo, &gCPUState.MultHi._u32_0 );
		StoreRegisterLo( rd, reg_lo );

		EPs2Reg	reg_hi( GetRegisterNoLoadHi( rd, Ps2Reg_V0 ) );
		GetVar( reg_hi, &gCPUState.MultHi._u32_1 );
		StoreRegisterHi( rd, reg_hi );
	}
}


//

inline void	CCodeGeneratorPS2::GenerateMTLO( EN64Reg rs )
{
	mMultIsValid = false;

	//gCPUState.MultLo._u64 = gGPR[ op_code.rs ]._u64;

	EPs2Reg	reg_lo( GetRegisterAndLoadLo( rs, Ps2Reg_V0 ) );
	SetVar( &gCPUState.MultLo._u32_0, reg_lo );

	EPs2Reg	reg_hi( GetRegisterAndLoadHi( rs, Ps2Reg_V0 ) );
	SetVar( &gCPUState.MultLo._u32_1, reg_hi );
}


//

inline void	CCodeGeneratorPS2::GenerateMTHI( EN64Reg rs )
{
	mMultIsValid = false;

	//gCPUState.MultHi._u64 = gGPR[ op_code.rs ]._u64;

	EPs2Reg	reg_lo( GetRegisterAndLoadLo( rs, Ps2Reg_V0 ) );
	SetVar( &gCPUState.MultHi._u32_0, reg_lo );

	EPs2Reg	reg_hi( GetRegisterAndLoadHi( rs, Ps2Reg_V0 ) );
	SetVar( &gCPUState.MultHi._u32_1, reg_hi );
}


//

inline void	CCodeGeneratorPS2::GenerateMULT( EN64Reg rs, EN64Reg rt )
{
	mMultIsValid = true;

	//s64 dwResult = (s64)gGPR[ op_code.rs ]._s32_0 * (s64)gGPR[ op_code.rt ]._s32_0;
	//gCPUState.MultLo = (s64)(s32)(dwResult);
	//gCPUState.MultHi = (s64)(s32)(dwResult >> 32);

	EPs2Reg	reg_lo_a( GetRegisterAndLoadLo( rs, Ps2Reg_V0 ) );
	EPs2Reg	reg_lo_b( GetRegisterAndLoadLo( rt, Ps2Reg_A0 ) );

	MULT( reg_lo_a, reg_lo_b );

	MFLO( Ps2Reg_V0 );
	MFHI( Ps2Reg_A0 );

	SetVar( &gCPUState.MultLo._u32_0, Ps2Reg_V0 );
	SetVar( &gCPUState.MultHi._u32_0, Ps2Reg_A0 );

#ifdef ENABLE_64BIT
	SRA( Ps2Reg_V0, Ps2Reg_V0, 0x1f );		// Sign extend
	SRA( Ps2Reg_A0, Ps2Reg_A0, 0x1f );		// Sign extend

	SetVar( &gCPUState.MultLo._u32_1, Ps2Reg_V0 );
	SetVar( &gCPUState.MultHi._u32_1, Ps2Reg_A0 );
#endif
}


//

inline void	CCodeGeneratorPS2::GenerateMULTU( EN64Reg rs, EN64Reg rt )
{
	mMultIsValid = true;

	//u64 dwResult = (u64)gGPR[ op_code.rs ]._u32_0 * (u64)gGPR[ op_code.rt ]._u32_0;
	//gCPUState.MultLo = (s64)(s32)(dwResult);
	//gCPUState.MultHi = (s64)(s32)(dwResult >> 32);

	EPs2Reg	reg_lo_a( GetRegisterAndLoadLo( rs, Ps2Reg_V0 ) );
	EPs2Reg	reg_lo_b( GetRegisterAndLoadLo( rt, Ps2Reg_A0 ) );

	MULTU( reg_lo_a, reg_lo_b );

	MFLO( Ps2Reg_V0 );
	MFHI( Ps2Reg_A0 );

	SetVar( &gCPUState.MultLo._u32_0, Ps2Reg_V0 );
	SetVar( &gCPUState.MultHi._u32_0, Ps2Reg_A0 );

	//Yoshi and DOOM64 must have sign extension //Corn
	SRA( Ps2Reg_V0, Ps2Reg_V0, 0x1f );		// Sign extend
	SRA( Ps2Reg_A0, Ps2Reg_A0, 0x1f );		// Sign extend

	SetVar( &gCPUState.MultLo._u32_1, Ps2Reg_V0 );
	SetVar( &gCPUState.MultHi._u32_1, Ps2Reg_A0 );
}


//

inline void	CCodeGeneratorPS2::GenerateDIV( EN64Reg rs, EN64Reg rt )
{
	mMultIsValid = true;

	//s32 nDividend = gGPR[ op_code.rs ]._s32_0;
	//s32 nDivisor  = gGPR[ op_code.rt ]._s32_0;

	//if (nDivisor)
	//{
	//	gCPUState.MultLo._u64 = (s64)(s32)(nDividend / nDivisor);
	//	gCPUState.MultHi._u64 = (s64)(s32)(nDividend % nDivisor);
	//}

#ifdef DIVZEROCHK
	EPs2Reg	reg_lo_rs( GetRegisterAndLoadLo( rs, Ps2Reg_V0 ) );
	EPs2Reg	reg_lo_rt( GetRegisterAndLoadLo( rt, Ps2Reg_A0 ) );

	CJumpLocation	branch( BEQ( reg_lo_rt, Ps2Reg_R0, CCodeLabel(nullptr), true ) );		// Can use branch delay for something?

	DIV( reg_lo_rs, reg_lo_rt );

	MFLO( Ps2Reg_V0 );
	MFHI( Ps2Reg_A0 );

	SetVar( &gCPUState.MultLo._u32_0, Ps2Reg_V0 );
	SetVar( &gCPUState.MultHi._u32_0, Ps2Reg_A0 );

	SRA( Ps2Reg_V0, Ps2Reg_V0, 0x1f );		// Sign extend
	SRA( Ps2Reg_A0, Ps2Reg_A0, 0x1f );		// Sign extend

	SetVar( &gCPUState.MultLo._u32_1, Ps2Reg_V0 );
	SetVar( &gCPUState.MultHi._u32_1, Ps2Reg_A0 );

	// Branch here - really should trigger exception!
	PatchJumpLong( branch, GetAssemblyBuffer()->GetLabel() );

#else
	EPs2Reg	reg_lo_rs( GetRegisterAndLoadLo( rs, Ps2Reg_V0 ) );
	EPs2Reg	reg_lo_rt( GetRegisterAndLoadLo( rt, Ps2Reg_A0 ) );

	DIV( reg_lo_rs, reg_lo_rt );

	MFLO( Ps2Reg_V0 );
	MFHI( Ps2Reg_A0 );

	SetVar( &gCPUState.MultLo._u32_0, Ps2Reg_V0 );
	SetVar( &gCPUState.MultHi._u32_0, Ps2Reg_A0 );

#ifdef ENABLE_64BIT
	SRA( Ps2Reg_V0, Ps2Reg_V0, 0x1f );		// Sign extend
	SRA( Ps2Reg_A0, Ps2Reg_A0, 0x1f );		// Sign extend

	SetVar( &gCPUState.MultLo._u32_1, Ps2Reg_V0 );
	SetVar( &gCPUState.MultHi._u32_1, Ps2Reg_A0 );
#endif

#endif
}


//

inline void	CCodeGeneratorPS2::GenerateDIVU( EN64Reg rs, EN64Reg rt )
{
	mMultIsValid = true;

	//u32 dwDividend = gGPR[ op_code.rs ]._u32_0;
	//u32 dwDivisor  = gGPR[ op_code.rt ]._u32_0;

	//if (dwDivisor) {
	//	gCPUState.MultLo._u64 = (s64)(s32)(dwDividend / dwDivisor);
	//	gCPUState.MultHi._u64 = (s64)(s32)(dwDividend % dwDivisor);
	//}

#ifdef DIVZEROCHK
	EPs2Reg	reg_lo_rs( GetRegisterAndLoadLo( rs, Ps2Reg_V0 ) );
	EPs2Reg	reg_lo_rt( GetRegisterAndLoadLo( rt, Ps2Reg_A0 ) );

	CJumpLocation	branch( BEQ( reg_lo_rt, Ps2Reg_R0, CCodeLabel(nullptr), true ) );		// Can use branch delay for something?

	DIVU( reg_lo_rs, reg_lo_rt );

	MFLO( Ps2Reg_V0 );
	MFHI( Ps2Reg_A0 );

	SetVar( &gCPUState.MultLo._u32_0, Ps2Reg_V0 );
	SetVar( &gCPUState.MultHi._u32_0, Ps2Reg_A0 );

	SRA( Ps2Reg_V0, Ps2Reg_V0, 0x1f );		// Sign extend
	SRA( Ps2Reg_A0, Ps2Reg_A0, 0x1f );		// Sign extend

	SetVar( &gCPUState.MultLo._u32_1, Ps2Reg_V0 );
	SetVar( &gCPUState.MultHi._u32_1, Ps2Reg_A0 );

	// Branch here - really should trigger exception!
	PatchJumpLong( branch, GetAssemblyBuffer()->GetLabel() );

#else
	EPs2Reg	reg_lo_rs( GetRegisterAndLoadLo( rs, Ps2Reg_V0 ) );
	EPs2Reg	reg_lo_rt( GetRegisterAndLoadLo( rt, Ps2Reg_A0 ) );

	DIVU( reg_lo_rs, reg_lo_rt );

	MFLO( Ps2Reg_V0 );
	MFHI( Ps2Reg_A0 );

	SetVar( &gCPUState.MultLo._u32_0, Ps2Reg_V0 );
	SetVar( &gCPUState.MultHi._u32_0, Ps2Reg_A0 );

#ifdef ENABLE_64BIT
	SRA( Ps2Reg_V0, Ps2Reg_V0, 0x1f );		// Sign extend
	SRA( Ps2Reg_A0, Ps2Reg_A0, 0x1f );		// Sign extend

	SetVar( &gCPUState.MultLo._u32_1, Ps2Reg_V0 );
	SetVar( &gCPUState.MultHi._u32_1, Ps2Reg_A0 );
#endif

#endif
}


//

inline void	CCodeGeneratorPS2::GenerateADDU( EN64Reg rd, EN64Reg rs, EN64Reg rt )
{
	if (mRegisterCache.IsKnownValue(rs, 0)
		& mRegisterCache.IsKnownValue(rt, 0))
	{
		SetRegister32s(rd, mRegisterCache.GetKnownValue(rs, 0)._s32
			+ mRegisterCache.GetKnownValue(rt, 0)._s32);
		return;
	}

	if( rs == N64Reg_R0 )
	{
		if(mRegisterCache.IsKnownValue(rt, 0))
		{
			SetRegister32s(rd, mRegisterCache.GetKnownValue(rt, 0)._s32);
			return;
		}

		// As RS is zero, the ADD is just a copy of RT to RD.
		// Try to avoid loading into a temp register if the dest is cached
		EPs2Reg reg_lo_d( GetRegisterNoLoadLo( rd, Ps2Reg_V0 ) );
		LoadRegisterLo( reg_lo_d, rt );
		UpdateRegister( rd, reg_lo_d, URO_HI_SIGN_EXTEND );
	}
	else if( rt == N64Reg_R0 )
	{
		if(mRegisterCache.IsKnownValue(rs, 0))
		{
			SetRegister32s(rd, mRegisterCache.GetKnownValue(rs, 0)._s32);
			return;
		}

		// As RT is zero, the ADD is just a copy of RS to RD.
		// Try to avoid loading into a temp register if the dest is cached
		EPs2Reg reg_lo_d( GetRegisterNoLoadLo( rd, Ps2Reg_V0 ) );
		LoadRegisterLo( reg_lo_d, rs );
		UpdateRegister( rd, reg_lo_d, URO_HI_SIGN_EXTEND );
	}
	else
	{
		//gGPR[ op_code.rd ]._s64 = (s64)(s32)( gGPR[ op_code.rs ]._s32_0 + gGPR[ op_code.rt ]._s32_0 );
		EPs2Reg	reg_lo_d( GetRegisterNoLoadLo( rd, Ps2Reg_V0 ) );
		EPs2Reg	reg_lo_a( GetRegisterAndLoadLo( rs, Ps2Reg_V0 ) );
		EPs2Reg	reg_lo_b( GetRegisterAndLoadLo( rt, Ps2Reg_A0 ) );
		ADDU( reg_lo_d, reg_lo_a, reg_lo_b );
		UpdateRegister( rd, reg_lo_d, URO_HI_SIGN_EXTEND );
	}
}


//

inline void	CCodeGeneratorPS2::GenerateSUBU( EN64Reg rd, EN64Reg rs, EN64Reg rt )
{
	if (mRegisterCache.IsKnownValue(rs, 0)
		& mRegisterCache.IsKnownValue(rt, 0))
	{
		SetRegister32s(rd, mRegisterCache.GetKnownValue(rs, 0)._s32
			- mRegisterCache.GetKnownValue(rt, 0)._s32);
		return;
	}

	//gGPR[ op_code.rd ]._s64 = (s64)(s32)( gGPR[ op_code.rs ]._s32_0 - gGPR[ op_code.rt ]._s32_0 );
	EPs2Reg	reg_lo_d( GetRegisterNoLoadLo( rd, Ps2Reg_V0 ) );
	EPs2Reg	reg_lo_a( GetRegisterAndLoadLo( rs, Ps2Reg_V0 ) );
	EPs2Reg	reg_lo_b( GetRegisterAndLoadLo( rt, Ps2Reg_A0 ) );
	SUBU( reg_lo_d, reg_lo_a, reg_lo_b );
	UpdateRegister( rd, reg_lo_d, URO_HI_SIGN_EXTEND );
}


//64bit unsigned division //Corn

inline void	CCodeGeneratorPS2::GenerateDDIVU( EN64Reg rs, EN64Reg rt )
{
	mMultIsValid = false;

	EPs2Reg	reg_lo_a( GetRegisterAndLoadLo( rs, Ps2Reg_A0 ) );
	EPs2Reg	reg_hi_a( GetRegisterAndLoadHi( rs, Ps2Reg_A1 ) );
	EPs2Reg	reg_lo_b( GetRegisterAndLoadLo( rt, Ps2Reg_A2 ) );

	if( reg_lo_a != Ps2Reg_A0 ) OR( Ps2Reg_A0, Ps2Reg_R0, reg_lo_a);
	if( reg_hi_a != Ps2Reg_A1 ) OR( Ps2Reg_A1, Ps2Reg_R0, reg_hi_a);
	if( reg_lo_b != Ps2Reg_A2 ) OR( Ps2Reg_A2, Ps2Reg_R0, reg_lo_b);

	JAL( CCodeLabel( (void*)_DDIVU ), true );
}


//64bit signed division //Corn

inline void	CCodeGeneratorPS2::GenerateDDIV( EN64Reg rs, EN64Reg rt )
{
	mMultIsValid = false;

	EPs2Reg	reg_lo_a( GetRegisterAndLoadLo( rs, Ps2Reg_A0 ) );
	EPs2Reg	reg_hi_a( GetRegisterAndLoadHi( rs, Ps2Reg_A1 ) );
	EPs2Reg	reg_lo_b( GetRegisterAndLoadLo( rt, Ps2Reg_A2 ) );

	if( reg_lo_a != Ps2Reg_A0 ) OR( Ps2Reg_A0, Ps2Reg_R0, reg_lo_a);
	if( reg_hi_a != Ps2Reg_A1 ) OR( Ps2Reg_A1, Ps2Reg_R0, reg_hi_a);
	if( reg_lo_b != Ps2Reg_A2 ) OR( Ps2Reg_A2, Ps2Reg_R0, reg_lo_b);

	JAL( CCodeLabel( (void*)_DDIV ), true );
}


//64bit unsigned multiply //Corn

inline void	CCodeGeneratorPS2::GenerateDMULTU( EN64Reg rs, EN64Reg rt )
{
	mMultIsValid = false;

	EPs2Reg	reg_lo_a( GetRegisterAndLoadLo( rs, Ps2Reg_A0 ) );
	EPs2Reg	reg_hi_a( GetRegisterAndLoadHi( rs, Ps2Reg_A1 ) );
	EPs2Reg	reg_lo_b( GetRegisterAndLoadLo( rt, Ps2Reg_A2 ) );
	EPs2Reg	reg_hi_b( GetRegisterAndLoadHi( rt, Ps2Reg_A3 ) );

	if( reg_lo_a != Ps2Reg_A0 ) OR( Ps2Reg_A0, Ps2Reg_R0, reg_lo_a);
	if( reg_hi_a != Ps2Reg_A1 ) OR( Ps2Reg_A1, Ps2Reg_R0, reg_hi_a);
	if( reg_lo_b != Ps2Reg_A2 ) OR( Ps2Reg_A2, Ps2Reg_R0, reg_lo_b);
	if( reg_hi_b != Ps2Reg_A3 ) OR( Ps2Reg_A3, Ps2Reg_R0, reg_hi_b);

	JAL( CCodeLabel( (void*)_DMULTU ), true );
}


//64bit signed multiply //Corn

inline void	CCodeGeneratorPS2::GenerateDMULT( EN64Reg rs, EN64Reg rt )
{
	mMultIsValid = false;

	EPs2Reg	reg_lo_a( GetRegisterAndLoadLo( rs, Ps2Reg_A0 ) );
	EPs2Reg	reg_hi_a( GetRegisterAndLoadHi( rs, Ps2Reg_A1 ) );
	EPs2Reg	reg_lo_b( GetRegisterAndLoadLo( rt, Ps2Reg_A2 ) );
	EPs2Reg	reg_hi_b( GetRegisterAndLoadHi( rt, Ps2Reg_A3 ) );

	if( reg_lo_a != Ps2Reg_A0 ) OR( Ps2Reg_A0, Ps2Reg_R0, reg_lo_a);
	if( reg_hi_a != Ps2Reg_A1 ) OR( Ps2Reg_A1, Ps2Reg_R0, reg_hi_a);
	if( reg_lo_b != Ps2Reg_A2 ) OR( Ps2Reg_A2, Ps2Reg_R0, reg_lo_b);
	if( reg_hi_b != Ps2Reg_A3 ) OR( Ps2Reg_A3, Ps2Reg_R0, reg_hi_b);

	JAL( CCodeLabel( (void*)_DMULT ), true );
}


//

inline void	CCodeGeneratorPS2::GenerateDADDIU( EN64Reg rt, EN64Reg rs, s16 immediate )
{
	if( rs == N64Reg_R0 )
	{
		SetRegister32s( rt, immediate );
	}
	else
	{
		EPs2Reg	reg_lo_d( GetRegisterNoLoadLo( rt, Ps2Reg_V0 ) );
		EPs2Reg	reg_lo_a( GetRegisterAndLoadLo( rs, Ps2Reg_A0 ) );

		if(reg_lo_d == reg_lo_a)
		{
			ADDIU( Ps2Reg_A0, reg_lo_a, immediate );
			SLTU( Ps2Reg_A1, Ps2Reg_A0, reg_lo_a );		// Overflowed?
			OR( reg_lo_d, Ps2Reg_A0, Ps2Reg_R0 );
		}
		else
		{
			ADDIU( reg_lo_d, reg_lo_a, immediate );
			SLTU( Ps2Reg_A1, reg_lo_d, reg_lo_a );		// Overflowed?
		}
		StoreRegisterLo( rt, reg_lo_d );

		EPs2Reg	reg_hi_d( GetRegisterNoLoadHi( rt, Ps2Reg_V0 ) );
		EPs2Reg	reg_hi_a( GetRegisterAndLoadHi( rs, Ps2Reg_A0 ) );

		ADDU( reg_hi_d, Ps2Reg_A1, reg_hi_a );		// Add on overflow
		StoreRegisterHi( rt, reg_hi_d );
	}
}


//

inline void	CCodeGeneratorPS2::GenerateDADDU( EN64Reg rd, EN64Reg rs, EN64Reg rt )
{
	//gGPR[ op_code.rd ]._u64 = gGPR[ op_code.rt ]._u64 + gGPR[ op_code.rs ]._u64

	//890c250:	00c41021 	addu	d_lo,a_lo,b_lo
	//890c254:	0046402b 	sltu	t0,d_lo,a_lo
	//890c260:	ad220000 	sw		d_lo,0(t1)

	//890c258:	00e51821 	addu	d_hi,a_hi,b_hi
	//890c25c:	01031821 	addu	d_hi,t0,d_hi
	//890c268:	ad230004 	sw		d_hi,4(t1)

	EPs2Reg	reg_lo_d( GetRegisterNoLoadLo( rd, Ps2Reg_V0 ) );
	EPs2Reg	reg_lo_a( GetRegisterAndLoadLo( rs, Ps2Reg_A0 ) );
	EPs2Reg	reg_lo_b( GetRegisterAndLoadLo( rt, Ps2Reg_A1 ) );

	ADDU( reg_lo_d, reg_lo_a, reg_lo_b );
    //Assumes that just one of the source regs are the same as DST
	SLTU( Ps2Reg_V1, reg_lo_d, reg_lo_d == reg_lo_a ? reg_lo_b : reg_lo_a  );		// Overflowed?
	StoreRegisterLo( rd, reg_lo_d );

	EPs2Reg	reg_hi_d( GetRegisterNoLoadHi( rd, Ps2Reg_V0 ) );
	EPs2Reg	reg_hi_a( GetRegisterAndLoadHi( rs, Ps2Reg_A0 ) );
	EPs2Reg	reg_hi_b( GetRegisterAndLoadHi( rt, Ps2Reg_A1 ) );

	ADDU( reg_hi_d, reg_hi_a, reg_hi_b );
	ADDU( reg_hi_d, Ps2Reg_V1, reg_hi_d );		// Add on overflow
	StoreRegisterHi( rd, reg_hi_d );
}


//

inline void	CCodeGeneratorPS2::GenerateAND( EN64Reg rd, EN64Reg rs, EN64Reg rt )
{
	//gGPR[ op_code.rd ]._u64 = gGPR[ op_code.rs ]._u64 & gGPR[ op_code.rt ]._u64;

	bool HiIsDone = false;

	if (mRegisterCache.IsKnownValue(rs, 1) & mRegisterCache.IsKnownValue(rt, 1))
	{
		SetRegister(rd, 1, mRegisterCache.GetKnownValue(rs, 1)._u32 & mRegisterCache.GetKnownValue(rt, 1)._u32 );
		HiIsDone = true;
	}
	else if ((mRegisterCache.IsKnownValue(rs, 1) & (mRegisterCache.GetKnownValue(rs, 1)._u32 == 0)) |
		     (mRegisterCache.IsKnownValue(rt, 1) & (mRegisterCache.GetKnownValue(rt, 1)._u32 == 0)) )
	{
		SetRegister(rd, 1, 0 );
		HiIsDone = true;
	}
	else if( mRegisterCache.IsKnownValue(rt, 1) & (mRegisterCache.GetKnownValue(rt, 1)._s32 == -1) )
	{
		EPs2Reg reg_hi_d( GetRegisterNoLoadHi( rd, Ps2Reg_V0 ) );
		LoadRegisterHi( reg_hi_d, rs );
		StoreRegisterHi( rd, reg_hi_d );
		HiIsDone = true;
	}

	if (mRegisterCache.IsKnownValue(rs, 0) & mRegisterCache.IsKnownValue(rt, 0))
	{
		SetRegister(rd, 0, mRegisterCache.GetKnownValue(rs, 0)._u32 & mRegisterCache.GetKnownValue(rt, 0)._u32 );
	}
	else if ((rs == N64Reg_R0) | (rt == N64Reg_R0))
	{
		SetRegister64( rd, 0, 0 );
	}
	else
	{
		// XXXX or into dest register
		EPs2Reg	reg_lo_d( GetRegisterNoLoadLo( rd, Ps2Reg_V0 ) );
		EPs2Reg	reg_lo_a( GetRegisterAndLoadLo( rs, Ps2Reg_V0 ) );
		EPs2Reg	reg_lo_b( GetRegisterAndLoadLo( rt, Ps2Reg_A0 ) );
		AND( reg_lo_d, reg_lo_a, reg_lo_b );
		StoreRegisterLo( rd, reg_lo_d );

		if(!HiIsDone)
		{
			EPs2Reg	reg_hi_d( GetRegisterNoLoadHi( rd, Ps2Reg_V0 ) );
			EPs2Reg	reg_hi_a( GetRegisterAndLoadHi( rs, Ps2Reg_V0 ) );
			EPs2Reg	reg_hi_b( GetRegisterAndLoadHi( rt, Ps2Reg_A0 ) );
			AND( reg_hi_d, reg_hi_a, reg_hi_b );
			StoreRegisterHi( rd, reg_hi_d );
		}
	}
}


//

void	CCodeGeneratorPS2::GenerateOR( EN64Reg rd, EN64Reg rs, EN64Reg rt )
{
	//gGPR[ op_code.rd ]._u64 = gGPR[ op_code.rs ]._u64 | gGPR[ op_code.rt ]._u64;

	bool HiIsDone = false;

	if (mRegisterCache.IsKnownValue(rs, 1) & mRegisterCache.IsKnownValue(rt, 1))
	{
		SetRegister(rd, 1, mRegisterCache.GetKnownValue(rs, 1)._u32 | mRegisterCache.GetKnownValue(rt, 1)._u32 );
		HiIsDone = true;
	}
	else if ((mRegisterCache.IsKnownValue(rs, 1) & (mRegisterCache.GetKnownValue(rs, 1)._s32 == -1)) |
		     (mRegisterCache.IsKnownValue(rt, 1) & (mRegisterCache.GetKnownValue(rt, 1)._s32 == -1)) )
	{
		SetRegister(rd, 1, ~0 );
		HiIsDone = true;
	}

	if (mRegisterCache.IsKnownValue(rs, 0) & mRegisterCache.IsKnownValue(rt, 0))
	{
		SetRegister(rd, 0, mRegisterCache.GetKnownValue(rs, 0)._u32 | mRegisterCache.GetKnownValue(rt, 0)._u32);
		return;
	}

	if( rs == N64Reg_R0 )
	{
		// This doesn't seem to happen
		/*if (mRegisterCache.IsKnownValue(rt, 1))
		{
			SetRegister(rd, 1, mRegisterCache.GetKnownValue(rt, 1)._u32 );
			HiIsDone = true;
		}
		*/
		if(mRegisterCache.IsKnownValue(rt, 0))
		{
			SetRegister64(rd,
				mRegisterCache.GetKnownValue(rt, 0)._u32, mRegisterCache.GetKnownValue(rt, 1)._u32);
			return;
		}

		// This case rarely seems to happen...
		// As RS is zero, the OR is just a copy of RT to RD.
		// Try to avoid loading into a temp register if the dest is cached
		EPs2Reg reg_lo_d( GetRegisterNoLoadLo( rd, Ps2Reg_V0 ) );
		LoadRegisterLo( reg_lo_d, rt );
		StoreRegisterLo( rd, reg_lo_d );
		if(!HiIsDone)
		{
			EPs2Reg reg_hi_d( GetRegisterNoLoadHi( rd, Ps2Reg_V0 ) );
			LoadRegisterHi( reg_hi_d, rt );
			StoreRegisterHi( rd, reg_hi_d );
		}
	}
	else if( rt == N64Reg_R0 )
	{
		if (mRegisterCache.IsKnownValue(rs, 1))
		{
			SetRegister(rd, 1, mRegisterCache.GetKnownValue(rs, 1)._u32 );
			HiIsDone = true;
		}

		if(mRegisterCache.IsKnownValue(rs, 0))
		{
			SetRegister64(rd, mRegisterCache.GetKnownValue(rs, 0)._u32,
				mRegisterCache.GetKnownValue(rs, 1)._u32);
			return;
		}

		// As RT is zero, the OR is just a copy of RS to RD.
		// Try to avoid loading into a temp register if the dest is cached
		EPs2Reg reg_lo_d( GetRegisterNoLoadLo( rd, Ps2Reg_V0 ) );
		LoadRegisterLo( reg_lo_d, rs );
		StoreRegisterLo( rd, reg_lo_d );
		if(!HiIsDone)
		{
			EPs2Reg reg_hi_d( GetRegisterNoLoadHi( rd, Ps2Reg_V0 ) );
			LoadRegisterHi( reg_hi_d, rs );
			StoreRegisterHi( rd, reg_hi_d );
		}
	}
	else
	{

		EPs2Reg	reg_lo_d( GetRegisterNoLoadLo( rd, Ps2Reg_V0 ) );
		EPs2Reg	reg_lo_a( GetRegisterAndLoadLo( rs, Ps2Reg_V0 ) );
		EPs2Reg	reg_lo_b( GetRegisterAndLoadLo( rt, Ps2Reg_A0 ) );
		OR( reg_lo_d, reg_lo_a, reg_lo_b );
		StoreRegisterLo( rd, reg_lo_d );

		if(!HiIsDone)
		{
			if( mRegisterCache.IsKnownValue(rs, 1) & (mRegisterCache.GetKnownValue(rs, 1)._u32 == 0) )
			{
				EPs2Reg reg_hi_d( GetRegisterNoLoadHi( rd, Ps2Reg_V0 ) );
				LoadRegisterHi( reg_hi_d, rt );
				StoreRegisterHi( rd, reg_hi_d );
			}
			else if( mRegisterCache.IsKnownValue(rt, 1) & (mRegisterCache.GetKnownValue(rt, 1)._u32 == 0) )
			{
				EPs2Reg reg_hi_d( GetRegisterNoLoadHi( rd, Ps2Reg_V0 ) );
				LoadRegisterHi( reg_hi_d, rs );
				StoreRegisterHi( rd, reg_hi_d );
			}
			else
			{
				EPs2Reg	reg_hi_d( GetRegisterNoLoadHi( rd, Ps2Reg_V0 ) );
				EPs2Reg	reg_hi_a( GetRegisterAndLoadHi( rs, Ps2Reg_V0 ) );
				EPs2Reg	reg_hi_b( GetRegisterAndLoadHi( rt, Ps2Reg_A0 ) );
				OR( reg_hi_d, reg_hi_a, reg_hi_b );
				StoreRegisterHi( rd, reg_hi_d );
			}
		}
	}
}


//

inline void	CCodeGeneratorPS2::GenerateXOR( EN64Reg rd, EN64Reg rs, EN64Reg rt )
{
	//gGPR[ op_code.rd ]._u64 = gGPR[ op_code.rs ]._u64 ^ gGPR[ op_code.rt ]._u64;

	bool HiIsDone = false;

	if (mRegisterCache.IsKnownValue(rs, 1) & mRegisterCache.IsKnownValue(rt, 1))
	{
		SetRegister(rd, 1, mRegisterCache.GetKnownValue(rs, 1)._u32 ^ mRegisterCache.GetKnownValue(rt, 1)._u32 );
		HiIsDone = true;
	}
	else if ((mRegisterCache.IsKnownValue(rs, 1) & (mRegisterCache.GetKnownValue(rs, 1)._u32 == 0)) )
	{
		EPs2Reg reg_hi_d( GetRegisterNoLoadHi( rd, Ps2Reg_V0 ) );
		LoadRegisterHi( reg_hi_d, rt );
		StoreRegisterHi( rd, reg_hi_d );
		HiIsDone = true;
	}
	else if ((mRegisterCache.IsKnownValue(rt, 1) & (mRegisterCache.GetKnownValue(rt, 1)._u32 == 0)) )
	{
		EPs2Reg reg_hi_d( GetRegisterNoLoadHi( rd, Ps2Reg_V0 ) );
		LoadRegisterHi( reg_hi_d, rs );
		StoreRegisterHi( rd, reg_hi_d );
		HiIsDone = true;
	}

	if (mRegisterCache.IsKnownValue(rs, 0) & mRegisterCache.IsKnownValue(rt, 0))
	{
		SetRegister(rd, 0, mRegisterCache.GetKnownValue(rs, 0)._u32 ^ mRegisterCache.GetKnownValue(rt, 0)._u32);
		return;
	}

	EPs2Reg	reg_lo_d( GetRegisterNoLoadLo( rd, Ps2Reg_V0 ) );
	EPs2Reg	reg_lo_a( GetRegisterAndLoadLo( rs, Ps2Reg_V0 ) );
	EPs2Reg	reg_lo_b( GetRegisterAndLoadLo( rt, Ps2Reg_A0 ) );
	XOR( reg_lo_d, reg_lo_a, reg_lo_b );
	StoreRegisterLo( rd, reg_lo_d );

	if(!HiIsDone)
	{
		EPs2Reg	reg_hi_d( GetRegisterNoLoadHi( rd, Ps2Reg_V0 ) );
		EPs2Reg	reg_hi_a( GetRegisterAndLoadHi( rs, Ps2Reg_V0 ) );
		EPs2Reg	reg_hi_b( GetRegisterAndLoadHi( rt, Ps2Reg_A0 ) );
		XOR( reg_hi_d, reg_hi_a, reg_hi_b );
		StoreRegisterHi( rd, reg_hi_d );
	}
}


//

inline void	CCodeGeneratorPS2::GenerateNOR( EN64Reg rd, EN64Reg rs, EN64Reg rt )
{
	//gGPR[ op_code.rd ]._u64 = ~(gGPR[ op_code.rs ]._u64 | gGPR[ op_code.rt ]._u64);

	bool HiIsDone = false;

	if (mRegisterCache.IsKnownValue(rs, 1) & mRegisterCache.IsKnownValue(rt, 1))
	{
		SetRegister(rd, 1, ~(mRegisterCache.GetKnownValue(rs, 1)._u32 | mRegisterCache.GetKnownValue(rt, 1)._u32) );
		HiIsDone = true;
	}

	if (mRegisterCache.IsKnownValue(rs, 0) & mRegisterCache.IsKnownValue(rt, 0))
	{
		SetRegister(rd, 0, ~(mRegisterCache.GetKnownValue(rs, 0)._u32 | mRegisterCache.GetKnownValue(rt, 0)._u32) );
		return;
	}

	EPs2Reg	reg_lo_d( GetRegisterNoLoadLo( rd, Ps2Reg_V0 ) );
	EPs2Reg	reg_lo_a( GetRegisterAndLoadLo( rs, Ps2Reg_V0 ) );
	EPs2Reg	reg_lo_b( GetRegisterAndLoadLo( rt, Ps2Reg_A0 ) );
	NOR( reg_lo_d, reg_lo_a, reg_lo_b );
	StoreRegisterLo( rd, reg_lo_d );

	if(!HiIsDone)
	{
		EPs2Reg	reg_hi_d( GetRegisterNoLoadHi( rd, Ps2Reg_V0 ) );
		EPs2Reg	reg_hi_a( GetRegisterAndLoadHi( rs, Ps2Reg_V0 ) );
		EPs2Reg	reg_hi_b( GetRegisterAndLoadHi( rt, Ps2Reg_A0 ) );
		NOR( reg_hi_d, reg_hi_a, reg_hi_b );
		StoreRegisterHi( rd, reg_hi_d );
	}
}


//

inline void	CCodeGeneratorPS2::GenerateSLT( EN64Reg rd, EN64Reg rs, EN64Reg rt )
{
#ifdef ENABLE_64BIT
	// Because we have a branch here, we need to make sure that we have a consistent view
	// of the registers regardless of whether we take it or not. We pull in the lo halves of
	// the registers here so that they're Valid regardless of whether we take the branch or not
	PrepareCachedRegisterLo( rs );
	PrepareCachedRegisterLo( rt );

	// If possible, we write directly into the destination register. We have to be careful though -
	// if the destination register is the same as either of the source registers we have to use
	// a temporary instead, to avoid overwriting the contents.
	EPs2Reg reg_lo_d( GetRegisterNoLoadLo( rd, Ps2Reg_V0 ) );

	if((rd == rs) | (rd == rt))
	{
		reg_lo_d = Ps2Reg_V0;
	}

	EPs2Reg	reg_hi_a( GetRegisterAndLoadHi( rs, Ps2Reg_V0 ) );
	EPs2Reg	reg_hi_b( GetRegisterAndLoadHi( rt, Ps2Reg_A0 ) );

	CJumpLocation	branch( BNE( reg_hi_a, reg_hi_b, CCodeLabel(nullptr), false ) );
	SLT( reg_lo_d, reg_hi_a, reg_hi_b );		// In branch delay slot

	// If the branch was not taken, it means that the high part of the registers was equal, so compare bottom half
	EPs2Reg	reg_lo_a( GetRegisterAndLoadLo( rs, Ps2Reg_V0 ) );
	EPs2Reg	reg_lo_b( GetRegisterAndLoadLo( rt, Ps2Reg_A0 ) );

	SLT( reg_lo_d, reg_lo_a, reg_lo_b );

	// Patch up the branch
	PatchJumpLong( branch, GetAssemblyBuffer()->GetLabel() );

#else
	/*
	if (mRegisterCache.IsKnownValue(rs, 0) & mRegisterCache.IsKnownValue(rt, 0))
	{
		SetRegister32s(rd, (mRegisterCache.GetKnownValue(rs, 0)._s32 < mRegisterCache.GetKnownValue(rt, 0)._s32) );
		return;
	}
	*/
	EPs2Reg reg_lo_d( GetRegisterNoLoadLo( rd, Ps2Reg_V0 ) );
	EPs2Reg	reg_lo_a( GetRegisterAndLoadLo( rs, Ps2Reg_V0 ) );
	EPs2Reg	reg_lo_b( GetRegisterAndLoadLo( rt, Ps2Reg_A0 ) );

	SLT( reg_lo_d, reg_lo_a, reg_lo_b );

#endif

	UpdateRegister( rd, reg_lo_d, URO_HI_CLEAR );
}


//

inline void	CCodeGeneratorPS2::GenerateSLTU( EN64Reg rd, EN64Reg rs, EN64Reg rt )
{
#ifdef ENABLE_64BIT
	// Because we have a branch here, we need to make sure that we have a consistant view
	// of the registers regardless of whether we take it or not. We pull in the lo halves of
	// the registers here so that they're Valid regardless of whether we take the branch or not
	PrepareCachedRegisterLo( rs );
	PrepareCachedRegisterLo( rt );

	// If possible, we write directly into the destination register. We have to be careful though -
	// if the destination register is the same as either of the source registers we have to use
	// a temporary instead, to avoid overwriting the contents.
	EPs2Reg reg_lo_d( GetRegisterNoLoadLo( rd, Ps2Reg_V0 ) );

	if((rd == rs) | (rd == rt))
	{
		reg_lo_d = Ps2Reg_V0;
	}

	EPs2Reg	reg_hi_a( GetRegisterAndLoadHi( rs, Ps2Reg_V0 ) );
	EPs2Reg	reg_hi_b( GetRegisterAndLoadHi( rt, Ps2Reg_A0 ) );

	CJumpLocation	branch( BNE( reg_hi_a, reg_hi_b, CCodeLabel(nullptr), false ) );
	SLTU( reg_lo_d, reg_hi_a, reg_hi_b );		// In branch delay slot

	// If the branch was not taken, it means that the high part of the registers was equal, so compare bottom half
	EPs2Reg	reg_lo_a( GetRegisterAndLoadLo( rs, Ps2Reg_V0 ) );
	EPs2Reg	reg_lo_b( GetRegisterAndLoadLo( rt, Ps2Reg_A0 ) );

	SLTU( reg_lo_d, reg_lo_a, reg_lo_b );

	// Patch up the branch
	PatchJumpLong( branch, GetAssemblyBuffer()->GetLabel() );

#else
	/*
	if (mRegisterCache.IsKnownValue(rs, 0) & mRegisterCache.IsKnownValue(rt, 0))
	{
		SetRegister32s(rd, (mRegisterCache.GetKnownValue(rs, 0)._u32 < mRegisterCache.GetKnownValue(rt, 0)._u32) );
		return;
	}
	*/
	EPs2Reg reg_lo_d( GetRegisterNoLoadLo( rd, Ps2Reg_V0 ) );
	EPs2Reg	reg_lo_a( GetRegisterAndLoadLo( rs, Ps2Reg_V0 ) );
	EPs2Reg	reg_lo_b( GetRegisterAndLoadLo( rt, Ps2Reg_A0 ) );

	SLTU( reg_lo_d, reg_lo_a, reg_lo_b );

#endif

	UpdateRegister( rd, reg_lo_d, URO_HI_CLEAR );
}


//

inline void	CCodeGeneratorPS2::GenerateADDIU( EN64Reg rt, EN64Reg rs, s16 immediate )
{
	//gGPR[op_code.rt]._s64 = (s64)(s32)(gGPR[op_code.rs]._s32_0 + (s32)(s16)op_code.immediate);

	if( rs == N64Reg_R0 )
	{
		SetRegister32s( rt, immediate );
	}
	else if(mRegisterCache.IsKnownValue( rs, 0 ))
	{
		s32		known_value( mRegisterCache.GetKnownValue( rs, 0 )._s32 + (s32)immediate );
		SetRegister32s( rt, known_value );
	}
	else
	{

		EPs2Reg dst_reg( GetRegisterNoLoadLo( rt, Ps2Reg_V0 ) );
		EPs2Reg	src_reg( GetRegisterAndLoadLo( rs, Ps2Reg_A0 ) );
		ADDIU( dst_reg, src_reg, immediate );

		UpdateRegister( rt, dst_reg, URO_HI_SIGN_EXTEND );
	}
}


//

inline void	CCodeGeneratorPS2::GenerateANDI( EN64Reg rt, EN64Reg rs, u16 immediate )
{
	//gGPR[op_code.rt]._u64 = gGPR[op_code.rs]._u64 & (u64)(u16)op_code.immediate;

	if(mRegisterCache.IsKnownValue( rs, 0 ))
	{
		s32		known_value_lo( mRegisterCache.GetKnownValue( rs, 0 )._u32 & (u32)immediate );
		s32		known_value_hi( 0 );

		SetRegister64( rt, known_value_lo, known_value_hi );
	}
	else
	{
		EPs2Reg dst_reg( GetRegisterNoLoadLo( rt, Ps2Reg_V0 ) );
		EPs2Reg	src_reg( GetRegisterAndLoadLo( rs, Ps2Reg_A0 ) );
		ANDI( dst_reg, src_reg, immediate );

		UpdateRegister( rt, dst_reg, URO_HI_CLEAR );
	}
}


//

inline void	CCodeGeneratorPS2::GenerateORI( EN64Reg rt, EN64Reg rs, u16 immediate )
{
	//gGPR[op_code.rt]._u64 = gGPR[op_code.rs]._u64 | (u64)(u16)op_code.immediate;

	if( rs == N64Reg_R0 )
	{
			// If we're oring again 0, then we're just setting a constant value
			SetRegister64( rt, immediate, 0 );
	}
	else if(mRegisterCache.IsKnownValue( rs, 0 ))
	{
		s32		known_value_lo( mRegisterCache.GetKnownValue( rs, 0 )._u32 | (u32)immediate );
		s32		known_value_hi( mRegisterCache.GetKnownValue( rs, 1 )._u32 );

		SetRegister64( rt, known_value_lo, known_value_hi );
	}
	else
	{
		EPs2Reg dst_reg( GetRegisterNoLoadLo( rt, Ps2Reg_V0 ) );
		EPs2Reg	src_reg( GetRegisterAndLoadLo( rs, Ps2Reg_A0 ) );
		ORI( dst_reg, src_reg, immediate );
		StoreRegisterLo( rt, dst_reg );

#ifdef ENABLE_64BIT	//This is out of spec but seems we can always ignore copying the hi part
		// If the source/dest regs are different we need to copy the high bits across
		if(rt != rs)
		{
			EPs2Reg dst_reg_hi( GetRegisterNoLoadHi( rt, Ps2Reg_V0 ) );
			LoadRegisterHi( dst_reg_hi, rs );
			StoreRegisterHi( rt, dst_reg_hi );
		}
#endif
	}
}


//

inline void	CCodeGeneratorPS2::GenerateXORI( EN64Reg rt, EN64Reg rs, u16 immediate )
{
	//gGPR[op_code.rt]._u64 = gGPR[op_code.rs]._u64 ^ (u64)(u16)op_code.immediate;

	if(mRegisterCache.IsKnownValue( rs, 0 ))
	{
		s32		known_value_lo( mRegisterCache.GetKnownValue( rs, 0 )._u32 ^ (u32)immediate );
		s32		known_value_hi( mRegisterCache.GetKnownValue( rs, 1 )._u32 );

		SetRegister64( rt, known_value_lo, known_value_hi );
	}
	else
	{
		EPs2Reg dst_reg( GetRegisterNoLoadLo( rt, Ps2Reg_V0 ) );
		EPs2Reg	src_reg( GetRegisterAndLoadLo( rs, Ps2Reg_A0 ) );
		XORI( dst_reg, src_reg, immediate );
		StoreRegisterLo( rt, dst_reg );

#ifdef ENABLE_64BIT	//This is out of spec but seems we can always ignore copying the hi part
		// If the source/dest regs are different we need to copy the high bits across
		// (if they are the same, we're xoring 0 to the top half which is essentially a NOP)
		if(rt != rs)
		{
			EPs2Reg dst_reg_hi( GetRegisterNoLoadHi( rt, Ps2Reg_V0 ) );
			LoadRegisterHi( dst_reg_hi, rs );
			StoreRegisterHi( rt, dst_reg_hi );
		}
#endif
	}
}


//

inline void	CCodeGeneratorPS2::GenerateLUI( EN64Reg rt, s16 immediate )
{
	//gGPR[op_code.rt]._s64 = (s64)(s32)((s32)(s16)op_code.immediate<<16);

	SetRegister32s( rt, s32( immediate ) << 16 );
}


//

inline void	CCodeGeneratorPS2::GenerateSLTI( EN64Reg rt, EN64Reg rs, s16 immediate )
{
#ifdef ENABLE_64BIT
	// Because we have a branch here, we need to make sure that we have a consistant view
	// of the register regardless of whether we take it or not. We pull in the lo halves of
	// the register here so that it's Valid regardless of whether we take the branch or not
	PrepareCachedRegisterLo( rs );

	// If possible, we write directly into the destination register. We have to be careful though -
	// if the destination register is the same as either of the source registers we have to use
	// a temporary instead, to avoid overwriting the contents.
	EPs2Reg reg_lo_d( GetRegisterNoLoadLo( rt, Ps2Reg_V0 ) );

	if(rt == rs)
	{
		reg_lo_d = Ps2Reg_V0;
	}

	CJumpLocation	branch;

	EPs2Reg		reg_hi_a( GetRegisterAndLoadHi( rs, Ps2Reg_V0 ) );
	if( immediate >= 0 )
	{
		// Positive data - we can avoid a contant load here
		branch = BNE( reg_hi_a, Ps2Reg_R0, CCodeLabel(nullptr), false );
		SLTI( reg_lo_d, reg_hi_a, 0x0000 );		// In branch delay slot
	}
	else
	{
		// Negative data
		LoadConstant( Ps2Reg_A0, -1 );
		branch = BNE( reg_hi_a, Ps2Reg_A0, CCodeLabel(nullptr), false );
		SLTI( reg_lo_d, reg_hi_a, 0xffff );		// In branch delay slot
	}
	// If the branch was not taken, it means that the high part of the registers was equal, so compare bottom half
	EPs2Reg	reg_lo_a( GetRegisterAndLoadLo( rs, Ps2Reg_V0 ) );

	//Potential bug!!!
	//If using 64bit mode this might need to be SLTIU since sign is already checked in hi word //Strmn
	SLTI( reg_lo_d, reg_lo_a, immediate );

	// Patch up the branch
	PatchJumpLong( branch, GetAssemblyBuffer()->GetLabel() );

#else
	/*
	if (mRegisterCache.IsKnownValue(rs, 0))
	{
		SetRegister32s( rt, (mRegisterCache.GetKnownValue(rs, 0)._s32 < (s32)immediate) );
		return;
	}
	*/
	EPs2Reg reg_lo_d( GetRegisterNoLoadLo( rt, Ps2Reg_V0 ) );
	EPs2Reg	reg_lo_a( GetRegisterAndLoadLo( rs, Ps2Reg_V0 ) );

	SLTI( reg_lo_d, reg_lo_a, immediate );

#endif

	UpdateRegister( rt, reg_lo_d, URO_HI_CLEAR );
}


//

inline void	CCodeGeneratorPS2::GenerateSLTIU( EN64Reg rt, EN64Reg rs, s16 immediate )
{
#ifdef ENABLE_64BIT
	// Because we have a branch here, we need to make sure that we have a consistent view
	// of the register regardless of whether we take it or not. We pull in the lo halves of
	// the register here so that it's Valid regardless of whether we take the branch or not
	PrepareCachedRegisterLo( rs );

	// If possible, we write directly into the destination register. We have to be careful though -
	// if the destination register is the same as either of the source registers we have to use
	// a temporary instead, to avoid overwriting the contents.
	EPs2Reg reg_lo_d( GetRegisterNoLoadLo( rt, Ps2Reg_V0 ) );

	if(rt == rs)
	{
		reg_lo_d = Ps2Reg_V0;
	}

	CJumpLocation	branch;

	EPs2Reg		reg_hi_a( GetRegisterAndLoadHi( rs, Ps2Reg_V0 ) );
	if( immediate >= 0 )
	{
		// Positive data - we can avoid a contant load here
		branch = BNE( reg_hi_a, Ps2Reg_R0, CCodeLabel(nullptr), false );
		SLTIU( reg_lo_d, reg_hi_a, 0x0000 );		// In branch delay slot
	}
	else
	{
		// Negative data
		LoadConstant( Ps2Reg_A0, -1 );
		branch = BNE( reg_hi_a, Ps2Reg_A0, CCodeLabel(nullptr), false );
		SLTIU( reg_lo_d, reg_hi_a, 0xffff );		// In branch delay slot
	}
	// If the branch was not taken, it means that the high part of the registers was equal, so compare bottom half
	EPs2Reg	reg_lo_a( GetRegisterAndLoadLo( rs, Ps2Reg_V0 ) );

	SLTIU( reg_lo_d, reg_lo_a, immediate );

	// Patch up the branch
	PatchJumpLong( branch, GetAssemblyBuffer()->GetLabel() );

#else
	/*
	if (mRegisterCache.IsKnownValue(rs, 0))
	{
		SetRegister32s( rt, (mRegisterCache.GetKnownValue(rs, 0)._u32 < (u32)immediate) );
		return;
	}
	*/
	EPs2Reg reg_lo_d( GetRegisterNoLoadLo( rt, Ps2Reg_V0 ) );
	EPs2Reg	reg_lo_a( GetRegisterAndLoadLo( rs, Ps2Reg_V0 ) );

	SLTIU( reg_lo_d, reg_lo_a, immediate );

#endif

	UpdateRegister( rt, reg_lo_d, URO_HI_CLEAR );
}


//

inline void	CCodeGeneratorPS2::GenerateSLL( EN64Reg rd, EN64Reg rt, u32 sa )
{
	//gGPR[ op_code.rd ]._s64 = (s64)(s32)( (gGPR[ op_code.rt ]._u32_0 << op_code.sa) & 0xFFFFFFFF );
	if (mRegisterCache.IsKnownValue(rt, 0))
	{
		SetRegister32s(rd, (s32)(mRegisterCache.GetKnownValue(rt, 0)._u32 << sa));
		return;
	}

	EPs2Reg reg_lo_rd( GetRegisterNoLoadLo( rd, Ps2Reg_V0 ) );
	EPs2Reg	reg_lo_rt( GetRegisterAndLoadLo( rt, Ps2Reg_V0 ) );

	SLL( reg_lo_rd, reg_lo_rt, sa );
	UpdateRegister( rd, reg_lo_rd, URO_HI_SIGN_EXTEND );
}


//Double Shift Left Logical (Doom64)

inline void	CCodeGeneratorPS2::GenerateDSLL( EN64Reg rd, EN64Reg rt, u32 sa )
{
	//gGPR[ op_code.rd ]._s64 = (s64)(s32)( (gGPR[ op_code.rt ]._u32_0 << op_code.sa) & 0xFFFFFFFF );
	//if (mRegisterCache.IsKnownValue(rt, 0))
	//{
	//	SetRegister32s(rd, (s32)(mRegisterCache.GetKnownValue(rt, 0)._u32 << sa));
	//	return;
	//}

	EPs2Reg reg_lo_rd( GetRegisterNoLoadLo( rd, Ps2Reg_V0 ) );
	EPs2Reg reg_hi_rd( GetRegisterNoLoadHi( rd, Ps2Reg_A0 ) );
	EPs2Reg	reg_lo_rt( GetRegisterAndLoadLo( rt, Ps2Reg_V0 ) );
	EPs2Reg	reg_hi_rt( GetRegisterAndLoadHi( rt, Ps2Reg_A0 ) );

	SLL( reg_hi_rd, reg_hi_rt, sa );
	if( sa != 0 )
	{
		SRL( Ps2Reg_A2, reg_lo_rt, 32-sa);
		OR( reg_hi_rd, reg_hi_rd, Ps2Reg_A2);
	}
	SLL( reg_lo_rd, reg_lo_rt, sa );
	StoreRegisterLo( rd, reg_lo_rd);
	StoreRegisterHi( rd, reg_hi_rd);
}


//

inline void	CCodeGeneratorPS2::GenerateDSLL32( EN64Reg rd, EN64Reg rt, u32 sa )
{
	EPs2Reg reg_hi_rd( GetRegisterNoLoadHi( rd, Ps2Reg_V0 ) );
	EPs2Reg	reg_lo_rt( GetRegisterAndLoadLo( rt, Ps2Reg_V0 ) );

	SLL( reg_hi_rd, reg_lo_rt, sa );
	SetRegister( rd, 0, 0 );	//Zero lo part
	StoreRegisterHi( rd, reg_hi_rd );	//Store result
}


//

inline void	CCodeGeneratorPS2::GenerateSRL( EN64Reg rd, EN64Reg rt, u32 sa )
{
	//gGPR[ op_code.rd ]._s64 = (s64)(s32)( gGPR[ op_code.rt ]._u32_0 >> op_code.sa );
	if (mRegisterCache.IsKnownValue(rt, 0))
	{
		SetRegister32s(rd, (s32)(mRegisterCache.GetKnownValue(rt, 0)._u32 >> sa));
		return;
	}

	EPs2Reg reg_lo_rd( GetRegisterNoLoadLo( rd, Ps2Reg_V0 ) );
	EPs2Reg	reg_lo_rt( GetRegisterAndLoadLo( rt, Ps2Reg_V0 ) );

	SRL( reg_lo_rd, reg_lo_rt, sa );
	UpdateRegister( rd, reg_lo_rd, URO_HI_SIGN_EXTEND );
}


//Double Shift Right Arithmetic (Doom64)

inline void	CCodeGeneratorPS2::GenerateDSRA( EN64Reg rd, EN64Reg rt, u32 sa )
{
	//gGPR[ op_code.rd ]._s64 = (s64)(s32)( gGPR[ op_code.rt ]._s32_0 >> op_code.sa );
	//if (mRegisterCache.IsKnownValue(rt, 0))
	//{
	//	SetRegister32s(rd, mRegisterCache.GetKnownValue(rt, 0)._s32 >> sa);
	//	return;
	//}

	EPs2Reg reg_lo_rd( GetRegisterNoLoadLo( rd, Ps2Reg_V0 ) );
	EPs2Reg reg_hi_rd( GetRegisterNoLoadHi( rd, Ps2Reg_A0 ) );
	EPs2Reg	reg_lo_rt( GetRegisterAndLoadLo( rt, Ps2Reg_V0 ) );
	EPs2Reg	reg_hi_rt( GetRegisterAndLoadHi( rt, Ps2Reg_A0 ) );

	SRL( reg_lo_rd, reg_lo_rt, sa );
	if( sa != 0 )
	{
		SLL( Ps2Reg_A2, reg_hi_rt, 32-sa );
		OR( reg_lo_rd, reg_lo_rd, Ps2Reg_A2);
	}
	SRA( reg_hi_rd, reg_hi_rt, sa );
	StoreRegisterLo( rd, reg_lo_rd);
	StoreRegisterHi( rd, reg_hi_rd);
}


//

inline void	CCodeGeneratorPS2::GenerateDSRA32( EN64Reg rd, EN64Reg rt, u32 sa )
{
	EPs2Reg reg_lo_rd( GetRegisterNoLoadLo( rd, Ps2Reg_V0 ) );
	EPs2Reg	reg_hi_rt( GetRegisterAndLoadHi( rt, Ps2Reg_V0 ) );

	SRA( reg_lo_rd, reg_hi_rt, sa );
	UpdateRegister( rd, reg_lo_rd, URO_HI_SIGN_EXTEND );
}


//

inline void	CCodeGeneratorPS2::GenerateSRA( EN64Reg rd, EN64Reg rt, u32 sa )
{
	//gGPR[ op_code.rd ]._s64 = (s64)(s32)( gGPR[ op_code.rt ]._s32_0 >> op_code.sa );
	if (mRegisterCache.IsKnownValue(rt, 0))
	{
		SetRegister32s(rd, mRegisterCache.GetKnownValue(rt, 0)._s32 >> sa);
		return;
	}

	EPs2Reg reg_lo_rd( GetRegisterNoLoadLo( rd, Ps2Reg_V0 ) );
	EPs2Reg	reg_lo_rt( GetRegisterAndLoadLo( rt, Ps2Reg_V0 ) );

	SRA( reg_lo_rd, reg_lo_rt, sa );
	UpdateRegister( rd, reg_lo_rd, URO_HI_SIGN_EXTEND );
}


//

inline void	CCodeGeneratorPS2::GenerateSLLV( EN64Reg rd, EN64Reg rs, EN64Reg rt )
{
	//gGPR[ op_code.rd ]._s64 = (s64)(s32)( (gGPR[ op_code.rt ]._u32_0 << ( gGPR[ op_code.rs ]._u32_0 & 0x1F ) ) & 0xFFFFFFFF );
	if (mRegisterCache.IsKnownValue(rs, 0)
		& mRegisterCache.IsKnownValue(rt, 0))
	{
		SetRegister32s(rd, (s32)(mRegisterCache.GetKnownValue(rt, 0)._u32 <<
			(mRegisterCache.GetKnownValue(rs, 0)._u32 & 0x1F)));
		return;
	}

	// PSP sllv does exactly the same op as n64- no need for masking
	EPs2Reg reg_lo_rd( GetRegisterNoLoadLo( rd, Ps2Reg_V0 ) );
	EPs2Reg reg_lo_rs( GetRegisterAndLoadLo( rs, Ps2Reg_A0 ) );
	EPs2Reg	reg_lo_rt( GetRegisterAndLoadLo( rt, Ps2Reg_V0 ) );

	SLLV( reg_lo_rd, reg_lo_rs, reg_lo_rt );
	UpdateRegister( rd, reg_lo_rd, URO_HI_SIGN_EXTEND );
}


//

inline void	CCodeGeneratorPS2::GenerateSRLV( EN64Reg rd, EN64Reg rs, EN64Reg rt )
{
	//gGPR[ op_code.rd ]._s64 = (s64)(s32)( gGPR[ op_code.rt ]._u32_0 >> ( gGPR[ op_code.rs ]._u32_0 & 0x1F ) );
	if (mRegisterCache.IsKnownValue(rs, 0)
		& mRegisterCache.IsKnownValue(rt, 0))
	{
		SetRegister32s(rd, (s32)(mRegisterCache.GetKnownValue(rt, 0)._u32 >>
			(mRegisterCache.GetKnownValue(rs, 0)._u32 & 0x1F)));
		return;
	}

	// PSP srlv does exactly the same op as n64- no need for masking
	EPs2Reg reg_lo_rd( GetRegisterNoLoadLo( rd, Ps2Reg_V0 ) );
	EPs2Reg reg_lo_rs( GetRegisterAndLoadLo( rs, Ps2Reg_A0 ) );
	EPs2Reg	reg_lo_rt( GetRegisterAndLoadLo( rt, Ps2Reg_V0 ) );

	SRLV( reg_lo_rd, reg_lo_rs, reg_lo_rt );
	UpdateRegister( rd, reg_lo_rd, URO_HI_SIGN_EXTEND );
}


//

inline void	CCodeGeneratorPS2::GenerateSRAV( EN64Reg rd, EN64Reg rs, EN64Reg rt )
{
	//gGPR[ op_code.rd ]._s64 = (s64)(s32)( gGPR[ op_code.rt ]._s32_0 >> ( gGPR[ op_code.rs ]._u32_0 & 0x1F ) );
	if (mRegisterCache.IsKnownValue(rs, 0)
		& mRegisterCache.IsKnownValue(rt, 0))
	{
		SetRegister32s(rd, (s32)(mRegisterCache.GetKnownValue(rt, 0)._s32 >>
			(mRegisterCache.GetKnownValue(rs, 0)._u32 & 0x1F)));
		return;
	}

	// PSP srlv does exactly the same op as n64- no need for masking
	EPs2Reg reg_lo_rd( GetRegisterNoLoadLo( rd, Ps2Reg_V0 ) );
	EPs2Reg reg_lo_rs( GetRegisterAndLoadLo( rs, Ps2Reg_A0 ) );
	EPs2Reg	reg_lo_rt( GetRegisterAndLoadLo( rt, Ps2Reg_V0 ) );

	SRAV( reg_lo_rd, reg_lo_rs, reg_lo_rt );
	UpdateRegister( rd, reg_lo_rd, URO_HI_SIGN_EXTEND );
}


//

inline void	CCodeGeneratorPS2::GenerateLB( u32 address, bool set_branch_delay, EN64Reg rt, EN64Reg base, s32 offset )
{
	EPs2Reg	reg_dst( GetRegisterNoLoadLo( rt, Ps2Reg_V0 ) );	// Use V0 to avoid copying return value if reg is not cached

	GenerateLoad( address, reg_dst, base, offset, OP_LB, 3, set_branch_delay ? ReadBitsDirectBD_s8 : ReadBitsDirect_s8 );	// NB this fills the whole of reg_dst

	UpdateRegister( rt, reg_dst, URO_HI_SIGN_EXTEND );
}


//

inline void	CCodeGeneratorPS2::GenerateLBU( u32 address, bool set_branch_delay, EN64Reg rt, EN64Reg base, s32 offset )
{
	EPs2Reg	reg_dst( GetRegisterNoLoadLo( rt, Ps2Reg_V0 ) );	// Use V0 to avoid copying return value if reg is not cached

	GenerateLoad( address, reg_dst, base, offset, OP_LBU, 3, set_branch_delay ? ReadBitsDirectBD_u8 : ReadBitsDirect_u8 );	// NB this fills the whole of reg_dst

	UpdateRegister( rt, reg_dst, URO_HI_CLEAR );
}


//

inline void	CCodeGeneratorPS2::GenerateLH( u32 address, bool set_branch_delay, EN64Reg rt, EN64Reg base, s32 offset )
{
	EPs2Reg	reg_dst( GetRegisterNoLoadLo( rt, Ps2Reg_V0 ) );	// Use V0 to avoid copying return value if reg is not cached

	GenerateLoad( address, reg_dst, base, offset, OP_LH, 2, set_branch_delay ? ReadBitsDirect_s16 : ReadBitsDirect_s16 );	// NB this fills the whole of reg_dst

	UpdateRegister( rt, reg_dst, URO_HI_SIGN_EXTEND );
}


//

inline void	CCodeGeneratorPS2::GenerateLHU( u32 address, bool set_branch_delay, EN64Reg rt, EN64Reg base, s32 offset )
{
	EPs2Reg	reg_dst( GetRegisterNoLoadLo( rt, Ps2Reg_V0 ) );	// Use V0 to avoid copying return value if reg is not cached

	GenerateLoad( address, reg_dst, base, offset, OP_LHU, 2, set_branch_delay ? ReadBitsDirectBD_u16 : ReadBitsDirect_u16 );	// NB this fills the whole of reg_dst

	UpdateRegister( rt, reg_dst, URO_HI_CLEAR );
}


//

inline void	CCodeGeneratorPS2::GenerateLW( u32 address, bool set_branch_delay, EN64Reg rt, EN64Reg base, s16 offset )
{
	EPs2Reg	reg_dst( GetRegisterNoLoadLo( rt, Ps2Reg_V0 ) );	// Use V0 to avoid copying return value if reg is not cached

	// This is for San Francisco 2049, otherwise it crashes when the race is about to start.
	if(rt == N64Reg_R0)
	{
		#ifdef DAEDALUS_DEBUG_CONSOLE
		DAEDALUS_ERROR("Attempted write to r0!");
		#endif
		return;
	}

	GenerateLoad( address, reg_dst, base, offset, OP_LW, 0, set_branch_delay ? ReadBitsDirectBD_u32 : ReadBitsDirect_u32 );

	UpdateRegister( rt, reg_dst, URO_HI_SIGN_EXTEND );
}


//

inline void	CCodeGeneratorPS2::GenerateLD( u32 address, bool set_branch_delay, EN64Reg rt, EN64Reg base, s16 offset )
{
	EPs2Reg	reg_dst_hi( GetRegisterNoLoadHi( rt, Ps2Reg_V0 ) );	// Use V0 to avoid copying return value if reg is not cached
	GenerateLoad( address, reg_dst_hi, base, offset, OP_LW, 0, set_branch_delay ? ReadBitsDirectBD_u32 : ReadBitsDirect_u32 );
	StoreRegisterHi( rt, reg_dst_hi );

	//Adding 4 to offset should be quite safe //Corn
	EPs2Reg	reg_dst_lo( GetRegisterNoLoadLo( rt, Ps2Reg_V0 ) );	// Use V0 to avoid copying return value if reg is not cached
	GenerateLoad( address, reg_dst_lo, base, offset + 4, OP_LW, 0, set_branch_delay ? ReadBitsDirectBD_u32 : ReadBitsDirect_u32 );
	StoreRegisterLo( rt, reg_dst_lo );
}


//

inline void	CCodeGeneratorPS2::GenerateLWC1( u32 address, bool set_branch_delay, u32 ft, EN64Reg base, s32 offset )
{
	EN64FloatReg	n64_ft = EN64FloatReg( ft );
	EPs2FloatReg	ps2_ft = EPs2FloatReg( n64_ft );// 1:1 Mapping

	//u32 address = (u32)( gGPR[op_code.base]._s32_0 + (s32)(s16)op_code.immediate );
	//value = Read32Bits(address);
	//gCPUState.FPU[op_code.ft]._s32_0 = value;

#ifdef ENABLE_LWC1
	//Since LW and LWC1 have same format we do a small hack that let us load directly to
	//the float reg without passing to the CPU first that saves us the MFC1 instruction //Corn
	GenerateLoad( address, (EPs2Reg)ps2_ft, base, offset, OP_LWC1, 0, set_branch_delay ? ReadBitsDirectBD_u32 : ReadBitsDirect_u32 );
	UpdateFloatRegister( n64_ft );
#else
	// TODO: Actually perform LWC1 here
	EPs2Reg	reg_dst( Ps2Reg_V0 );				// GenerateLoad is slightly more efficient when using V0
	GenerateLoad( address, reg_dst, base, offset, OP_LW, 0, set_branch_delay ? ReadBitsDirectBD_u32 : ReadBitsDirect_u32 );

	//SetVar( &gCPUState.FPU[ ft ]._u32, reg_dst );
	MTC1( ps2_ft, reg_dst );
	UpdateFloatRegister( n64_ft );
#endif
}


//

#ifdef ENABLE_LDC1
inline void	CCodeGeneratorPS2::GenerateLDC1( u32 address, bool set_branch_delay, u32 ft, EN64Reg base, s32 offset )
{
	EPs2Reg		reg_base( GetRegisterAndLoadLo( base, Ps2Reg_A0 ) );
	ADDU( Ps2Reg_A0, reg_base, gMemoryBaseReg );

	EN64FloatReg	n64_ft = EN64FloatReg( ft + 1 );
	EPs2FloatReg	ps2_ft = EPs2FloatReg( n64_ft );// 1:1 Mapping
	//GenerateLoad( address, (EPs2Reg)ps2_ft, base, offset, OP_LWC1, 0, set_branch_delay ? ReadBitsDirectBD_u32 : ReadBitsDirect_u32 );
	LWC1( ps2_ft, Ps2Reg_A0, offset);
	mRegisterCache.MarkFPAsValid( n64_ft, true );
	mRegisterCache.MarkFPAsDirty( n64_ft, true );

	n64_ft = EN64FloatReg( ft );
	ps2_ft = EPs2FloatReg( n64_ft );// 1:1 Mapping
	//GenerateLoad( address, (EPs2Reg)ps2_ft, base, offset + 4, OP_LWC1, 0, set_branch_delay ? ReadBitsDirectBD_u32 : ReadBitsDirect_u32 );
	LWC1( ps2_ft, Ps2Reg_A0, offset+4);
	mRegisterCache.MarkFPAsValid( n64_ft, true );
	mRegisterCache.MarkFPAsDirty( n64_ft, true );
	mRegisterCache.MarkFPAsSim( n64_ft, false );
}
#endif

#ifdef ENABLE_LWR_LWL

//Unaligned load Left //Corn

inline void	CCodeGeneratorPS2::GenerateLWL( u32 address, bool set_branch_delay, EN64Reg rt, EN64Reg base, s16 offset )
{

	//Will return the value in Ps2Reg_V0 and the shift in Ps2Reg_A3
	GenerateLoad( address, Ps2Reg_V0, base, offset, OP_LWL, 0, set_branch_delay ? ReadBitsDirectBD_u32 : ReadBitsDirect_u32 );

	EPs2Reg	reg_dst( GetRegisterAndLoadLo( rt, Ps2Reg_A0 ) );

	SLL( Ps2Reg_A3, Ps2Reg_A3, 0x3 );    // shift *= 8
    NOR( Ps2Reg_A2, Ps2Reg_R0, Ps2Reg_R0 );    // Load 0xFFFFFFFF
    SLLV( Ps2Reg_A2, Ps2Reg_A3, Ps2Reg_A2 );    // mask <<= shift
    NOR( Ps2Reg_A2, Ps2Reg_A2, Ps2Reg_R0 ); // mask != mask
    SLLV( Ps2Reg_A3, Ps2Reg_A3, Ps2Reg_V0 );    // memory <<= shift
    AND( reg_dst, reg_dst, Ps2Reg_A2 ); // reg_dst & mask
    OR( reg_dst, reg_dst, Ps2Reg_A3 );    // reg_dst | memory

	UpdateRegister( rt, reg_dst, URO_HI_SIGN_EXTEND );
}


//Unaligned load Right //Corn

inline void	CCodeGeneratorPS2::GenerateLWR( u32 address, bool set_branch_delay, EN64Reg rt, EN64Reg base, s16 offset )
{
	//Will return the value in Ps2Reg_V0 and the shift in Ps2Reg_A3
	GenerateLoad( address, Ps2Reg_V0, base, offset, OP_LWL, 0, set_branch_delay ? ReadBitsDirectBD_u32 : ReadBitsDirect_u32 );

	EPs2Reg	reg_dst( GetRegisterAndLoadLo( rt, Ps2Reg_A0 ) );

	SLL( Ps2Reg_A3, Ps2Reg_A3, 0x3 );    // shift *= 8
    ADDIU( Ps2Reg_A2, Ps2Reg_R0, 0xFF00 );    // Load 0xFFFFFF00
    SLLV( Ps2Reg_A2, Ps2Reg_A3, Ps2Reg_A2 );    // mask <<= shift
    AND( reg_dst, reg_dst, Ps2Reg_A2 ); // reg_dst & mask
    XORI( Ps2Reg_A3, Ps2Reg_A3, 0x18 ); // shift ^= shift (eg. invert shift)
    SRLV( Ps2Reg_A3, Ps2Reg_A3, Ps2Reg_V0 );    // memory >>= shift
    OR( reg_dst, reg_dst, Ps2Reg_A3 );    // reg_dst | memory

	UpdateRegister( rt, reg_dst, URO_HI_SIGN_EXTEND );
}
#endif


//

inline void	CCodeGeneratorPS2::GenerateSB( u32 current_pc, bool set_branch_delay, EN64Reg rt, EN64Reg base, s32 offset )
{
	EPs2Reg		reg_value( GetRegisterAndLoadLo( rt, Ps2Reg_A1 ) );

	GenerateStore( current_pc, reg_value, base, offset, OP_SB, 3, set_branch_delay ? WriteBitsDirectBD_u8 : WriteBitsDirect_u8 );
}


//

inline void	CCodeGeneratorPS2::GenerateSH( u32 current_pc, bool set_branch_delay, EN64Reg rt, EN64Reg base, s32 offset )
{
	EPs2Reg		reg_value( GetRegisterAndLoadLo( rt, Ps2Reg_A1 ) );

	GenerateStore( current_pc, reg_value, base, offset, OP_SH, 2, set_branch_delay ? WriteBitsDirectBD_u16 : WriteBitsDirect_u16 );
}


//

inline void	CCodeGeneratorPS2::GenerateSW( u32 current_pc, bool set_branch_delay, EN64Reg rt, EN64Reg base, s32 offset )
{
	EPs2Reg		reg_value( GetRegisterAndLoadLo( rt, Ps2Reg_A1 ) );

	GenerateStore( current_pc, reg_value, base, offset, OP_SW, 0, set_branch_delay ? WriteBitsDirectBD_u32 : WriteBitsDirect_u32 );
}


//

inline void	CCodeGeneratorPS2::GenerateSD( u32 current_pc, bool set_branch_delay, EN64Reg rt, EN64Reg base, s32 offset )
{
	EPs2Reg		reg_value_hi( GetRegisterAndLoadHi( rt, Ps2Reg_A1 ) );
	GenerateStore( current_pc, reg_value_hi, base, offset, OP_SW, 0, set_branch_delay ? WriteBitsDirectBD_u32 : WriteBitsDirect_u32 );

	//Adding 4 to offset should be quite safe //Corn
	EPs2Reg		reg_value_lo( GetRegisterAndLoadLo( rt, Ps2Reg_A1 ) );
	GenerateStore( current_pc, reg_value_lo, base, offset + 4, OP_SW, 0, set_branch_delay ? WriteBitsDirectBD_u32 : WriteBitsDirect_u32 );
}


//

inline void	CCodeGeneratorPS2::GenerateSWC1( u32 current_pc, bool set_branch_delay, u32 ft, EN64Reg base, s32 offset )
{
	EN64FloatReg	n64_ft = EN64FloatReg( ft );
	EPs2FloatReg	ps2_ft( GetFloatRegisterAndLoad( n64_ft ) );

#ifdef ENABLE_SWC1
	//Since SW and SWC1 have same format we do a small hack that let us save directly from
	//the float reg without passing to the CPU first that saves us the MFC1 instruction //Corn
	GenerateStore( current_pc, (EPs2Reg)ps2_ft, base, offset, OP_SWC1, 0, set_branch_delay ? WriteBitsDirectBD_u32 : WriteBitsDirect_u32 );
#else
	MFC1( Ps2Reg_A1, ps2_ft );
	GenerateStore( current_pc, Ps2Reg_A1, base, offset, OP_SW, 0, set_branch_delay ? WriteBitsDirectBD_u32 : WriteBitsDirect_u32 );
#endif
}


//

#ifdef ENABLE_SDC1
inline void	CCodeGeneratorPS2::GenerateSDC1( u32 current_pc, bool set_branch_delay, u32 ft, EN64Reg base, s32 offset )
{
	EN64FloatReg	n64_ft_sig = EN64FloatReg( ft );
	EPs2FloatReg	ps2_ft_sig = GetFloatRegisterAndLoad( n64_ft_sig );
	EN64FloatReg	n64_ft = EN64FloatReg( ft + 1 );
	EPs2FloatReg	ps2_ft( GetFloatRegisterAndLoad( EN64FloatReg(n64_ft + 1) ) );

	//GenerateStore( current_pc, (EPs2Reg)ps2_ft, base, offset, OP_SWC1, 0, set_branch_delay ? WriteBitsDirectBD_u32 : WriteBitsDirect_u32 );
	//GenerateStore( current_pc, (EPs2Reg)ps2_ft_sig, base, offset + 4, OP_SWC1, 0, set_branch_delay ? WriteBitsDirectBD_u32 : WriteBitsDirect_u32 );
	EPs2Reg		reg_base( GetRegisterAndLoadLo( base, Ps2Reg_A0 ) );
	ADDU( Ps2Reg_A0, reg_base, gMemoryBaseReg );
	SWC1( ps2_ft, Ps2Reg_A0, offset);
	SWC1( ps2_ft_sig, Ps2Reg_A0, offset+4);
}
#endif


//

#ifdef ENABLE_SWR_SWL
inline void	CCodeGeneratorPS2::GenerateSWL( u32 current_pc, bool set_branch_delay, EN64Reg rt, EN64Reg base, s32 offset )
{
	EPs2Reg		reg_value( GetRegisterAndLoadLo( rt, Ps2Reg_A1 ) );

	GenerateStore( current_pc, reg_value, base, offset, OP_SWL, 3, set_branch_delay ? WriteBitsDirectBD_u32 : WriteBitsDirect_u32 );
}


//

inline void	CCodeGeneratorPS2::GenerateSWR( u32 current_pc, bool set_branch_delay, EN64Reg rt, EN64Reg base, s32 offset )
{
	EPs2Reg		reg_value( GetRegisterAndLoadLo( rt, Ps2Reg_A1 ) );

	GenerateStore( current_pc, reg_value, base, offset, OP_SWR, 3, set_branch_delay ? WriteBitsDirectBD_u32 : WriteBitsDirect_u32 );
}
#endif


//

inline void	CCodeGeneratorPS2::GenerateMFC1( EN64Reg rt, u32 fs )
{
	//gGPR[ op_code.rt ]._s64 = (s64)(s32)gCPUState.FPU[fs]._s32_0

	EPs2Reg			reg_dst( GetRegisterNoLoadLo( rt, Ps2Reg_V0 ) );
	EN64FloatReg	n64_fs = EN64FloatReg( fs );
	EPs2FloatReg	ps2_fs( GetFloatRegisterAndLoad( n64_fs ) );

	MFC1( reg_dst, ps2_fs );

	//Needs to be sign extended(or breaks DKR)
	UpdateRegister( rt, reg_dst, URO_HI_SIGN_EXTEND );
}


//

inline void	CCodeGeneratorPS2::GenerateMTC1( u32 fs, EN64Reg rt )
{
	//gCPUState.FPU[fs]._s32_0 = gGPR[ op_code.rt ]._s32_0;

	EPs2Reg			ps2_rt( GetRegisterAndLoadLo( rt, Ps2Reg_V0 ) );
	EN64FloatReg	n64_fs = EN64FloatReg( fs );
	EPs2FloatReg	ps2_fs = EPs2FloatReg( n64_fs );//1:1 Mapping

	MTC1( ps2_fs, ps2_rt );
	UpdateFloatRegister( n64_fs );
}


//

inline void	CCodeGeneratorPS2::GenerateCFC1( EN64Reg rt, u32 fs )
{
	if( (fs == 0) | (fs == 31) )
	{
		EPs2Reg			reg_dst( GetRegisterNoLoadLo( rt, Ps2Reg_V0 ) );

		GetVar( reg_dst, &gCPUState.FPUControl[ fs ]._u32 );
#ifdef ENABLE_64BIT
		UpdateRegister( rt, reg_dst, URO_HI_SIGN_EXTEND );
#else
		StoreRegisterLo( rt, reg_dst );
#endif
	}
}


//

// This breaks BombreMan Hero, not sure how to fix it though, maybe because we don't set the rounding mode?
// http://forums.daedalusx64.com/viewtopic.php?f=77&t=2258&p=36374&hilit=GenerateCTC1#p36374
//
void	CCodeGeneratorPS2::GenerateCTC1( u32 fs, EN64Reg rt )
{
	#ifdef DAEDALUS_ENABLE_ASSERTS
	DAEDALUS_ASSERT( fs == 31, "CTC1 register is invalid");
	#endif
	EPs2Reg			ps2_rt_lo( GetRegisterAndLoadLo( rt, Ps2Reg_V0 ) );
	SetVar( &gCPUState.FPUControl[ fs ]._u32, ps2_rt_lo );

	// Only the lo part is ever set - Salvy
	//EPs2Reg			ps2_rt_hi( GetRegisterAndLoadHi( rt, Ps2Reg_V0 ) );
	//SetVar( &gCPUState.FPUControl[ fs ]._u32_1, ps2_rt_hi );

	//XXXX TODO:
	// Change the rounding mode
}


//

inline void	CCodeGeneratorPS2::GenerateBEQ( EN64Reg rs, EN64Reg rt, const SBranchDetails * p_branch, CJumpLocation * p_branch_jump )
{
	#ifdef DAEDALUS_ENABLE_ASSERTS
	DAEDALUS_ASSERT( p_branch != nullptr, "No branch details?" );
	DAEDALUS_ASSERT( p_branch->Direct, "Indirect branch for BEQ?" );
	#endif
	// One or other of these may be r0 - we don't really care for optimisation purposes though
	// as ultimately the register load regs factored out
	EPs2Reg		reg_a( GetRegisterAndLoadLo( rs, Ps2Reg_V0 ) );
	EPs2Reg		reg_b( GetRegisterAndLoadLo( rt, Ps2Reg_A0 ) );

	// XXXX This may actually need to be a 64 bit compare, but this is what R4300.cpp does

	if( p_branch->ConditionalBranchTaken )
	{
		// Flip the sign of the test -
		*p_branch_jump = BNE( reg_a, reg_b, CCodeLabel(nullptr), true );
	}
	else
	{
		*p_branch_jump = BEQ( reg_a, reg_b, CCodeLabel(nullptr), true );
	}
}


//

inline void	CCodeGeneratorPS2::GenerateBNE( EN64Reg rs, EN64Reg rt, const SBranchDetails * p_branch, CJumpLocation * p_branch_jump )
{
	#ifdef DAEDALUS_ENABLE_ASSERTS
	DAEDALUS_ASSERT( p_branch != nullptr, "No branch details?" );
	DAEDALUS_ASSERT( p_branch->Direct, "Indirect branch for BNE?" );
	#endif
	// One or other of these may be r0 - we don't really care for optimisation purposes though
	// as ultimately the register load regs factored out
	EPs2Reg		reg_a( GetRegisterAndLoadLo( rs, Ps2Reg_V0 ) );
	EPs2Reg		reg_b( GetRegisterAndLoadLo( rt, Ps2Reg_A0 ) );

	// XXXX This may actually need to be a 64 bit compare, but this is what R4300.cpp does

	if( p_branch->ConditionalBranchTaken )
	{
		// Flip the sign of the test -
		*p_branch_jump = BEQ( reg_a, reg_b, CCodeLabel(nullptr), true );
	}
	else
	{
		*p_branch_jump = BNE( reg_a, reg_b, CCodeLabel(nullptr), true );
	}
}


//

inline void	CCodeGeneratorPS2::GenerateBLEZ( EN64Reg rs, const SBranchDetails * p_branch, CJumpLocation * p_branch_jump )
{
	#ifdef DAEDALUS_ENABLE_ASSERTS
	DAEDALUS_ASSERT( p_branch != nullptr, "No branch details?" );
	DAEDALUS_ASSERT( p_branch->Direct, "Indirect branch for BLEZ?" );
	#endif
	EPs2Reg		reg_a( GetRegisterAndLoadLo( rs, Ps2Reg_V0 ) );

	// XXXX This may actually need to be a 64 bit compare, but this is what R4300.cpp does

	if( p_branch->ConditionalBranchTaken )
	{
		// Flip the sign of the test -
		*p_branch_jump = BGTZ( reg_a, CCodeLabel(nullptr), true );
	}
	else
	{
		*p_branch_jump = BLEZ( reg_a, CCodeLabel(nullptr), true );
	}
}


//

inline void	CCodeGeneratorPS2::GenerateBGTZ( EN64Reg rs, const SBranchDetails * p_branch, CJumpLocation * p_branch_jump )
{
	#ifdef DAEDALUS_ENABLE_ASSERTS
	DAEDALUS_ASSERT( p_branch != nullptr, "No branch details?" );
	DAEDALUS_ASSERT( p_branch->Direct, "Indirect branch for BGTZ?" );
	#endif
	EPs2Reg		reg_lo( GetRegisterAndLoadLo( rs, Ps2Reg_V0 ) );

	//64bit compare is needed for DK64 or DK can walk trough walls
	if(g_ROM.GameHacks == DK64)
	{
#if 0
		//Do a full 64 bit compare //Corn
		SRL( Ps2Reg_V0, reg_lo, 1);	//Free MSB sign bit
		ANDI( Ps2Reg_V1, reg_lo, 1);	//Extract LSB bit
		OR( Ps2Reg_V0, Ps2Reg_V0, Ps2Reg_V1);	//Merge with OR
		EPs2Reg		reg_hi( GetRegisterAndLoadHi( rs, Ps2Reg_V1 ) );
		OR( Ps2Reg_V1, Ps2Reg_V0, reg_hi);	//Merge lo with hi part of register with sign bit
#else
		//This takes some shortcuts //Corn
		EPs2Reg		reg_hi( GetRegisterAndLoadHi( rs, Ps2Reg_V1 ) );
		OR( Ps2Reg_V1, reg_lo, reg_hi);
#endif
		if( p_branch->ConditionalBranchTaken )
		{
			// Flip the sign of the test -
			*p_branch_jump = BLEZ( Ps2Reg_V1, CCodeLabel(nullptr), true );
		}
		else
		{
			*p_branch_jump = BGTZ( Ps2Reg_V1, CCodeLabel(nullptr), true );
		}
	}
	else
	{
		//Do a fast 32 bit compare //Corn
		if( p_branch->ConditionalBranchTaken )
		{
			// Flip the sign of the test -
			*p_branch_jump = BLEZ( reg_lo, CCodeLabel(nullptr), true );
		}
		else
		{
			*p_branch_jump = BGTZ( reg_lo, CCodeLabel(nullptr), true );
		}
	}
}


//

inline void	CCodeGeneratorPS2::GenerateBLTZ( EN64Reg rs, const SBranchDetails * p_branch, CJumpLocation * p_branch_jump )
{
	#ifdef DAEDALUS_ENABLE_ASSERTS
	DAEDALUS_ASSERT( p_branch != nullptr, "No branch details?" );
	DAEDALUS_ASSERT( p_branch->Direct, "Indirect branch for BLTZ?" );
	#endif
	EPs2Reg		reg_a( GetRegisterAndLoadLo( rs, Ps2Reg_V0 ) );

	// XXXX This should actually need to be a 64 bit compare???

	if( p_branch->ConditionalBranchTaken )
	{
		// Flip the sign of the test -
		*p_branch_jump = BGEZ( reg_a, CCodeLabel(nullptr), true );
	}
	else
	{
		*p_branch_jump = BLTZ( reg_a, CCodeLabel(nullptr), true );
	}
}


//

inline void	CCodeGeneratorPS2::GenerateBGEZ( EN64Reg rs, const SBranchDetails * p_branch, CJumpLocation * p_branch_jump )
{
	#ifdef DAEDALUS_ENABLE_ASSERTS
	DAEDALUS_ASSERT( p_branch != nullptr, "No branch details?" );
	DAEDALUS_ASSERT( p_branch->Direct, "Indirect branch for BGEZ?" );
	#endif
	EPs2Reg		reg_a( GetRegisterAndLoadLo( rs, Ps2Reg_V0 ) );

	// XXXX This should actually need to be a 64 bit compare???

	if( p_branch->ConditionalBranchTaken )
	{
		// Flip the sign of the test -
		*p_branch_jump = BLTZ( reg_a, CCodeLabel(nullptr), true );
	}
	else
	{
		*p_branch_jump = BGEZ( reg_a, CCodeLabel(nullptr), true );
	}
}


//

inline void	CCodeGeneratorPS2::GenerateBC1F( const SBranchDetails * p_branch, CJumpLocation * p_branch_jump )
{
	#ifdef DAEDALUS_ENABLE_ASSERTS
	DAEDALUS_ASSERT( p_branch != nullptr, "No branch details?" );
	DAEDALUS_ASSERT( p_branch->Direct, "Indirect branch for BC1F?" );
	#endif
	//If compare was done in current fragment then use BC1T or BC1F directly //Corn
	if( mFloatCMPIsValid )
	{
		if( p_branch->ConditionalBranchTaken )
		{
			// Flip the sign of the test -
			*p_branch_jump = BC1T( CCodeLabel(nullptr), true );
		}
		else
		{
			*p_branch_jump = BC1F( CCodeLabel(nullptr), true );
		}
	}
	else
	{
		GetVar( Ps2Reg_V0, &gCPUState.FPUControl[31]._u32 );
#if 0
		EXT( Ps2Reg_V0, Ps2Reg_V0, 0, 23 );	//Extract condition bit (true/false)
#else
		LoadConstant( Ps2Reg_A0, FPCSR_C );
		AND( Ps2Reg_V0, Ps2Reg_V0, Ps2Reg_A0 );
#endif
		if( p_branch->ConditionalBranchTaken )
		{
			// Flip the sign of the test -
			*p_branch_jump = BNE( Ps2Reg_V0, Ps2Reg_R0, CCodeLabel(nullptr), true );
		}
		else
		{
			*p_branch_jump = BEQ( Ps2Reg_V0, Ps2Reg_R0, CCodeLabel(nullptr), true );
		}
	}
}


//

inline void	CCodeGeneratorPS2::GenerateBC1T( const SBranchDetails * p_branch, CJumpLocation * p_branch_jump )
{
	#ifdef DAEDALUS_ENABLE_ASSERTS
	DAEDALUS_ASSERT( p_branch != nullptr, "No branch details?" );
	DAEDALUS_ASSERT( p_branch->Direct, "Indirect branch for BC1T?" );
	#endif
	//If compare was done in current fragment then use BC1T or BC1F directly //Corn
	if( mFloatCMPIsValid )
	{
		if( p_branch->ConditionalBranchTaken )
		{
			// Flip the sign of the test -
			*p_branch_jump = BC1F( CCodeLabel(nullptr), true );
		}
		else
		{
			*p_branch_jump = BC1T( CCodeLabel(nullptr), true );
		}
	}
	else
	{
		GetVar( Ps2Reg_V0, &gCPUState.FPUControl[31]._u32 );
#if 0
		EXT( Ps2Reg_V0, Ps2Reg_V0, 0, 23 );	//Extract condition bit (true/false)
#else
		LoadConstant( Ps2Reg_A0, FPCSR_C );
		AND( Ps2Reg_V0, Ps2Reg_V0, Ps2Reg_A0 );
#endif
		if( p_branch->ConditionalBranchTaken )
		{
			// Flip the sign of the test -
			*p_branch_jump = BEQ( Ps2Reg_V0, Ps2Reg_R0, CCodeLabel(nullptr), true );
		}
		else
		{
			*p_branch_jump = BNE( Ps2Reg_V0, Ps2Reg_R0, CCodeLabel(nullptr), true );
		}
	}
}


//

inline void	CCodeGeneratorPS2::GenerateADD_D_Sim( u32 fd, u32 fs, u32 ft )
{
	EN64FloatReg	n64_fs = EN64FloatReg( fs );
	EN64FloatReg	n64_ft = EN64FloatReg( ft );
	EN64FloatReg	n64_fd = EN64FloatReg( fd );

	EPs2FloatReg	ps2_fd_sig = EPs2FloatReg( n64_fd );//1:1 Mapping
	EPs2FloatReg	ps2_fd = EPs2FloatReg( n64_fd + 1 );//1:1 Mapping
	EPs2FloatReg	ps2_fs( GetSimFloatRegisterAndLoad( n64_fs ) );
	EPs2FloatReg	ps2_ft( GetSimFloatRegisterAndLoad( n64_ft ) );

	//Use float now instead of double :)
	ADD_S( ps2_fd, ps2_fs, ps2_ft );
	if( !mRegisterCache.IsFPSim( n64_fd ) ) MOV_S( ps2_fd_sig, EPs2FloatReg( fs ));	//Copy signature as well if needed

	UpdateSimDoubleRegister( n64_fd );
}


//

inline void	CCodeGeneratorPS2::GenerateSUB_D_Sim( u32 fd, u32 fs, u32 ft )
{
	EN64FloatReg	n64_fs = EN64FloatReg( fs );
	EN64FloatReg	n64_ft = EN64FloatReg( ft );
	EN64FloatReg	n64_fd = EN64FloatReg( fd );

	EPs2FloatReg	ps2_fd_sig = EPs2FloatReg( n64_fd );//1:1 Mapping
	EPs2FloatReg	ps2_fd = EPs2FloatReg( n64_fd + 1 );//1:1 Mapping
	EPs2FloatReg	ps2_fs( GetSimFloatRegisterAndLoad( n64_fs ) );
	EPs2FloatReg	ps2_ft( GetSimFloatRegisterAndLoad( n64_ft ) );

	//Use float now instead of double :)
	SUB_S( ps2_fd, ps2_fs, ps2_ft );
	if( !mRegisterCache.IsFPSim( n64_fd ) ) MOV_S( ps2_fd_sig, EPs2FloatReg( fs ));	//Copy signature as well if needed

	UpdateSimDoubleRegister( n64_fd );
}


//

inline void	CCodeGeneratorPS2::GenerateMUL_D_Sim( u32 fd, u32 fs, u32 ft )
{
	EN64FloatReg	n64_fs = EN64FloatReg( fs );
	EN64FloatReg	n64_ft = EN64FloatReg( ft );
	EN64FloatReg	n64_fd = EN64FloatReg( fd );

	EPs2FloatReg	ps2_fd_sig = EPs2FloatReg( n64_fd );//1:1 Mapping
	EPs2FloatReg	ps2_fd = EPs2FloatReg( n64_fd + 1 );//1:1 Mapping
	EPs2FloatReg	ps2_fs( GetSimFloatRegisterAndLoad( n64_fs ) );
	EPs2FloatReg	ps2_ft( GetSimFloatRegisterAndLoad( n64_ft ) );

	//Use float now instead of double :)
	MUL_S( ps2_fd, ps2_fs, ps2_ft );
	if( !mRegisterCache.IsFPSim( n64_fd ) ) MOV_S( ps2_fd_sig, EPs2FloatReg( fs ));	//Copy signature as well if needed

	UpdateSimDoubleRegister( n64_fd );
}


//

inline void	CCodeGeneratorPS2::GenerateDIV_D_Sim( u32 fd, u32 fs, u32 ft )
{
	EN64FloatReg	n64_fs = EN64FloatReg( fs );
	EN64FloatReg	n64_ft = EN64FloatReg( ft );
	EN64FloatReg	n64_fd = EN64FloatReg( fd );

	EPs2FloatReg	ps2_fd_sig = EPs2FloatReg( n64_fd );//1:1 Mapping
	EPs2FloatReg	ps2_fd = EPs2FloatReg( n64_fd + 1 );//1:1 Mapping
	EPs2FloatReg	ps2_fs( GetSimFloatRegisterAndLoad( n64_fs ) );
	EPs2FloatReg	ps2_ft( GetSimFloatRegisterAndLoad( n64_ft ) );

	//Use float now instead of double :)
	DIV_S( ps2_fd, ps2_fs, ps2_ft );
	if( !mRegisterCache.IsFPSim( n64_fd ) ) MOV_S( ps2_fd_sig, EPs2FloatReg( fs ));	//Copy signature as well if needed

	UpdateSimDoubleRegister( n64_fd );
}


//

inline void	CCodeGeneratorPS2::GenerateSQRT_D_Sim( u32 fd, u32 fs )
{
	EN64FloatReg	n64_fs = EN64FloatReg( fs );
	EN64FloatReg	n64_fd = EN64FloatReg( fd );

	EPs2FloatReg	ps2_fd_sig = EPs2FloatReg( n64_fd );//1:1 Mapping
	EPs2FloatReg	ps2_fd = EPs2FloatReg( n64_fd + 1 );//1:1 Mapping
	EPs2FloatReg	ps2_fs( GetSimFloatRegisterAndLoad( n64_fs ) );

	//Use float now instead of double :)
	SQRT_S( ps2_fd, ps2_fs );
	if( !mRegisterCache.IsFPSim( n64_fd ) ) MOV_S( ps2_fd_sig, EPs2FloatReg( fs ));	//Copy signature as well if needed

	UpdateSimDoubleRegister( n64_fd );
}


//

inline void	CCodeGeneratorPS2::GenerateABS_D_Sim( u32 fd, u32 fs )
{
	EN64FloatReg	n64_fs = EN64FloatReg( fs );
	EN64FloatReg	n64_fd = EN64FloatReg( fd );

	EPs2FloatReg	ps2_fd_sig = EPs2FloatReg( n64_fd );//1:1 Mapping
	EPs2FloatReg	ps2_fd = EPs2FloatReg( n64_fd + 1 );//1:1 Mapping
	EPs2FloatReg	ps2_fs( GetSimFloatRegisterAndLoad( n64_fs ) );

	//Use float now instead of double :)
	ABS_S( ps2_fd, ps2_fs );
	if( !mRegisterCache.IsFPSim( n64_fd ) ) MOV_S( ps2_fd_sig, EPs2FloatReg( fs ));	//Copy signature as well if needed

	UpdateSimDoubleRegister( n64_fd );
}


//

inline void	CCodeGeneratorPS2::GenerateMOV_D_Sim( u32 fd, u32 fs )
{
	EN64FloatReg	n64_fs = EN64FloatReg( fs );
	EN64FloatReg	n64_fd = EN64FloatReg( fd );

	EPs2FloatReg	ps2_fd_sig = EPs2FloatReg( n64_fd );//1:1 Mapping
	EPs2FloatReg	ps2_fd = EPs2FloatReg( n64_fd + 1 );//1:1 Mapping
	EPs2FloatReg	ps2_fs_sig = EPs2FloatReg( n64_fs );//1:1 Mapping

	if( mRegisterCache.IsFPSim( n64_fs ) )
	{
		//Copy Sim double
		EPs2FloatReg	ps2_fs( GetSimFloatRegisterAndLoad( n64_fs ) );

		//Move Sim double :)
		MOV_S( ps2_fd, ps2_fs );
		MOV_S( ps2_fd_sig, ps2_fs_sig);	//Copy signature as well

		UpdateSimDoubleRegister( n64_fd );
	}
	else
	{
		//Copy true double!
		GetFloatRegisterAndLoad( n64_fs );
		EPs2FloatReg	ps2_fs( GetFloatRegisterAndLoad( EN64FloatReg(n64_fs + 1) ) );

		//Move double :)
		MOV_S( ps2_fd_sig, ps2_fs_sig);	//Lo part
		MOV_S( ps2_fd, ps2_fs );	//Hi part

		UpdateFloatRegister( n64_fd );
		UpdateFloatRegister( EN64FloatReg(n64_fd + 1) );
	}
}


//

inline void	CCodeGeneratorPS2::GenerateNEG_D_Sim( u32 fd, u32 fs )
{
	EN64FloatReg	n64_fs = EN64FloatReg( fs );
	EN64FloatReg	n64_fd = EN64FloatReg( fd );

	EPs2FloatReg	ps2_fd_sig = EPs2FloatReg( n64_fd );//1:1 Mapping
	EPs2FloatReg	ps2_fd = EPs2FloatReg( n64_fd + 1 );//1:1 Mapping
	EPs2FloatReg	ps2_fs( GetSimFloatRegisterAndLoad( n64_fs ) );

	//Use float now instead of double :)
	NEG_S( ps2_fd, ps2_fs );
	if( !mRegisterCache.IsFPSim( n64_fd ) ) MOV_S( ps2_fd_sig, EPs2FloatReg( fs ));	//Copy signature as well if needed

	UpdateSimDoubleRegister( n64_fd );
}


//Convert Double to s32(TRUNC)

inline void	CCodeGeneratorPS2::GenerateTRUNC_W_D_Sim( u32 fd, u32 fs )
{
	EN64FloatReg	n64_fs = EN64FloatReg( fs );
	EN64FloatReg	n64_fd = EN64FloatReg( fd );

	EPs2FloatReg	ps2_fd_sig = EPs2FloatReg( n64_fd );//1:1 Mapping
	//EPs2FloatReg	ps2_fd = EPs2FloatReg( n64_fd + 1 );//1:1 Mapping
	EPs2FloatReg	ps2_fs( GetSimFloatRegisterAndLoad( n64_fs ) );

	//Use float now instead of double :)
	//TRUNC_W_S( ps2_fd_sig, ps2_fs );
	printf("*******************************************************trunc2\n");
	JAL(CCodeLabel((const void*)(truncf)), false);
	MFC1(Ps2Reg_A0, ps2_fs);	//Get float to convert
	MTC1(ps2_fd_sig, Ps2Reg_V0);	//Copy to FPU

	UpdateFloatRegister( n64_fd );
}


//Convert Double to s32

inline void	CCodeGeneratorPS2::GenerateCVT_W_D_Sim( u32 fd, u32 fs )
{
	EN64FloatReg	n64_fs = EN64FloatReg( fs );
	EN64FloatReg	n64_fd = EN64FloatReg( fd );

	EPs2FloatReg	ps2_fd_sig = EPs2FloatReg( n64_fd );//1:1 Mapping
	//EPs2FloatReg	ps2_fd = EPs2FloatReg( n64_fd + 1 );//1:1 Mapping
	EPs2FloatReg	ps2_fs( GetSimFloatRegisterAndLoad( n64_fs ) );

	//Use float now instead of double :)
	CVT_W_S( ps2_fd_sig, ps2_fs );

	UpdateFloatRegister( n64_fd );
}


//Convert Double to Float

inline void	CCodeGeneratorPS2::GenerateCVT_S_D_Sim( u32 fd, u32 fs )
{
	EN64FloatReg	n64_fs = EN64FloatReg( fs );
	EN64FloatReg	n64_fd = EN64FloatReg( fd );

	EPs2FloatReg	ps2_fd_sig = EPs2FloatReg( n64_fd );//1:1 Mapping
	//EPs2FloatReg	ps2_fd = EPs2FloatReg( n64_fd + 1 );//1:1 Mapping
	EPs2FloatReg	ps2_fs( GetSimFloatRegisterAndLoad( n64_fs ) );

	//Use float now instead of double :)
	MOV_S( ps2_fd_sig, ps2_fs );

	UpdateFloatRegister( n64_fd );
}


//

inline void	CCodeGeneratorPS2::GenerateCMP_D_Sim( u32 fs, ECop1OpFunction cmp_op, u32 ft )
{
	mFloatCMPIsValid = true;

	EN64FloatReg	n64_fs = EN64FloatReg( fs );
	EN64FloatReg	n64_ft = EN64FloatReg( ft );

	EPs2FloatReg	ps2_fs( GetSimFloatRegisterAndLoad( n64_fs ) );
	EPs2FloatReg	ps2_ft( GetSimFloatRegisterAndLoad( n64_ft ) );

	//Use float now instead of double :)
	CMP_S( ps2_fs, cmp_op, ps2_ft );

#if 0 //Improved version no branch //Corn
	GetVar( Ps2Reg_V0, &gCPUState.FPUControl[31]._u32 );
	CFC1( Ps2Reg_A0, (EPs2FloatReg)31 );
	EXT( Ps2Reg_A0, Ps2Reg_A0, 0, 23 );	//Extract condition bit (true/false)
	INS( Ps2Reg_V0, Ps2Reg_A0, 23, 23 );	//Insert condition bit (true/false)
	SetVar( &gCPUState.FPUControl[31]._u32, Ps2Reg_V0 );

#else //Improved version with only one branch //Corn
	GetVar( Ps2Reg_V0, &gCPUState.FPUControl[31]._u32 );
	LoadConstant( Ps2Reg_A0, FPCSR_C );
	CJumpLocation	test_condition( BC1T( CCodeLabel( nullptr ), false ) );
	OR( Ps2Reg_V0, Ps2Reg_V0, Ps2Reg_A0 );		// flag |= c

	NOR( Ps2Reg_A0, Ps2Reg_A0, Ps2Reg_V0 );		// c = !c
	AND( Ps2Reg_V0, Ps2Reg_V0, Ps2Reg_A0 );		// flag &= !c

	CCodeLabel		condition_true( GetAssemblyBuffer()->GetLabel() );
	SetVar( &gCPUState.FPUControl[31]._u32, Ps2Reg_V0 );
	PatchJumpLong( test_condition, condition_true );
#endif
}


//

inline void	CCodeGeneratorPS2::GenerateADD_S( u32 fd, u32 fs, u32 ft )
{
	EN64FloatReg	n64_fs = EN64FloatReg( fs );
	EN64FloatReg	n64_ft = EN64FloatReg( ft );
	EN64FloatReg	n64_fd = EN64FloatReg( fd );

	EPs2FloatReg	ps2_fd = EPs2FloatReg( n64_fd );//1:1 Mapping
	EPs2FloatReg	ps2_fs( GetFloatRegisterAndLoad( n64_fs ) );
	EPs2FloatReg	ps2_ft( GetFloatRegisterAndLoad( n64_ft ) );

	//SET_ROUND_MODE( gRoundingMode );		//XXXX Is this needed?

	ADD_S( ps2_fd, ps2_fs, ps2_ft );

	UpdateFloatRegister( n64_fd );
}


//

inline void	CCodeGeneratorPS2::GenerateSUB_S( u32 fd, u32 fs, u32 ft )
{
	EN64FloatReg	n64_fs = EN64FloatReg( fs );
	EN64FloatReg	n64_ft = EN64FloatReg( ft );
	EN64FloatReg	n64_fd = EN64FloatReg( fd );

	EPs2FloatReg	ps2_fd = EPs2FloatReg(n64_fd ); //1:1 Mapping
	EPs2FloatReg	ps2_fs( GetFloatRegisterAndLoad( n64_fs ) );
	EPs2FloatReg	ps2_ft( GetFloatRegisterAndLoad( n64_ft ) );

	//SET_ROUND_MODE( gRoundingMode );		//XXXX Is this needed?

	SUB_S( ps2_fd, ps2_fs, ps2_ft );

	UpdateFloatRegister( n64_fd );
}


//

inline void	CCodeGeneratorPS2::GenerateMUL_S( u32 fd, u32 fs, u32 ft )
{
	EN64FloatReg	n64_fs = EN64FloatReg( fs );
	EN64FloatReg	n64_ft = EN64FloatReg( ft );
	EN64FloatReg	n64_fd = EN64FloatReg( fd );

	EPs2FloatReg	ps2_fd = EPs2FloatReg( n64_fd );
	EPs2FloatReg	ps2_fs( GetFloatRegisterAndLoad( n64_fs ) );
	EPs2FloatReg	ps2_ft( GetFloatRegisterAndLoad( n64_ft ) );

	//SET_ROUND_MODE( gRoundingMode );		//XXXX Is this needed?

	MUL_S( ps2_fd, ps2_fs, ps2_ft );

	UpdateFloatRegister( n64_fd );
}


//

inline void	CCodeGeneratorPS2::GenerateDIV_S( u32 fd, u32 fs, u32 ft )
{
	EN64FloatReg	n64_fs = EN64FloatReg( fs );
	EN64FloatReg	n64_ft = EN64FloatReg( ft );
	EN64FloatReg	n64_fd = EN64FloatReg( fd );

	EPs2FloatReg	ps2_fd = EPs2FloatReg( n64_fd );
	EPs2FloatReg	ps2_fs( GetFloatRegisterAndLoad( n64_fs ) );
	EPs2FloatReg	ps2_ft( GetFloatRegisterAndLoad( n64_ft ) );

	//SET_ROUND_MODE( gRoundingMode );		//XXXX Is this needed?

	DIV_S( ps2_fd, ps2_fs, ps2_ft );

	UpdateFloatRegister( n64_fd );
}


//

inline void	CCodeGeneratorPS2::GenerateSQRT_S( u32 fd, u32 fs )
{
	EN64FloatReg	n64_fs = EN64FloatReg( fs );
	EN64FloatReg	n64_fd = EN64FloatReg( fd );

	EPs2FloatReg	ps2_fd = EPs2FloatReg( n64_fd );
	EPs2FloatReg	ps2_fs( GetFloatRegisterAndLoad( n64_fs ) );

	//SET_ROUND_MODE( gRoundingMode );		//XXXX Is this needed?

	SQRT_S( ps2_fd, ps2_fs );

	UpdateFloatRegister( n64_fd );
}


//

inline void	CCodeGeneratorPS2::GenerateABS_S( u32 fd, u32 fs )
{
	EN64FloatReg	n64_fs = EN64FloatReg( fs );
	EN64FloatReg	n64_fd = EN64FloatReg( fd );

	EPs2FloatReg	ps2_fd = EPs2FloatReg( n64_fd );
	EPs2FloatReg	ps2_fs( GetFloatRegisterAndLoad( n64_fs ) );

	//SET_ROUND_MODE( gRoundingMode );		//XXXX Is this needed?

	ABS_S( ps2_fd, ps2_fs );

	UpdateFloatRegister( n64_fd );
}


//

inline void	CCodeGeneratorPS2::GenerateMOV_S( u32 fd, u32 fs )
{
	EN64FloatReg	n64_fs = EN64FloatReg( fs );
	EN64FloatReg	n64_fd = EN64FloatReg( fd );

	EPs2FloatReg	ps2_fd = EPs2FloatReg( n64_fd );
	EPs2FloatReg	ps2_fs( GetFloatRegisterAndLoad( n64_fs ) );

	//SET_ROUND_MODE( gRoundingMode );		//XXXX Is this needed?

	MOV_S( ps2_fd, ps2_fs );

	UpdateFloatRegister( n64_fd );
}


//

inline void	CCodeGeneratorPS2::GenerateNEG_S( u32 fd, u32 fs )
{
	EN64FloatReg	n64_fs = EN64FloatReg( fs );
	EN64FloatReg	n64_fd = EN64FloatReg( fd );

	EPs2FloatReg	ps2_fd = EPs2FloatReg( n64_fd );
	EPs2FloatReg	ps2_fs( GetFloatRegisterAndLoad( n64_fs ) );

	//SET_ROUND_MODE( gRoundingMode );		//XXXX Is this needed?

	NEG_S( ps2_fd, ps2_fs );

	UpdateFloatRegister( n64_fd );
}


//Convert Float to s32

inline void	CCodeGeneratorPS2::GenerateTRUNC_W_S( u32 fd, u32 fs )
{
	EN64FloatReg	n64_fs = EN64FloatReg( fs );
	EN64FloatReg	n64_fd = EN64FloatReg( fd );

	EPs2FloatReg	ps2_fd = EPs2FloatReg( n64_fd );
	EPs2FloatReg	ps2_fs( GetFloatRegisterAndLoad( n64_fs ) );

	//SET_ROUND_MODE( gRoundingMode );		//XXXX Is this needed?
	printf("*******************************************************trunc\n");
	//TRUNC_W_S( ps2_fd, ps2_fs );
	JAL(CCodeLabel((const void*)(truncf)), false);
	MFC1(Ps2Reg_A0, ps2_fs);	//Get float to convert
	MTC1(ps2_fd, Ps2Reg_V0);	//Copy to FPU

	UpdateFloatRegister( n64_fd );
}


//Convert Float to s32

inline void	CCodeGeneratorPS2::GenerateFLOOR_W_S( u32 fd, u32 fs )
{
	EN64FloatReg	n64_fs = EN64FloatReg( fs );
	EN64FloatReg	n64_fd = EN64FloatReg( fd );

	EPs2FloatReg	ps2_fd = EPs2FloatReg( n64_fd );
	EPs2FloatReg	ps2_fs( GetFloatRegisterAndLoad( n64_fs ) );

	//SET_ROUND_MODE( gRoundingMode );		//XXXX Is this needed?

	printf("*******************************************************floor\n");
	//FLOOR_W_S( ps2_fd, ps2_fs );
	JAL(CCodeLabel((const void*)(floorf)), false);
	MFC1(Ps2Reg_A0, ps2_fs);	//Get float to convert
	MTC1(ps2_fd, Ps2Reg_V0);	//Copy to FPU

	UpdateFloatRegister( n64_fd );
}

//Convert Float to s32

inline void	CCodeGeneratorPS2::GenerateCVT_W_S( u32 fd, u32 fs )
{
	EN64FloatReg	n64_fs = EN64FloatReg( fs );
	EN64FloatReg	n64_fd = EN64FloatReg( fd );

	EPs2FloatReg	ps2_fd = EPs2FloatReg( n64_fd );
	EPs2FloatReg	ps2_fs( GetFloatRegisterAndLoad( n64_fs ) );

	//SET_ROUND_MODE( gRoundingMode );		//XXXX Is this needed?

	CVT_W_S( ps2_fd, ps2_fs );

	UpdateFloatRegister( n64_fd );
}


//Convert s32 to (simulated)Double

inline void	CCodeGeneratorPS2::GenerateCVT_D_W_Sim( u32 fd, u32 fs )
{
	EN64FloatReg	n64_fs = EN64FloatReg( fs );
	EN64FloatReg	n64_fd = EN64FloatReg( fd );

	EPs2FloatReg	ps2_fd_sig = EPs2FloatReg( n64_fd );
	EPs2FloatReg	ps2_fd = EPs2FloatReg( n64_fd + 1 );
	EPs2FloatReg	ps2_fs( GetFloatRegisterAndLoad( n64_fs ) );

	CVT_S_W( ps2_fd, ps2_fs );
	LoadConstant( Ps2Reg_A0, SIMULATESIG );	//Get signature
	MTC1( ps2_fd_sig , Ps2Reg_A0 );	//Write signature to float reg

	UpdateSimDoubleRegister( n64_fd );
}


//Convert Float to (simulated)Double

inline void	CCodeGeneratorPS2::GenerateCVT_D_S_Sim( u32 fd, u32 fs )
{
	EN64FloatReg	n64_fs = EN64FloatReg( fs );
	EN64FloatReg	n64_fd = EN64FloatReg( fd );

	EPs2FloatReg	ps2_fd_sig = EPs2FloatReg( n64_fd );
	EPs2FloatReg	ps2_fd = EPs2FloatReg( n64_fd + 1 );
	EPs2FloatReg	ps2_fs( GetFloatRegisterAndLoad( n64_fs ) );

	MOV_S( ps2_fd, ps2_fs );
	LoadConstant( Ps2Reg_A0, SIMULATESIG );	//Get signature
	MTC1( ps2_fd_sig , Ps2Reg_A0 );	//Write signature to float reg

	//SetFloatVar( &gCPUState.FPU[fd + 0]._f32, ps2_fd_sig );
	//SetFloatVar( &gCPUState.FPU[fd + 1]._f32, ps2_fd );

	UpdateSimDoubleRegister( n64_fd );
}


//Convert Float to Double

inline void	CCodeGeneratorPS2::GenerateCVT_D_S( u32 fd, u32 fs )
{
	EN64FloatReg	n64_fs = EN64FloatReg( fs );
	EN64FloatReg	n64_fd = EN64FloatReg( fd );

	EPs2FloatReg	ps2_fd_sig = EPs2FloatReg( n64_fd );//1:1 Mapping
	EPs2FloatReg	ps2_fd = EPs2FloatReg( n64_fd + 1 );//1:1 Mapping
	EPs2FloatReg	ps2_fs( GetFloatRegisterAndLoad( n64_fs ) );

	JAL( CCodeLabel( ( const void * )( _FloatToDouble ) ), false );	//Convert Float to Double
	MFC1( Ps2Reg_A0, ps2_fs );	//Get float to convert

	MTC1( ps2_fd_sig , Ps2Reg_V0 );	//Copy converted Double lo to FPU
	MTC1( ps2_fd , Ps2Reg_V1 ); //Copy converted Double hi to FPU

	mRegisterCache.MarkFPAsValid( n64_fd, true );
	mRegisterCache.MarkFPAsDirty( n64_fd, true );
	mRegisterCache.MarkFPAsSim( n64_fd, false );	//Dont flag as simulated its a real Double!!!
	mRegisterCache.MarkFPAsValid( EN64FloatReg(n64_fd + 1), true );
	mRegisterCache.MarkFPAsDirty( EN64FloatReg(n64_fd + 1), true );
}


//

inline void	CCodeGeneratorPS2::GenerateCMP_S( u32 fs, ECop1OpFunction cmp_op, u32 ft )
{
	mFloatCMPIsValid = true;

	EN64FloatReg	n64_fs = EN64FloatReg( fs );
	EN64FloatReg	n64_ft = EN64FloatReg( ft );

	EPs2FloatReg	ps2_fs( GetFloatRegisterAndLoad( n64_fs ) );
	EPs2FloatReg	ps2_ft( GetFloatRegisterAndLoad( n64_ft ) );

	CMP_S( ps2_fs, cmp_op, ps2_ft );

#if 0 //Improved version no branch //Corn
	GetVar( Ps2Reg_V0, &gCPUState.FPUControl[31]._u32 );
	CFC1( Ps2Reg_A0, (EPs2FloatReg)31 );
	EXT( Ps2Reg_A0, Ps2Reg_A0, 0, 23 );	//Extract condition bit (true/false)
	INS( Ps2Reg_V0, Ps2Reg_A0, 23, 23 );	//Insert condition bit (true/false)
	SetVar( &gCPUState.FPUControl[31]._u32, Ps2Reg_V0 );

#else //Improved version with only one branch //Corn
	GetVar( Ps2Reg_V0, &gCPUState.FPUControl[31]._u32 );
	LoadConstant( Ps2Reg_A0, FPCSR_C );
	CJumpLocation	test_condition( BC1T( CCodeLabel( nullptr ), false ) );
	OR( Ps2Reg_V0, Ps2Reg_V0, Ps2Reg_A0 );		// flag |= c

	NOR( Ps2Reg_A0, Ps2Reg_A0, Ps2Reg_R0 );		// c = !c
	AND( Ps2Reg_V0, Ps2Reg_V0, Ps2Reg_A0 );		// flag &= !c

	CCodeLabel		condition_true( GetAssemblyBuffer()->GetLabel() );
	SetVar( &gCPUState.FPUControl[31]._u32, Ps2Reg_V0 );
	PatchJumpLong( test_condition, condition_true );
#endif
}


//

inline void	CCodeGeneratorPS2::GenerateADD_D( u32 fd, u32 fs, u32 ft )
{
	#ifdef DAEDALUS_DEBUG_CONSOLE
	NOT_IMPLEMENTED( __FUNCTION__ );
	#endif
}


//

inline void	CCodeGeneratorPS2::GenerateSUB_D( u32 fd, u32 fs, u32 ft )
{
	#ifdef DAEDALUS_DEBUG_CONSOLE
	NOT_IMPLEMENTED( __FUNCTION__ );
	#endif
}


//

inline void	CCodeGeneratorPS2::GenerateMUL_D( u32 fd, u32 fs, u32 ft )
{
	#ifdef DAEDALUS_DEBUG_CONSOLE
	NOT_IMPLEMENTED( __FUNCTION__ );
	#endif
}


//

inline void	CCodeGeneratorPS2::GenerateDIV_D( u32 fd, u32 fs, u32 ft )
{
	#ifdef DAEDALUS_DEBUG_CONSOLE
	NOT_IMPLEMENTED( __FUNCTION__ );
	#endif
}


//

inline void	CCodeGeneratorPS2::GenerateSQRT_D( u32 fd, u32 fs )
{
	#ifdef DAEDALUS_DEBUG_CONSOLE
	NOT_IMPLEMENTED( __FUNCTION__ );
	#endif
}


//

inline void	CCodeGeneratorPS2::GenerateMOV_D( u32 fd, u32 fs )
{
	#ifdef DAEDALUS_DEBUG_CONSOLE
	NOT_IMPLEMENTED( __FUNCTION__ );
	#endif
}


//

inline void	CCodeGeneratorPS2::GenerateNEG_D( u32 fd, u32 fs )
{
	#ifdef DAEDALUS_DEBUG_CONSOLE
	NOT_IMPLEMENTED( __FUNCTION__ );
	#endif
}



//Convert s32 to Float

inline void	CCodeGeneratorPS2::GenerateCVT_S_W( u32 fd, u32 fs )
{
	EN64FloatReg	n64_fs = EN64FloatReg( fs );
	EN64FloatReg	n64_fd = EN64FloatReg( fd );

	EPs2FloatReg	ps2_fd = EPs2FloatReg( n64_fd );
	EPs2FloatReg	ps2_fs( GetFloatRegisterAndLoad( n64_fs ) );

	//SET_ROUND_MODE( gRoundingMode );		//XXXX Is this needed?

	CVT_S_W( ps2_fd, ps2_fs );

	UpdateFloatRegister( n64_fd );
}


//

inline void	CCodeGeneratorPS2::GenerateMFC0( EN64Reg rt, u32 fs )
{
	#ifdef DAEDALUS_ENABLE_ASSERTS
	// Never seen this to happen, no reason to bother to handle it
	DAEDALUS_ASSERT( fs != C0_RAND, "Reading MFC0 random register is unhandled");
	#endif
	EPs2Reg reg_dst( GetRegisterNoLoadLo( rt, Ps2Reg_V0 ) );

	GetVar( reg_dst, &gCPUState.CPUControl[ fs ]._u32 );
	UpdateRegister( rt, reg_dst, URO_HI_SIGN_EXTEND );
}


//

inline void	CCodeGeneratorPS2::GenerateMTC0( EN64Reg rt, u32 fs )
{
	EPs2Reg reg_src( GetRegisterAndLoadLo( rt, Ps2Reg_V0 ) );

	SetVar( &gCPUState.CPUControl[ fs ]._u32, reg_src );
}
