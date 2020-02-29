/*
Copyright (C) 2005-2007 StrmnNrmn

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

#include <stdio.h>
#include <png.h>
#include <malloc.h>

#include "stdafx.h"
#include "Graphics/NativeTexture.h"
#include "Graphics/NativePixelFormat.h"
#include "Graphics/ColourValue.h"
#include "Utility/FastMemcpy.h"

#include "Math/MathUtil.h"

#include <gsKit.h>
#include <kernel.h>

extern GSGLOBAL* gsGlobal;
GSTEXTURE* CurrTex;
extern GSTEXTURE DummyTex;
extern u32 texture_vram;
extern u32 clut_vram;

//*****************************************************************************
//
//*****************************************************************************
namespace
{
static const u32 kPalette4BytesRequired = 16 * sizeof( NativePf8888 );
static const u32 kPalette8BytesRequired = 256 * sizeof( NativePf8888 );

//*****************************************************************************
//
//*****************************************************************************
u32	GetTextureBlockWidth( u32 dimension, ETextureFormat texture_format )
{
	#ifdef DAEDALUS_ENABLE_ASSERTS
	DAEDALUS_ASSERT( GetNextPowerOf2( dimension ) == dimension, "This is not a power of 2" );
	#endif

	//not sure about this
	int tbw = 16;
	if (texture_format == TexFmt_CI4_8888 || texture_format == TexFmt_CI8_8888)
		tbw = 8;

	// Ensure that the pitch is at least 16 bytes
	while( CalcBytesRequired(dimension, texture_format) < tbw )
	{
		dimension *= 2;
	}

	return dimension;
}

//*****************************************************************************
//
//*****************************************************************************

u32	CorrectDimension( u32 dimension )
{
	static const u32 MIN_TEXTURE_DIMENSION = 1;
	return Max( GetNextPowerOf2(dimension), MIN_TEXTURE_DIMENSION );
}

}

//*****************************************************************************
//
//*****************************************************************************
CRefPtr<CNativeTexture>	CNativeTexture::Create( u32 width, u32 height, ETextureFormat texture_format )
{
	return new CNativeTexture( width, height, texture_format );
}

//*****************************************************************************
//
//*****************************************************************************
CNativeTexture::CNativeTexture( u32 w, u32 h, ETextureFormat texture_format )
:	mTextureFormat( texture_format )
,	mWidth( w )
,	mHeight( h )
,	mCorrectedWidth( CorrectDimension( w ) )
,	mCorrectedHeight( CorrectDimension( h ) )
,	mTextureBlockWidth( GetTextureBlockWidth( mCorrectedWidth, texture_format ) )
,	mpPalette( nullptr )
,	mTextureConv( false )
{
	mScale.x = 1.0f / (f32)mCorrectedWidth;
	mScale.y = 1.0f / (f32)mCorrectedHeight;

	u32 bytes_required = GetBytesRequired();

	switch (texture_format)
	{
		case TexFmt_5551:		mTexturePs2.PSM = GS_PSM_CT16;
			break;
		case TexFmt_8888:		mTexturePs2.PSM = GS_PSM_CT32;
			break;
		case TexFmt_CI4_8888:	mTexturePs2.PSM = GS_PSM_T4;
			break;
		case TexFmt_CI8_8888:	mTexturePs2.PSM = GS_PSM_T8;
			break;
		case TexFmt_4444:		mTexturePs2.PSM = GS_PSM_CT32;
			mTextureConv = true;
			bytes_required *= 2;
			break;
		case TexFmt_5650:		mTexturePs2.PSM = GS_PSM_CT16;
			mTextureConv = true;
			break;
	}

	mTexturePs2.Width = mCorrectedWidth;
	mTexturePs2.Height = mCorrectedHeight;
	mTexturePs2.Filter = GS_FILTER_LINEAR;

	mpData = mTexturePs2.Mem = (u32*)memalign(128, bytes_required);
	mTexturePs2.Vram = texture_vram;

	if (!mpData)
	{
		printf("CNativeTexture::CNativeTexture: out of memory for texture!\n");

		/*mpData = DummyTex.Mem;
		mCorrectedWidth = DummyTex.Width;
		mCorrectedHeight = DummyTex.Height;
		mTextureBlockWidth = GetTextureBlockWidth(mCorrectedWidth, texture_format);*/
		return;
	}
	else
	{
		memset(mpData, 0, bytes_required);
	}


	if (mTexturePs2.PSM == GS_PSM_T4 || mTexturePs2.PSM == GS_PSM_T8)
	{
		mTexturePs2.ClutPSM = GS_PSM_CT32;
		mTexturePs2.VramClut = clut_vram;
		mpPalette = mTexturePs2.Clut = (u32*)memalign(128, kPalette8BytesRequired);

		if (!mpPalette)
		{
			printf("CNativeTexture::CNativeTexture: out of memory for clut!\n");
			return;
		}
		else
		{
			memset(mpPalette, 0, kPalette8BytesRequired);
		}
	}

	gsKit_setup_tbw(&mTexturePs2);
}

//*****************************************************************************
//
//*****************************************************************************
CNativeTexture::~CNativeTexture()
{
	if(mpData)
		free(mpData);
	
	if(mpPalette);
		free(mpPalette);
}

//*****************************************************************************
//
//*****************************************************************************
bool	CNativeTexture::HasData() const
{
	return mpData != nullptr && (!IsTextureFormatPalettised(mTextureFormat) || mpPalette != nullptr);
}

//*****************************************************************************
//
//*****************************************************************************
void	CNativeTexture::InstallTexture() const
{
	u32 bytes_required = GetBytesRequired();

	if (!HasData())
	{
		CurrTex = (GSTEXTURE*)&DummyTex;
	}
	else
	{
		CurrTex = (GSTEXTURE*)&mTexturePs2;

		if (mTexturePs2.PSM == GS_PSM_CT32 && mTextureConv)
			bytes_required *= 2;

		if (mTexturePs2.PSM == GS_PSM_T8)
		{
			SyncDCache(mTexturePs2.Clut, (u8*)mTexturePs2.Clut + kPalette8BytesRequired);
			gsKit_texture_send_inline(gsGlobal, mTexturePs2.Clut, 16, 16, mTexturePs2.VramClut, mTexturePs2.ClutPSM, 1, GS_CLUT_PALLETE);
		}
		else if (mTexturePs2.PSM == GS_PSM_T4)
		{
			SyncDCache(mTexturePs2.Clut, (u8*)mTexturePs2.Clut + kPalette8BytesRequired);
			gsKit_texture_send_inline(gsGlobal, mTexturePs2.Clut, 8, 2, mTexturePs2.VramClut, mTexturePs2.ClutPSM, 1, GS_CLUT_PALLETE);
		}

		SyncDCache(mTexturePs2.Mem, (u8*)mTexturePs2.Mem + bytes_required);
		gsKit_texture_send_inline(gsGlobal, mTexturePs2.Mem, mTexturePs2.Width, mTexturePs2.Height, mTexturePs2.Vram, mTexturePs2.PSM, mTexturePs2.TBW, GS_CLUT_NONE);
	}
}


namespace
{
	template< typename T >
	void ReadPngData( u32 width, u32 height, u32 stride, u8 ** p_row_table, int color_type, T * p_dest )
	{
		u8 r=0, g=0, b=0, a=0;

		for ( u32 y = 0; y < height; ++y )
		{
			const u8 * pRow = p_row_table[ y ];

			T * p_dest_row( p_dest );

			for ( u32 x = 0; x < width; ++x )
			{
				switch ( color_type )
				{
				case PNG_COLOR_TYPE_GRAY:
					r = g = b = *pRow++;
					if ( r == 0 && g == 0 && b == 0 )	a = 0x00;
					else								a = 0xff;
					break;
				case PNG_COLOR_TYPE_GRAY_ALPHA:
					r = g = b = *pRow++;
					if ( r == 0 && g == 0 && b == 0 )	a = 0x00;
					else								a = 0xff;
					pRow++;
					break;
				case PNG_COLOR_TYPE_RGB:
					b = *pRow++;
					g = *pRow++;
					r = *pRow++;
					if ( r == 0 && g == 0 && b == 0 )	a = 0x00;
					else								a = 0xff;
					break;
				case PNG_COLOR_TYPE_RGB_ALPHA:
					b = *pRow++;
					g = *pRow++;
					r = *pRow++;
					a = *pRow++;
					break;
				}

				p_dest_row[ x ] = T( r, g, b, a );
			}

			p_dest = reinterpret_cast< T * >( reinterpret_cast< u8 * >( p_dest ) + stride );
		}
	}

	//*****************************************************************************
	//	Thanks 71M/Shazz
	//	p_texture is either an existing texture (in case it must be of the
	//	correct dimensions and format) else a new texture is created and returned.
	//*****************************************************************************
	CRefPtr<CNativeTexture>	LoadPng( const char * p_filename, ETextureFormat texture_format )
	{
		const size_t	SIGNATURE_SIZE = 8;
		u8	signature[ SIGNATURE_SIZE ];

		FILE * fh( fopen( p_filename,"rb" ) );
		if(fh == nullptr)
		{
			return nullptr;
		}

		if (fread( signature, sizeof(u8), SIGNATURE_SIZE, fh ) != SIGNATURE_SIZE)
		{
			fclose(fh);
			return nullptr;
		}

		if ( !png_check_sig( signature, SIGNATURE_SIZE ) )
		{
			return nullptr;
		}

		png_struct * p_png_struct( png_create_read_struct( PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr ) );
		if ( p_png_struct == nullptr)
		{
			return nullptr;
		}

		png_info * p_png_info( png_create_info_struct( p_png_struct ) );
		if ( p_png_info == nullptr )
		{
			png_destroy_read_struct( &p_png_struct, nullptr, nullptr );
			return nullptr;
		}

		/*if ( setjmp( png_jmpbuf(p_png_struct) ) != 0 )
		{
			png_destroy_read_struct( &p_png_struct, nullptr, nullptr );
			return nullptr;
		}*/

		png_init_io( p_png_struct, fh );
		png_set_sig_bytes( p_png_struct, SIGNATURE_SIZE );
		png_read_png( p_png_struct, p_png_info, PNG_TRANSFORM_STRIP_16 | PNG_TRANSFORM_PACKING | PNG_TRANSFORM_EXPAND | PNG_TRANSFORM_BGR, nullptr );

		png_uint_32 width = png_get_image_width(p_png_struct, p_png_info);//p_png_info->width;
		png_uint_32 height = png_get_image_height(p_png_struct, p_png_info);//p_png_info->height;


		CRefPtr<CNativeTexture>	texture = CNativeTexture::Create( width, height, texture_format );

		#ifdef DAEDALUS_ENABLE_ASSERTS
		DAEDALUS_ASSERT( texture->GetWidth() >= width, "Width is unexpectedly small" );
		DAEDALUS_ASSERT( texture->GetHeight() >= height, "Height is unexpectedly small" );
		DAEDALUS_ASSERT( texture_format == texture->GetFormat(), "Texture format doesn't match" );
		#endif
		u8 *	p_dest( new u8[ texture->GetBytesRequired() ] );
		if( !p_dest )
		{
			texture = nullptr;
		}
		else
		{
			u32		stride( texture->GetStride() );

			switch( texture_format )
			{
				case TexFmt_5650:
					ReadPngData< NativePf5650 >( width, height, stride, png_get_rows(p_png_struct, p_png_info), png_get_color_type(p_png_struct, p_png_info), reinterpret_cast< NativePf5650 * >( p_dest ) );
					break;
				case TexFmt_5551:
					ReadPngData< NativePf5551 >( width, height, stride, png_get_rows(p_png_struct, p_png_info), png_get_color_type(p_png_struct, p_png_info), reinterpret_cast< NativePf5551 * >( p_dest ) );
					break;
				case TexFmt_4444:
					ReadPngData< NativePf4444 >( width, height, stride, png_get_rows(p_png_struct, p_png_info), png_get_color_type(p_png_struct, p_png_info), reinterpret_cast< NativePf4444 * >( p_dest ) );
					break;
				case TexFmt_8888:
					ReadPngData< NativePf8888 >( width, height, stride, png_get_rows(p_png_struct, p_png_info), png_get_color_type(p_png_struct, p_png_info), reinterpret_cast< NativePf8888 * >( p_dest ) );
					break;

			case TexFmt_CI4_8888:
			case TexFmt_CI8_8888:
			#ifdef DAEDALUS_DEBUG_CONSOLE
				DAEDALUS_ERROR( "Can't use palettised format for png." );
				#endif
				break;

			default:
						#ifdef DAEDALUS_DEBUG_CONSOLE
				DAEDALUS_ERROR( "Unhandled texture format" );
				#endif
				break;
			}

			texture->SetData( p_dest, nullptr );
		}

		//
		// Cleanup
		//
		delete [] p_dest;
		png_destroy_read_struct( &p_png_struct, &p_png_info, nullptr );
		fclose(fh);

		return texture;
	}
}

//*****************************************************************************
//
//*****************************************************************************
CRefPtr<CNativeTexture>	CNativeTexture::CreateFromPng( const char * p_filename, ETextureFormat texture_format )
{
	return LoadPng( p_filename, texture_format );
}

//*****************************************************************************
//
//*****************************************************************************
void	CNativeTexture::SetData( void * data, void * palette )
{
	size_t data_len = GetBytesRequired();
	u32 i;

	if (HasData())
	{
		u32 r, g, b, a;
		u8* mem8 = (u8*)mTexturePs2.Mem;
		u16* mem16 = (u16*)mTexturePs2.Mem;
		u32* mem32 = (u32*)mTexturePs2.Mem;
		u8* dat = (u8*)data;
		u8* pal = (u8*)palette;

		memcpy(mpData, data, data_len);
		
		if (mTexturePs2.PSM == GS_PSM_T4)
		{
			for (i = 0; i < 16; i++)
			{
				mTexturePs2.Clut[i] = (u32)pal[i * 4 + 0] | (u32)pal[i * 4 + 1] << 8 | (u32)pal[i * 4 + 2] << 16 | (((u32)pal[i * 4 + 3] + 1) / 2) << 24;
			}
		}
		else if (mTexturePs2.PSM == GS_PSM_T8)
		{
			for (i = 0; i < 256; i++)
			{
				mTexturePs2.Clut[i] = (u32)pal[i * 4 + 0] | (u32)pal[i * 4 + 1] << 8 | (u32)pal[i * 4 + 2] << 16 | (((u32)pal[i * 4 + 3] + 1) / 2) << 24;
			}

			for (i = 0; i < 256; i++)
			{
				if ((i & 0x18) == 8)
				{
					u32 tmp = mTexturePs2.Clut[i];
					mTexturePs2.Clut[i] = mTexturePs2.Clut[i + 8];
					mTexturePs2.Clut[i + 8] = tmp;
				}
			}
		}
		else if (mTexturePs2.PSM == GS_PSM_CT16)
		{
			if (mTextureConv)  //565
			{
				for (i = 0; i < mTexturePs2.Width * mTexturePs2.Height; i++)
				{
					r = mem16[i] & 0x1F;
					g = (mem16[i] >> 6) & 0x1F;
					b = (mem16[i] >> 11) & 0x1F;
					a = 1;
					mem16[i] = r | g << 5 | b << 10 | a << 15;
				}
			}
		}
		else if (mTexturePs2.PSM == GS_PSM_CT32)
		{
			if (mTextureConv) //4444
			{
				for (i = 0; i < mTexturePs2.Width * mTexturePs2.Height; i++)
				{
					r = ((dat[i * 2 + 0]) & 0xF) << 4;
					g = ((dat[i * 2 + 0] >> 4) & 0xF) << 4;
					b = ((dat[i * 2 + 1]) & 0xF) << 4;
					a = ((dat[i * 2 + 1] >> 4) & 0xF) << 4;

					mem32[i] = r | g << 8 | b << 16 | ((a + 1) / 2) << 24;
				}

				data_len *= 2;
			}
			else
			{
				for (i = 3; i < mTexturePs2.Width * mTexturePs2.Height * 4; i += 4)
				{
					mem8[i] = ((u32)mem8[i] + 1) / 2;
				}
			}
		}
	}
}

//*****************************************************************************
//
//*****************************************************************************
u32	CNativeTexture::GetStride() const
{
	return CalcBytesRequired( mTextureBlockWidth, mTextureFormat );

}
//*****************************************************************************
//
//*****************************************************************************

u32		CNativeTexture::GetBytesRequired() const
{
	return GetStride() * mCorrectedHeight;
}
