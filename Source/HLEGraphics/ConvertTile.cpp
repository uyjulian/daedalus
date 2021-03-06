#include "stdafx.h"

#ifdef DAEDALUS_ACCURATE_TMEM
#include "Core/ROM.h"
#include "HLEGraphics/ConvertTile.h"
#include "HLEGraphics/RDP.h"
#include "HLEGraphics/TextureInfo.h"
#include "Graphics/NativePixelFormat.h"
#include "Utility/Endian.h"
#include "Utility/Alignment.h"

#include <vector>

struct TileDestInfo
{
	explicit TileDestInfo( ETextureFormat tex_fmt )
		:	Format( tex_fmt )
		,	Width( 0 )
		,	Height( 0 )
		,	Pitch( 0 )
		,	Data( nullptr )
		//,	Palette( nullptr )
	{
	}

	ETextureFormat		Format;
	u32					Width;			// Describes the width of the locked area. Use lPitch to move between successive lines
	u32					Height;			// Describes the height of the locked area
	s32					Pitch;			// Specifies the number of bytes on each row (not necessarily bitdepth*width/8)
	void *				Data;			// Pointer to the top left pixel of the image
	//NativePf8888 *		Palette;
};

static const u8 OneToEight[] = {
	0x00,   // 0 -> 00 00 00 00
	0xff    // 1 -> 11 11 11 11
};

static const u8 ThreeToEight[] = {
	0x00,   // 000 -> 00 00 00 00
	0x24,   // 001 -> 00 10 01 00
	0x49,   // 010 -> 01 00 10 01
	0x6d,   // 011 -> 01 10 11 01
	0x92,   // 100 -> 10 01 00 10
	0xb6,   // 101 -> 10 11 01 10
	0xdb,   // 110 -> 11 01 10 11
	0xff    // 111 -> 11 11 11 11
};

static const u8 FourToEight[] = {
	0x00, 0x11, 0x22, 0x33,
	0x44, 0x55, 0x66, 0x77,
	0x88, 0x99, 0xaa, 0xbb,
	0xcc, 0xdd, 0xee, 0xff
};

static const u8 FiveToEight[] = {
	0x00, // 00000 -> 00000000
	0x08, // 00001 -> 00001000
	0x10, // 00010 -> 00010000
	0x18, // 00011 -> 00011000
	0x21, // 00100 -> 00100001
	0x29, // 00101 -> 00101001
	0x31, // 00110 -> 00110001
	0x39, // 00111 -> 00111001
	0x42, // 01000 -> 01000010
	0x4a, // 01001 -> 01001010
	0x52, // 01010 -> 01010010
	0x5a, // 01011 -> 01011010
	0x63, // 01100 -> 01100011
	0x6b, // 01101 -> 01101011
	0x73, // 01110 -> 01110011
	0x7b, // 01111 -> 01111011

	0x84, // 10000 -> 10000100
	0x8c, // 10001 -> 10001100
	0x94, // 10010 -> 10010100
	0x9c, // 10011 -> 10011100
	0xa5, // 10100 -> 10100101
	0xad, // 10101 -> 10101101
	0xb5, // 10110 -> 10110101
	0xbd, // 10111 -> 10111101
	0xc6, // 11000 -> 11000110
	0xce, // 11001 -> 11001110
	0xd6, // 11010 -> 11010110
	0xde, // 11011 -> 11011110
	0xe7, // 11100 -> 11100111
	0xef, // 11101 -> 11101111
	0xf7, // 11110 -> 11110111
	0xff  // 11111 -> 11111111
 };

ALIGNED_EXTERN(u8, gTMEM[4096], 16);

// convert rgba values (0-255 per channel) to a dword in A8R8G8B8 order..
#define CONVERT_RGBA(r,g,b,a)  (a<<24) | (b<<16) | (g<<8) | r


static u32 RGBA16(u16 v)
{
	u32 r = FiveToEight[(v>>11)&0x1f];
	u32 g = FiveToEight[(v>> 6)&0x1f];
	u32 b = FiveToEight[(v>> 1)&0x1f];
	u32 a = ((v     )&0x01)? 255 : 0;
	return CONVERT_RGBA(r, g, b, a);
}

static u32 IA16(u16 v)
{
	u32 i = (v>>8)&0xff;
	u32 a = (v   )&0xff;
	return CONVERT_RGBA(i, i, i, a);
}

static u32 I4(u8 v)
{
	u32 i = FourToEight[v & 0x0F];
	return CONVERT_RGBA(i, i, i, i);
}

static u32 IA4(u8 v)
{
	u32 i = ThreeToEight[(v & 0x0f) >> 1];
	u32 a = OneToEight[(v & 0x01)];
	return CONVERT_RGBA(i, i, i, a);
}

static u32 YUV16(s32 Y, s32 U, s32 V)
{
    s32 r = s32(Y + (1.370705f * (V-128)));
    s32 g = s32(Y - (0.698001f * (V-128)) - (0.337633f * (U-128)));
    s32 b = s32(Y + (1.732446f * (U-128)));

    r = r < 0 ? 0 : (r > 255 ? 255 : r);
    g = g < 0 ? 0 : (g > 255 ? 255 : g);
    b = b < 0 ? 0 : (b > 255 ? 255 : b);
    return CONVERT_RGBA(r, g, b, 255);
}


static void ConvertRGBA32(const TileDestInfo & dsti, const TextureInfo & ti)
{
	u32 width = dsti.Width;
	u32 height = dsti.Height;

	u8 * dst = static_cast<u8*>(dsti.Data);
	u32 dst_row_stride = dsti.Pitch;
	u32 dst_row_offset = 0;

	const u8 * src = gTMEM;
	u32 src_row_stride = ti.GetLine()<<3;
	u32 src_row_offset = ti.GetTmemAddress()<<3;

	// NB! RGBA/32 line needs to be doubled.
	src_row_stride *= 2;

	u32 row_swizzle = 0;
	for (u32 y = 0; y < height; ++y)
	{
		u32 src_offset = src_row_offset;
		u32 dst_offset = dst_row_offset;
		for (u32 x = 0; x < width; ++x)
		{
			u32 o = src_offset^row_swizzle;

			dst[dst_offset+0] = src[(o+0)&0xfff];
			dst[dst_offset+1] = src[(o+1)&0xfff];
			dst[dst_offset+2] = src[(o+2)&0xfff];
			dst[dst_offset+3] = src[(o+3)&0xfff];

			src_offset += 4;
			dst_offset += 4;
		}
		src_row_offset += src_row_stride;
		dst_row_offset += dst_row_stride;

		row_swizzle ^= 0x8;   // Alternate lines are qword-swapped
	}
}

static void ConvertRGBA16(const TileDestInfo & dsti, const TextureInfo & ti)
{
	u32 width = dsti.Width;
	u32 height = dsti.Height;

	u32 * dst = static_cast<u32*>(dsti.Data);
	u32 dst_row_stride = dsti.Pitch / sizeof(u32);
	u32 dst_row_offset = 0;

	const u8 * src = gTMEM;
	u32 src_row_stride = ti.GetLine()<<3;
	u32 src_row_offset = ti.GetTmemAddress()<<3;

	u32 row_swizzle = 0;
	for (u32 y = 0; y < height; ++y)
	{
		u32 src_offset = src_row_offset;
		u32 dst_offset = dst_row_offset;
		for (u32 x = 0; x < width; ++x)
		{
			u32 o = src_offset^row_swizzle;
			u32 src_pixel_hi = src[(o+0)&0xfff];
			u32 src_pixel_lo = src[(o+1)&0xfff];
			u16 src_pixel = (src_pixel_hi << 8) | src_pixel_lo;

			dst[dst_offset+0] = RGBA16(src_pixel);

			src_offset += 2;
			dst_offset += 1;
		}
		src_row_offset += src_row_stride;
		dst_row_offset += dst_row_stride;

		row_swizzle ^= 0x4;   // Alternate lines are word-swapped
	}
}

template <u32 (*PalConvertFn)(u16)>
static void ConvertCI8T(const TileDestInfo & dsti, const TextureInfo & ti)
{
	u32 width = dsti.Width;
	u32 height = dsti.Height;

	u32 * dst = static_cast<u32*>(dsti.Data);
	u32 dst_row_stride = dsti.Pitch / sizeof(u32);
	u32 dst_row_offset = 0;

	const u8 * src     = gTMEM;
	const u16 * src16 = (u16*)src;

	u32 src_row_stride = ti.GetLine()<<3;
	u32 src_row_offset = ti.GetTmemAddress()<<3;

	// Convert the palette once, here.
	u32 palette[256];
	for (u32 i = 0; i < 256; ++i)
	{
		u16 src_pixel = src16[0x400+(i<<2)];
		palette[i] = PalConvertFn(src_pixel);
	}

	u32 row_swizzle = 0;
	for (u32 y = 0; y < height; ++y)
	{
		u32 src_offset = src_row_offset;
		u32 dst_offset = dst_row_offset;
		for (u32 x = 0; x < width; ++x)
		{
			u32 o = src_offset^row_swizzle;
			u8 src_pixel = src[o&0xfff];

			dst[dst_offset+0] = palette[src_pixel];

			src_offset += 1;
			dst_offset += 1;
		}
		src_row_offset += src_row_stride;
		dst_row_offset += dst_row_stride;

		row_swizzle ^= 0x4;   // Alternate lines are word-swapped
	}
}

template <u32 (*PalConvertFn)(u16)>
static void ConvertCI4T(const TileDestInfo & dsti, const TextureInfo & ti)
{
	u32 width = dsti.Width;
	u32 height = dsti.Height;

	u32 * dst = static_cast<u32*>(dsti.Data);
	u32 dst_row_stride = dsti.Pitch / sizeof(u32);
	u32 dst_row_offset = 0;

	const u8 * src =  gTMEM;
	const u16 * src16 = (u16*)src;

	u32 src_row_stride = ti.GetLine()<<3;
	u32 src_row_offset = ti.GetTmemAddress()<<3;

	// Convert the palette once, here.
	u32 pal_address = 0x400 + (ti.GetPalette()<<6);
	u32 palette[16];
	for (u32 i = 0; i < 16; ++i)
	{
		u16 src_pixel = src16[pal_address+(i<<2)];
		palette[i] = PalConvertFn(src_pixel);
	}

	u32 row_swizzle = 0;
	for (u32 y = 0; y < height; ++y)
	{
		u32 src_offset = src_row_offset;
		u32 dst_offset = dst_row_offset;

		// Process 2 pixels at a time
		for (u32 x = 0; x+1 < width; x += 2)
		{
			u32 o = src_offset^row_swizzle;
			u8 src_pixel = src[o&0xfff];

			dst[dst_offset+0] = palette[(src_pixel&0xf0)>>4];
			dst[dst_offset+1] = palette[(src_pixel&0x0f)>>0];

			src_offset += 1;
			dst_offset += 2;
		}

		// Handle trailing pixel, if odd width
		if (width&1)
		{
			u32 o = src_offset^row_swizzle;
			u8 src_pixel = src[o&0xfff];

			dst[dst_offset+0] = palette[(src_pixel&0xf0)>>4];

			src_offset += 1;
			dst_offset += 1;
		}

		src_row_offset += src_row_stride;
		dst_row_offset += dst_row_stride;

		row_swizzle ^= 0x4;   // Alternate lines are word-swapped
	}
}

static void ConvertCI8(const TileDestInfo & dsti, const TextureInfo & ti)
{
	switch (ti.GetTLutFormat())
	{
	case kTT_RGBA16:
		ConvertCI8T< RGBA16 >(dsti, ti);
		break;
	case kTT_IA16:
		ConvertCI8T< IA16 >(dsti, ti);
		break;
	default:
		DAEDALUS_ERROR("Unhandled tlut format %d/%d", ti.GetTLutFormat());
		break;
	}
}

static void ConvertCI4(const TileDestInfo & dsti, const TextureInfo & ti)
{
	switch (ti.GetTLutFormat())
	{
	case kTT_RGBA16:
		ConvertCI4T< RGBA16 >(dsti, ti);
		break;
	case kTT_IA16:
		ConvertCI4T< IA16 >(dsti, ti);
		break;
	default:
		DAEDALUS_ERROR("Unhandled tlut format %d/%d", ti.GetTLutFormat());
		break;
	}
}

static void ConvertIA16(const TileDestInfo & dsti, const TextureInfo & ti)
{
	u32 width = dsti.Width;
	u32 height = dsti.Height;

	u32 * dst = static_cast<u32*>(dsti.Data);
	u32 dst_row_stride = dsti.Pitch / sizeof(u32);
	u32 dst_row_offset = 0;

	const u8 * src     = gTMEM;
	u32 src_row_stride = ti.GetLine()<<3;
	u32 src_row_offset = ti.GetTmemAddress()<<3;

	u32 row_swizzle = 0;
	for (u32 y = 0; y < height; ++y)
	{
		u32 src_offset = src_row_offset;
		u32 dst_offset = dst_row_offset;
		for (u32 x = 0; x < width; ++x)
		{
			u32 o = src_offset^row_swizzle;
			u32 src_pixel_hi = src[(o+0)&0xfff];
			u32 src_pixel_lo = src[(o+1)&0xfff];
			u16 src_pixel = (src_pixel_hi << 8) | src_pixel_lo;

			dst[dst_offset+0] = IA16(src_pixel);

			src_offset += 2;
			dst_offset += 1;
		}
		src_row_offset += src_row_stride;
		dst_row_offset += dst_row_stride;

		row_swizzle ^= 0x4;   // Alternate lines are word-swapped
	}
}

static void ConvertIA8(const TileDestInfo & dsti, const TextureInfo & ti)
{
	u32 width = dsti.Width;
	u32 height = dsti.Height;

	u8 * dst = static_cast<u8*>(dsti.Data);
	u32 dst_row_stride = dsti.Pitch;
	u32 dst_row_offset = 0;

	const u8 * src     = gTMEM;
	u32 src_row_stride = ti.GetLine()<<3;
	u32 src_row_offset = ti.GetTmemAddress()<<3;

	u32 row_swizzle = 0;
	for (u32 y = 0; y < height; ++y)
	{
		u32 src_offset = src_row_offset;
		u32 dst_offset = dst_row_offset;
		for (u32 x = 0; x < width; ++x)
		{
			u32 o = src_offset^row_swizzle;
			u8 src_pixel = src[o&0xfff];

			u8 i = FourToEight[(src_pixel>>4)&0xf];
			u8 a = FourToEight[(src_pixel   )&0xf];

			dst[dst_offset+0] = i;
			dst[dst_offset+1] = i;
			dst[dst_offset+2] = i;
			dst[dst_offset+3] = a;

			src_offset += 1;
			dst_offset += 4;
		}
		src_row_offset += src_row_stride;
		dst_row_offset += dst_row_stride;

		row_swizzle ^= 0x4;   // Alternate lines are word-swapped
	}
}

static void ConvertIA4(const TileDestInfo & dsti, const TextureInfo & ti)
{
	u32 width = dsti.Width;
	u32 height = dsti.Height;

	u32 * dst = static_cast<u32*>(dsti.Data);
	u32 dst_row_stride = dsti.Pitch / sizeof(u32);
	u32 dst_row_offset = 0;

	const u8 * src     = gTMEM;
	u32 src_row_stride = ti.GetLine()<<3;
	u32 src_row_offset = ti.GetTmemAddress()<<3;

	u32 row_swizzle = 0;
	for (u32 y = 0; y < height; ++y)
	{
		u32 src_offset = src_row_offset;
		u32 dst_offset = dst_row_offset;

		// Process 2 pixels at a time
		for (u32 x = 0; x+1 < width; x += 2)
		{
			u32 o = src_offset^row_swizzle;
			u8 src_pixel = src[o&0xfff];

			dst[dst_offset+0] = IA4(src_pixel>>4);
			dst[dst_offset+1] = IA4((src_pixel&0xf));

			src_offset += 1;
			dst_offset += 2;
		}

		// Handle trailing pixel, if odd width
		if (width&1)
		{
			u32 o = src_offset^row_swizzle;
			u8 src_pixel = src[o&0xfff];

			dst[dst_offset+0] = IA4(src_pixel>>4);

			src_offset += 1;
			dst_offset += 1;
		}

	  src_row_offset += src_row_stride;
	  dst_row_offset += dst_row_stride;

	  row_swizzle ^= 0x4;   // Alternate lines are word-swapped
	}
}

static void ConvertI8(const TileDestInfo & dsti, const TextureInfo & ti)
{
	u32 width = dsti.Width;
	u32 height = dsti.Height;

	u8 * dst = static_cast<u8*>(dsti.Data);
	u32 dst_row_stride = dsti.Pitch;
	u32 dst_row_offset = 0;

	const u8 * src     = gTMEM;
	u32 src_row_stride = ti.GetLine()<<3;
	u32 src_row_offset = ti.GetTmemAddress()<<3;

	u32 row_swizzle = 0;
	for (u32 y = 0; y < height; ++y)
	{
		u32 src_offset = src_row_offset;
		u32 dst_offset = dst_row_offset;
		for (u32 x = 0; x < width; ++x)
		{
			u32 o = src_offset^row_swizzle;
			u8 i = src[o&0xfff];

			dst[dst_offset+0] = i;
			dst[dst_offset+1] = i;
			dst[dst_offset+2] = i;
			dst[dst_offset+3] = i;

			src_offset += 1;
			dst_offset += 4;
		}
		src_row_offset += src_row_stride;
		dst_row_offset += dst_row_stride;

		row_swizzle ^= 0x4;   // Alternate lines are word-swapped
	}
}

static void ConvertI4(const TileDestInfo & dsti, const TextureInfo & ti)
{
	u32 width = dsti.Width;
	u32 height = dsti.Height;

	u32 * dst = static_cast<u32*>(dsti.Data);
	u32 dst_row_stride = dsti.Pitch / sizeof(u32);
	u32 dst_row_offset = 0;

	const u8 * src     = gTMEM;

	u32 src_row_stride = ti.GetLine()<<3;
	u32 src_row_offset = ti.GetTmemAddress()<<3;

	u32 row_swizzle = 0;
	for (u32 y = 0; y < height; ++y)
	{
		u32 src_offset = src_row_offset;
		u32 dst_offset = dst_row_offset;

		// Process 2 pixels at a time
		for (u32 x = 0; x+1 < width; x += 2)
		{
			u32 o = src_offset^row_swizzle;
			u8 src_pixel = src[o&0xfff];

			dst[dst_offset+0] = I4(src_pixel>>4);
			dst[dst_offset+1] = I4((src_pixel&0xf));

			src_offset += 1;
			dst_offset += 2;
		}

		// Handle trailing pixel, if odd width
		if (width&1)
		{
			u32 o = src_offset^row_swizzle;
			u8 src_pixel = src[o&0xfff];

			dst[dst_offset+0] = I4(src_pixel>>4);

			src_offset += 1;
			dst_offset += 1;
		}

		src_row_offset += src_row_stride;
		dst_row_offset += dst_row_stride;

		row_swizzle ^= 0x4;   // Alternate lines are word-swapped
	}
}

static void ConvertYUV16(const TileDestInfo & dsti, const TextureInfo & ti)
{
	u32 width = dsti.Width;
	u32 height = dsti.Height;

	u32 * dst = static_cast<u32*>(dsti.Data);
	u32 dst_row_stride = dsti.Pitch / sizeof(u32);
	u32 dst_row_offset = 0;

	const u8 * src     = gTMEM;
	u32 src_row_stride = ti.GetLine()<<3;
	u32 src_row_offset = ti.GetTmemAddress()<<3;

	// NB! YUV/16 line needs to be doubled.
	src_row_stride *= 2;
	u32 row_swizzle = 0;

	for (u32 y = 0; y < height; ++y)
	{
		u32 src_offset = src_row_offset;
		u32 dst_offset = dst_row_offset;
		for (u32 x = 0; x < width; ++x)
		{
			u32 o = src_offset^row_swizzle;
			s32 y0 = src[(o+1)&0xfff];
			s32 y1 = src[(o+3)&0xfff];
			s32 u0 = src[(o+0)&0xfff];
			s32 v0 = src[(o+2)&0xfff];

			dst[dst_offset+0] = YUV16(y0,u0,v0);
			dst[dst_offset+1] = YUV16(y1,u0,v0);

			src_offset += 4;
			dst_offset += 2;
		}
		src_row_offset += src_row_stride;
		dst_row_offset += dst_row_stride;

		row_swizzle ^= 0x4;   // Alternate lines are word-swapped
	}
}

typedef void ( *ConvertFunction )(const TileDestInfo & dsti, const TextureInfo & ti);
static const ConvertFunction gConvertFunctions[ 32 ] =
{
	// 4bpp				8bpp			16bpp				32bpp
	nullptr,			nullptr,		ConvertRGBA16,		ConvertRGBA32,			// RGBA
	nullptr,			nullptr,		ConvertYUV16,		nullptr,				// YUV
	ConvertCI4,			ConvertCI8,		nullptr,			nullptr,				// CI
	ConvertIA4,			ConvertIA8,		ConvertIA16,		nullptr,				// IA
	ConvertI4,			ConvertI8,		nullptr,			nullptr,				// I
	nullptr,			nullptr,		nullptr,			nullptr,				// ?
	nullptr,			nullptr,		nullptr,			nullptr,				// ?
	nullptr,			nullptr,		nullptr,			nullptr					// ?
};

bool ConvertTile(const TextureInfo & ti,
				 void * texels,
				 NativePf8888 * palette,
				 ETextureFormat texture_format,
				 u32 pitch)
{
	DAEDALUS_ASSERT(texture_format == TexFmt_8888, "OSX should only use RGBA 8888 textures");

	TileDestInfo dsti( texture_format );
	dsti.Data    = texels;
	dsti.Width   = ti.GetWidth();
	dsti.Height  = ti.GetHeight();
	dsti.Pitch   = pitch;
	//dsti.Palette = palette;

	DAEDALUS_ASSERT(ti.GetLine() != 0, "No line");

	const ConvertFunction fn = gConvertFunctions[ (ti.GetFormat() << 2) | ti.GetSize() ];
	if( fn )
	{
		fn( dsti, ti );
		return true;
	}

	DAEDALUS_ERROR("Unhandled format %d/%d", ti.GetFormat(), ti.GetSize());
	return false;
}
#endif //DAEDALUS_ACCURATE_TMEM
