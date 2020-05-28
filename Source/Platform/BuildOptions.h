#ifndef BUILDOPTIONS_H_
#define BUILDOPTIONS_H_

//
//	Platform options
//
#undef  DAEDALUS_COMPRESSED_ROM_SUPPORT			// Define this to enable support for compressed Roms(zip'ed). If you define this, you will need to add unzip.c and ioapi.c to the project too. (Located at Source/Utility/Zip/)
#undef  DAEDALUS_BREAKPOINTS_ENABLED			// Define this to enable breakpoint support
// DAEDALUS_ENDIAN_MODE should be defined as one of:
//
#define DAEDALUS_ENDIAN_LITTLE 1
#define DAEDALUS_ENDIAN_BIG 2


// The endianness should really be defined
#ifndef DAEDALUS_ENDIAN_MODE
#error DAEDALUS_ENDIAN_MODE was not specified in Platform.h
#endif

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


#endif // BUILDOPTIONS_H_
