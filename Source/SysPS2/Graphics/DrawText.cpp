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
#include "DrawText.h"

#include <gsKit.h>

#include "Graphics/NativeTexture.h"
#include "Math/Vector2.h"
#include "Math/Vector3.h"
#include "SysPS2/Utility/PathsPS2.h"
#include "Utility/Macros.h"
#include "Utility/Preferences.h"
#include "Utility/Translate.h"

extern GSGLOBAL* gsGlobal;
extern GSFONTM* gsFontM;

static float fontScale[2];

//*************************************************************************************
//
//*************************************************************************************
void	CDrawText::Initialise()
{
	fontScale[F_REGULAR] = 0.75f;
	fontScale[F_LARGE_BOLD] = 1.10f;
}

//*************************************************************************************
//
//*************************************************************************************
void	CDrawText::Destroy()
{

}

//*************************************************************************************
//
//*************************************************************************************
const char * CDrawText::Translate( const char * dest, u32 & length )
{
	return Translate_Strings( dest, length );
}

//*************************************************************************************
//
//*************************************************************************************
u32	CDrawText::Render( EFont font, s32 x, s32 y, float scale, const char * p_str, u32 length, c32 colour )
{
	return Render( font, x, y, scale, p_str, length, colour, c32( 0,0,0,160 ) );
}

//*************************************************************************************
//
//*************************************************************************************
u32	CDrawText::Render( EFont font_type, s32 x, s32 y, float scale, const char * p_str, u32 length, c32 colour, c32 drop_colour )
{
	char prt_buf[255];

	strncpy(prt_buf, Translate(p_str, length), length);
	prt_buf[length] = 0;
	/*if( font )
	{
		sceGuEnable(GU_BLEND);
		sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
		intraFontSetStyle( font, scale, colour.GetColour(), 0,0, INTRAFONT_ALIGN_LEFT );
		return s32( intraFontPrintEx( font,  x, y, Translate( p_str, length ), length) ) - x;
	}

	return strlen( p_str ) * 16;		// Guess. Better off just returning 0?*/

	u64 FontColour = GS_SETREG_RGBAQ(colour.GetR(), colour.GetG(), colour.GetB(), (colour.GetA()) / 2, 0x00);
	gsFontM->Align = GSKIT_FALIGN_LEFT;
	gsFontM->Spacing = 0.7f;
	gsKit_fontm_print_scaled(gsGlobal, gsFontM, x, y, 0, fontScale[font_type] * scale, FontColour, prt_buf);

	//printf("Render %f scale %f\n", fontScale[font_type], scale);

	return s32(26.0f * gsFontM->Spacing * fontScale[font_type] * scale * length);
}

//*************************************************************************************
//
//*************************************************************************************
s32		CDrawText::GetTextWidth( EFont font_type, const char * p_str, u32 length, float scale )
{

	/*intraFont * font( gFonts[ font_type ] );
	if( font )
	{
		intraFontSetStyle( font, 1.0f, 0xffffffff, 0,0, INTRAFONT_ALIGN_LEFT );
		return s32( intraFontMeasureTextEx( font, Translate( p_str, length ), length ) );
	}*/

	/*u64 FontColour = GS_SETREG_RGBAQ(0xFF, 0xFF, 0xFF, 0x80, 0x00);

	gsFontM->Align = GSKIT_FALIGN_LEFT;
	gsFontM->Spacing = 0.5f;
	gsKit_fontm_print_scaled(gsGlobal, gsFontM, 0, 0, 0, 1.0f, FontColour, Translate(p_str, length));*/

	//printf("GetTextWidth %s %d %d \n", p_str, length, s32(26.0f * gsFontM->Spacing * FONT_SCALE * length));
	//printf("GetTextWidth %f\n", fontScale[font_type]);
	
	return s32(26.0f * gsFontM->Spacing * fontScale[font_type] * scale * length);
}

//*************************************************************************************
//
//*************************************************************************************
s32		CDrawText::GetFontHeight( EFont font_type )
{
	/*intraFont * font( gFonts[ font_type ] );
	if( font )
	{
		s32		pixels( ( s32( font->advancey ) + 3 ) / 4 );
		return pixels;
	}*/

	return s32(26.0f * fontScale[font_type]);
}

//*************************************************************************************
//
//*************************************************************************************
namespace DrawTextUtilities
{
	const c32	TextWhite			= c32( 255, 255, 255 );
	const c32	TextWhiteDisabled	= c32( 208, 208, 208 );
	const c32	TextBlue			= c32(  80,  80, 208 );
	const c32	TextBlueDisabled	= c32(  80,  80, 178 );
	const c32	TextRed				= c32( 255, 0, 0 );
	const c32	TextRedDisabled		= c32( 208, 208, 208 );

	static c32 COLOUR_SHADOW_HEAVY = c32( 0x80000000 );
	static c32 COLOUR_SHADOW_LIGHT = c32( 0x50000000 );


	const char *	FindPreviousSpace( const char * p_str_start, const char * p_str_end )
	{
		while( p_str_end > p_str_start )
		{
			if( *p_str_end == ' ' )
			{
				return p_str_end;
			}
			p_str_end--;
		}

		// Not found
		return nullptr;
	}

	void	WrapText( CDrawText::EFont font, s32 width, const char * p_str, u32 length, std::vector<u32> & lengths, bool & match, float scale)
	{
		lengths.clear();

		// Manual line breaking (Used for translations)
		if(gGlobalPreferences.Language != 0)
		{
			u32 i, j;
			for (i = 0, j = 0; i < length; i++)
			{
				match = true;
				if (p_str[i] == '\n')
				{
					j++;
					lengths.push_back( match );
				}
			}
			if( match )
			{
				lengths.push_back( match );
			}

			return;
		}

		// Auto-linebreaking
		const char *	p_line_str( p_str );
		const char *	p_str_end( p_str + length );

		while( p_line_str < p_str_end )
		{
			u32		length_remaining( p_str_end - p_line_str );
			s32		chunk_width( CDrawText::GetTextWidth( font, p_line_str, length_remaining, scale ) );

			if( chunk_width <= width )
			{
				lengths.push_back( length_remaining );
				p_line_str += length_remaining;
			}
			else
			{
				// Search backwards until we find a break
				const char *	p_chunk_end( p_str_end );
				bool			found_chunk( false );
				while( p_chunk_end > p_line_str )
				{
					const char * p_space( FindPreviousSpace( p_line_str, p_chunk_end ) );

					if( p_space != nullptr )
					{
						u32		chunk_length( p_space + 1 - p_line_str );
						chunk_width = CDrawText::GetTextWidth( font, p_line_str, chunk_length, scale );
						if( chunk_width <= width )
						{
							lengths.push_back( chunk_length );
							p_line_str += chunk_length;
							found_chunk = true;
							break;
						}
						else
						{
							// Need to try again with the previous space
							p_chunk_end = p_space - 1;
						}
					}
					else
					{
						// No more spaces - just render the whole chunk
						lengths.push_back( p_chunk_end - p_line_str );
						p_line_str = p_chunk_end;
						found_chunk = true;
						break;
					}
				}
#ifdef DAEDALUS_ENABLE_ASSERTS
				DAEDALUS_ASSERT( found_chunk, "Didn't find chunk while splitting string for rendering?" );
				#endif
			}
		}
	}

}
