/*
Copyright (C) 2001 StrmnNrmn

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

#ifndef CORE_MEMORY_H_
#define CORE_MEMORY_H_

#include "Ultra/ultra_rcp.h"
#include "System/AtomicPrimitives.h"
#include "System/Endian.h"

bool Memory_Init();
void Memory_Fini();
bool Memory_Reset();
void Memory_Cleanup();

enum MEMBANKTYPE
{
	MEM_UNUSED = 0,			// Simplifies code so that we don't have to check for illegal memory accesses

	MEM_RD_RAM,				// 8 or 4 Mb (4/8*1024*1024)

	MEM_SP_MEM,				// 0x2000

	MEM_PIF_RAM,			// 0x40

	MEM_RD_REG0,			// 0x30		// This has changed - used to be 1Mb
	MEM_SP_REG,				// 0x20
	MEM_SP_PC_REG,			// 0x10		// SP_PC_REG + SP_IBITS_REG
	MEM_DPC_REG,			// 0x20
	MEM_MI_REG,				// 0x10
	MEM_VI_REG,				// 0x38
	MEM_AI_REG,				// 0x18
	MEM_PI_REG,				// 0x34
	MEM_RI_REG,				// 0x20
	MEM_SI_REG,				// 0x1C

	MEM_SAVE,				// 0x20000 128KBytes EEPROM (512 bytes), 4X EEPROM (2Kbytes), SRAM (32Kbytes), FlashRAM (128Kbytes)
	MEM_MEMPACK,			// 0x20000 MEMPack 32Kbytes * 4 = 128KBytes

	NUM_MEM_BUFFERS
};


static const u32 MEMORY_4_MEG( 4*1024*1024 );
static const u32 MEMORY_8_MEG( 8*1024*1024 );
#define MAX_RAM_ADDRESS MEMORY_8_MEG

typedef const void * (*ReadFunction )( u32 address );
typedef void (*WriteFunction )( u32 address, u32 value );

struct MemWriteEntry
{
	u8*           base;
	WriteFunction write_func;
};

struct MemReadEntry
{
	const u8*    base;
	ReadFunction read_func;
};

extern u32		gRamSize;
#ifdef DAEDALUS_PROFILE_EXECUTION
extern u32		gTLBReadHit;
extern u32		gTLBWriteHit;
#endif
extern void *	gMemBuffers[NUM_MEM_BUFFERS];
extern const u32 gMemBufferSizes[NUM_MEM_BUFFERS];

// Useful defines for making code look nicer:
#define gu8RamBase ((u8*)gMemBuffers[MEM_RD_RAM])
#define gs8RamBase ((s8*)gMemBuffers[MEM_RD_RAM])
#define gu32RamBase ((u32*)gMemBuffers[MEM_RD_RAM])

#define gu8SpMemBase ((u8*)gMemBuffers[MEM_SP_MEM])
#define gu8SpImemBase ((u8*)gMemBuffers[MEM_SP_MEM] + SP_DMA_IMEM)

extern u8* gu8RamBase_8000;


typedef const void * (*ReadFunction )( u32 address );
typedef void (*WriteFunction )( u32 address, u32 value );

#ifndef DAEDALUS_SILENT
typedef bool (*InternalMemFastFunction)( u32 address, void ** p_translated );
#endif

extern MemReadEntry  gMemReadLUT[0x4000];
extern MemWriteEntry gMemWriteLUT[0x4000];

// Fast memory access
inline const void* ReadAddress( u32 address )
{
		const MemReadEntry& entry = gMemReadLUT[address >> 18];

	// Access through pointer with no function calls at all (Fast)
		if (entry.base)
		return (void*)(entry.base + address);

	// Need to go through the HW access handlers or TLB (Slow)
	return entry.read_func(address);
}

inline void WriteAddress( u32 address, u32 value )
{
	const MemWriteEntry& entry = gMemWriteLUT[address >> 18];

	// Access through pointer with no function calls at all (Fast)
	if (entry.base)
	{
		*(u32*)(entry.base + address) = value;
		return;
	}
	// Need to go through the HW access handlers or TLB (Slow)
	entry.write_func(address, value);
}

#ifndef DAEDALUS_SILENT
bool Memory_GetInternalReadAddress(u32 address, const void ** p_translated);
#endif


// Quick Read/Write methods that require a base returned by
// ReadAddress or Memory_GetInternalReadAddress etc

inline u64 QuickRead64Bits(const void* ptr, u32 offset)
{
	u64 data = *(u64*)(static_cast<const u8*>(ptr) + offset);
	return (data >> 32) + (data << 32);
}

inline u32 QuickRead32Bits(const void* ptr, u32 offset)
{
	return *(u32*)(static_cast<const u8*>(ptr) + offset);
}

inline u16 QuickRead16Bits(const void* ptr, u32 offset)
{
	return *(u16*)((uintptr_t)(static_cast<const u8*>(ptr) + offset) ^ U16_TWIDDLE);
}

inline void QuickWrite16Bits(void* ptr, u32 offset, u16 value)
{
	*(u16*)((uintptr_t)(static_cast<u8*>(ptr) + offset) ^ U16_TWIDDLE) = value;
}

inline void QuickWrite64Bits(void* ptr, u32 offset, u64 value)
{
	*(u64*)(static_cast<u8*>(ptr) + offset) = (value >> 32) + (value << 32);
}

inline void QuickWrite32Bits(void* ptr, u32 offset, u32 value)
{
	*(u32*)(static_cast<u8*>(ptr) + offset) = value;
}

inline void QuickWrite32Bits(const void* ptr, u32 value)
{
	*(u32*)ptr = value;
}

#define MEMORY_SIZE_RDRAM				0x400000
#define MEMORY_SIZE_EXRDRAM				0x400000
#define MEMORY_SIZE_RDRAM_DEFAULT		MEMORY_SIZE_RDRAM + MEMORY_SIZE_EXRDRAM
#define MEMORY_SIZE_RAMREGS0			0x30
#define MEMORY_SIZE_RAMREGS4			0x30
#define MEMORY_SIZE_RAMREGS8			0x30
#define MEMORY_SIZE_SPMEM				0x2000
#define MEMORY_SIZE_SPREG_1				0x24
#define MEMORY_SIZE_SPREG_2				0x8
#define MEMORY_SIZE_DPC					0x20
#define MEMORY_SIZE_DPS					0x10
#define MEMORY_SIZE_MI					0x10
#define MEMORY_SIZE_VI					0x50
#define MEMORY_SIZE_AI					0x18
#define MEMORY_SIZE_PI					0x4C
#define MEMORY_SIZE_RI					0x20
#define MEMORY_SIZE_SI					0x1C
#define MEMORY_SIZE_C2A1				0x8000
#define MEMORY_SIZE_C1A1				0x8000
#define MEMORY_SIZE_C2A2				0x20000
#define MEMORY_SIZE_GIO_REG				0x804
#define MEMORY_SIZE_C1A3				0x8000
#define MEMORY_SIZE_PIF					0x800
#define MEMORY_SIZE_DUMMY				0x10000

#define MEMORY_START_RDRAM		0x00000000
#define MEMORY_START_EXRDRAM	0x00400000
#define MEMORY_START_RAMREGS0	0x03F00000
#define MEMORY_START_RAMREGS4	0x03F04000
#define MEMORY_START_RAMREGS8	0x03F80000
#define MEMORY_START_SPMEM		0x04000000
#define MEMORY_START_SPREG_1	0x04040000
#define MEMORY_START_SPREG_2	0x04080000
#define MEMORY_START_DPC		0x04100000
#define MEMORY_START_DPS		0x04200000
#define MEMORY_START_MI			0x04300000
#define MEMORY_START_VI			0x04400000
#define MEMORY_START_AI			0x04500000
#define MEMORY_START_PI			0x04600000
#define MEMORY_START_RI			0x04700000
#define MEMORY_START_SI			0x04800000
#define MEMORY_START_C2A1		0x05000000
#define MEMORY_START_C1A1		0x06000000
#define MEMORY_START_C2A2		0x08000000
#define MEMORY_START_ROM_IMAGE	0x10000000
#define MEMORY_START_GIO		0x18000000
#define MEMORY_START_PIF		0x1FC00000
#define MEMORY_START_C1A3		0x1FD00000
#define MEMORY_START_DUMMY		0x1FFF0000

#define FLASHRAM_READ_ADDR		0x08000000
#define FLASHRAM_WRITE_ADDR		0x08010000

extern u8 * gu8RamBase_8000;
//extern u8 * gu8RamBase_A000;


//#define MEMORY_CHECK_ALIGN( address, align )	DAEDALUS_ASSERT( (address & ~(align-1)) == 0, "Unaligned memory access" )
#define MEMORY_CHECK_ALIGN( address, align )

#if (DAEDALUS_ENDIAN_MODE == DAEDALUS_ENDIAN_BIG)

inline u64 Read64Bits( u32 address )				{ MEMORY_CHECK_ALIGN( address, 8 ); return *(u64 *)ReadAddress( address ); }
inline u32 Read32Bits( u32 address )				{ MEMORY_CHECK_ALIGN( address, 4 ); return *(u32 *)ReadAddress( address ); }
inline u16 Read16Bits( u32 address )				{ MEMORY_CHECK_ALIGN( address, 2 ); return *(u16 *)ReadAddress( address ); }
inline u8 Read8Bits( u32 address )					{                                   return *(u8  *)ReadAddress( address ); }

inline void Write64Bits( u32 address, u64 data )	{ MEMORY_CHECK_ALIGN( address, 8 ); *(u64 *)ReadAddress( address ) = data; }
inline void Write32Bits( u32 address, u32 data )	{ MEMORY_CHECK_ALIGN( address, 4 ); WriteAddress(address, data); }
inline void Write16Bits( u32 address, u16 data )	{ MEMORY_CHECK_ALIGN( address, 2 ); *(u16 *)ReadAddress(address) = data; }
inline void Write8Bits( u32 address, u8 data )		{                                   *(u8 *)ReadAddress(address) = data;}

#elif (DAEDALUS_ENDIAN_MODE == DAEDALUS_ENDIAN_LITTLE)

inline u64 Read64Bits( u32 address )				{ MEMORY_CHECK_ALIGN( address, 8 ); u64 data = *(u64 *)ReadAddress( address ); data = (data>>32) + (data<<32); return data; }
inline u32 Read32Bits( u32 address )				{ MEMORY_CHECK_ALIGN( address, 4 ); return *(u32 *)ReadAddress( address ); }
inline u16 Read16Bits( u32 address )				{ MEMORY_CHECK_ALIGN( address, 2 ); return *(u16 *)ReadAddress( address ^ U16_TWIDDLE ); }
inline u8 Read8Bits( u32 address )					{                                   return *(u8  *)ReadAddress( address ^ U8_TWIDDLE ); }

inline void Write64Bits( u32 address, u64 data )	{ MEMORY_CHECK_ALIGN( address, 8 ); *(u64 *)ReadAddress( address ) = (data>>32) + (data<<32); }
inline void Write32Bits( u32 address, u32 data )	{ MEMORY_CHECK_ALIGN( address, 4 ); WriteAddress(address, data); }
inline void Write16Bits( u32 address, u16 data )	{ MEMORY_CHECK_ALIGN( address, 2 ); *(u16 *)ReadAddress(address ^ U16_TWIDDLE) = data; }
inline void Write8Bits( u32 address, u8 data )		{                                   *(u8 *)ReadAddress(address ^ U8_TWIDDLE) = data;}

#else
#error No DAEDALUS_ENDIAN_MODE specified
#endif //DAEDALUS_ENDIAN_MODE

//inline void Write64Bits_NoSwizzle( u32 address, u64 data ){ MEMORY_CHECK_ALIGN( address, 8 ); *(u64 *)WriteAddress( address ) = (data>>32) + (data<<32); }
inline void Write32Bits_NoSwizzle( u32 address, u32 data )	{ MEMORY_CHECK_ALIGN( address, 4 ); WriteAddress(address, data); }
inline void Write16Bits_NoSwizzle( u32 address, u16 data )	{ MEMORY_CHECK_ALIGN( address, 2 ); *(u16 *)ReadAddress(address) = data; }
inline void Write8Bits_NoSwizzle( u32 address, u8 data )	{                                   *(u8 *)ReadAddress(address) = data;}

/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
//                Register Macros                  //
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////

#define REGISTER_FUNCTIONS( set, base_reg, memory_buffer )								\
	inline void Memory_##set##_SetRegister( u32 reg, u32 value )						\
	{																					\
		((u32 *)gMemBuffers[memory_buffer])[ (reg - base_reg) / 4 ] = value;		\
	}																					\
																						\
	inline u32 Memory_##set##_GetRegister( u32 reg )									\
	{																					\
		return ((u32 *)gMemBuffers[memory_buffer])[ (reg - base_reg) / 4 ];		\
	}																					\
																						\
	inline u32 Memory_##set##_SetRegisterBits( u32 reg, u32 and_bits, u32 or_bits )		\
	{																					\
		u32 * p( &((u32 *)gMemBuffers[memory_buffer])[ (reg - base_reg) / 4 ] );	\
		return AtomicBitSet( p, and_bits, or_bits );									\
	}																					\
																						\
	inline u32 Memory_##set##_SetRegisterBits( u32 reg, u32 value )						\
	{																					\
		u32 * p( &((u32 *)gMemBuffers[memory_buffer])[ (reg - base_reg) / 4 ] );	\
		return AtomicBitSet( p, 0xffffffff, value );									\
	}																					\
																						\
	inline u32 Memory_##set##_ClrRegisterBits( u32 reg, u32 value )						\
	{																					\
		u32 * p( &((u32 *)gMemBuffers[memory_buffer])[ (reg - base_reg) / 4 ] );	\
		return AtomicBitSet( p, ~value, 0 );											\
	}																					\
																						\
	inline u32 * set##_REG_ADDRESS( u32 reg )											\
	{																					\
		return &((u32 *)gMemBuffers[memory_buffer])[(reg - base_reg) / 4];			\
	}

REGISTER_FUNCTIONS( MI, MI_BASE_REG, MEM_MI_REG )
REGISTER_FUNCTIONS( SP, SP_BASE_REG, MEM_SP_REG )
REGISTER_FUNCTIONS( PC, SP_PC_REG, MEM_SP_PC_REG )
REGISTER_FUNCTIONS( AI, AI_BASE_REG, MEM_AI_REG )
REGISTER_FUNCTIONS( VI, VI_BASE_REG, MEM_VI_REG )
REGISTER_FUNCTIONS( SI, SI_BASE_REG, MEM_SI_REG )
REGISTER_FUNCTIONS( PI, PI_BASE_REG, MEM_PI_REG )
REGISTER_FUNCTIONS( DPC, DPC_BASE_REG, MEM_DPC_REG )

#undef REGISTER_FUNCTIONS

class VIOriginChangedEventHandler
{
  public:
	virtual ~VIOriginChangedEventHandler();
	virtual void OnOriginChanged(u32 value) = 0;
};

void Memory_RegisterVIOriginChangedEventHandler(VIOriginChangedEventHandler* handler);
void Memory_UnregisterVIOriginChangedEventHandler(VIOriginChangedEventHandler* handler);


#endif // CORE_MEMORY_H_
