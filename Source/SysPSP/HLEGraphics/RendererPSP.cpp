#include "stdafx.h"
#include "RendererPSP.h"

#include <pspgu.h>

#include "Core/ROM.h"
#include "Debug/Dump.h"
#include "Graphics/NativeTexture.h"
#include "HLEGraphics/Combiner/BlendConstant.h"
#include "HLEGraphics/Combiner/RenderSettings.h"
#include "HLEGraphics/Texture.h"
#include "Math/MathUtil.h"
#include "OSHLE/ultra_gbi.h"
#include "Utility/IO.h"

// FIXME - surely these should be defined by a system header? Or GU_TRUE etc?
#define GL_TRUE                           1
#define GL_FALSE                          0

BaseRenderer * gRenderer    = NULL;
RendererPSP  * gRendererPSP = NULL;

extern void InitBlenderMode( u32 blender );

#ifdef DAEDALUS_DEBUG_DISPLAYLIST

extern void PrintMux( FILE * fh, u64 mux );

//***************************************************************************
//*General blender used for Blend Explorer when debuging Dlists //Corn
//***************************************************************************
extern DebugBlendSettings gDBlend;

#define BLEND_MODE_MAKER \
{ \
	const u32 PSPtxtFunc[5] = \
	{ \
		GU_TFX_MODULATE, \
		GU_TFX_BLEND, \
		GU_TFX_ADD, \
		GU_TFX_REPLACE, \
		GU_TFX_DECAL \
	}; \
	const u32 PSPtxtA[2] = \
	{ \
		GU_TCC_RGB, \
		GU_TCC_RGBA \
	}; \
	switch( gDBlend.ForceRGB ) \
	{ \
		case 1: details.ColourAdjuster.SetRGB( c32::White ); break; \
		case 2: details.ColourAdjuster.SetRGB( c32::Black ); break; \
		case 3: details.ColourAdjuster.SetRGB( c32::Red ); break; \
		case 4: details.ColourAdjuster.SetRGB( c32::Green ); break; \
		case 5: details.ColourAdjuster.SetRGB( c32::Blue ); break; \
		case 6: details.ColourAdjuster.SetRGB( c32::Magenta ); break; \
		case 7: details.ColourAdjuster.SetRGB( c32::Gold ); break; \
	} \
	switch( gDBlend.SetRGB ) \
	{ \
		case 1: details.ColourAdjuster.SetRGB( details.PrimColour ); break; \
		case 2: details.ColourAdjuster.SetRGB( details.PrimColour.ReplicateAlpha() ); break; \
		case 3: details.ColourAdjuster.SetRGB( details.EnvColour ); break; \
		case 4: details.ColourAdjuster.SetRGB( details.EnvColour.ReplicateAlpha() ); break; \
	} \
	switch( gDBlend.SetA ) \
	{ \
		case 1: details.ColourAdjuster.SetA( details.PrimColour ); break; \
		case 2: details.ColourAdjuster.SetA( details.PrimColour.ReplicateAlpha() ); break; \
		case 3: details.ColourAdjuster.SetA( details.EnvColour ); break; \
		case 4: details.ColourAdjuster.SetA( details.EnvColour.ReplicateAlpha() ); break; \
	} \
	switch( gDBlend.SetRGBA ) \
	{ \
		case 1: details.ColourAdjuster.SetRGBA( details.PrimColour ); break; \
		case 2: details.ColourAdjuster.SetRGBA( details.PrimColour.ReplicateAlpha() ); break; \
		case 3: details.ColourAdjuster.SetRGBA( details.EnvColour ); break; \
		case 4: details.ColourAdjuster.SetRGBA( details.EnvColour.ReplicateAlpha() ); break; \
	} \
	switch( gDBlend.ModRGB ) \
	{ \
		case 1: details.ColourAdjuster.ModulateRGB( details.PrimColour ); break; \
		case 2: details.ColourAdjuster.ModulateRGB( details.PrimColour.ReplicateAlpha() ); break; \
		case 3: details.ColourAdjuster.ModulateRGB( details.EnvColour ); break; \
		case 4: details.ColourAdjuster.ModulateRGB( details.EnvColour.ReplicateAlpha() ); break; \
	} \
	switch( gDBlend.ModA ) \
	{ \
		case 1: details.ColourAdjuster.ModulateA( details.PrimColour ); break; \
		case 2: details.ColourAdjuster.ModulateA( details.PrimColour.ReplicateAlpha() ); break; \
		case 3: details.ColourAdjuster.ModulateA( details.EnvColour ); break; \
		case 4: details.ColourAdjuster.ModulateA( details.EnvColour.ReplicateAlpha() ); break; \
	} \
	switch( gDBlend.ModRGBA ) \
	{ \
		case 1: details.ColourAdjuster.ModulateRGBA( details.PrimColour ); break; \
		case 2: details.ColourAdjuster.ModulateRGBA( details.PrimColour.ReplicateAlpha() ); break; \
		case 3: details.ColourAdjuster.ModulateRGBA( details.EnvColour ); break; \
		case 4: details.ColourAdjuster.ModulateRGBA( details.EnvColour.ReplicateAlpha() ); break; \
	} \
	switch( gDBlend.SubRGB ) \
	{ \
		case 1: details.ColourAdjuster.SubtractRGB( details.PrimColour ); break; \
		case 2: details.ColourAdjuster.SubtractRGB( details.PrimColour.ReplicateAlpha() ); break; \
		case 3: details.ColourAdjuster.SubtractRGB( details.EnvColour ); break; \
		case 4: details.ColourAdjuster.SubtractRGB( details.EnvColour.ReplicateAlpha() ); break; \
	} \
	switch( gDBlend.SubA ) \
	{ \
		case 1: details.ColourAdjuster.SubtractA( details.PrimColour ); break; \
		case 2: details.ColourAdjuster.SubtractA( details.PrimColour.ReplicateAlpha() ); break; \
		case 3: details.ColourAdjuster.SubtractA( details.EnvColour ); break; \
		case 4: details.ColourAdjuster.SubtractA( details.EnvColour.ReplicateAlpha() ); break; \
	} \
	switch( gDBlend.SubRGBA ) \
	{ \
		case 1: details.ColourAdjuster.SubtractRGBA( details.PrimColour ); break; \
		case 2: details.ColourAdjuster.SubtractRGBA( details.PrimColour.ReplicateAlpha() ); break; \
		case 3: details.ColourAdjuster.SubtractRGBA( details.EnvColour ); break; \
		case 4: details.ColourAdjuster.SubtractRGBA( details.EnvColour.ReplicateAlpha() ); break; \
	} \
	if( gDBlend.AOpaque ) details.ColourAdjuster.SetAOpaque(); \
	switch( gDBlend.sceENV ) \
	{ \
		case 1: sceGuTexEnvColor( details.EnvColour.GetColour() ); break; \
		case 2: sceGuTexEnvColor( details.PrimColour.GetColour() ); break; \
	} \
	details.InstallTexture = gDBlend.TexInstall; \
	sceGuTexFunc( PSPtxtFunc[ (gDBlend.TXTFUNC >> 1) % 6 ], PSPtxtA[ gDBlend.TXTFUNC & 1 ] ); \
} \

#endif // DAEDALUS_DEBUG_DISPLAYLIST



void RendererPSP::RenderUsingCurrentBlendMode( DaedalusVtx * p_vertices, u32 num_vertices, u32 triangle_mode, u32 render_mode, bool disable_zbuffer )
{
	static bool	ZFightingEnabled( false );

	DAEDALUS_PROFILE( "BaseRenderer::RenderUsingCurrentBlendMode" );

	if ( disable_zbuffer )
	{
		sceGuDisable(GU_DEPTH_TEST);
		sceGuDepthMask( GL_TRUE );	// GL_TRUE to disable z-writes
	}
	else
	{
		// Fixes Zfighting issues we have on the PSP.
		if( gRDPOtherMode.zmode == 3 )
		{
			if( !ZFightingEnabled )
			{
				ZFightingEnabled = true;
				sceGuDepthRange(65535,80);
			}
		}
		else if( ZFightingEnabled )
		{
			ZFightingEnabled = false;
			sceGuDepthRange(65535,0);
		}

		// Enable or Disable ZBuffer test
		if ( (mTnL.Flags.Zbuffer & gRDPOtherMode.z_cmp) | gRDPOtherMode.z_upd )
		{
			sceGuEnable(GU_DEPTH_TEST);
		}
		else
		{
			sceGuDisable(GU_DEPTH_TEST);
		}

		// GL_TRUE to disable z-writes
		sceGuDepthMask( gRDPOtherMode.z_upd ? GL_FALSE : GL_TRUE );
	}

	// Initiate Texture Filter
	//
	// G_TF_AVERAGE : 1, G_TF_BILERP : 2 (linear)
	// G_TF_POINT   : 0 (nearest)
	//
	if( (gRDPOtherMode.text_filt != G_TF_POINT) | (gGlobalPreferences.ForceLinearFilter) )
	{
		sceGuTexFilter(GU_LINEAR,GU_LINEAR);
	}
	else
	{
		sceGuTexFilter(GU_NEAREST,GU_NEAREST);
	}

	u32 cycle_mode = gRDPOtherMode.cycle_type;

	// Initiate Blender
	//
	if(cycle_mode < CYCLE_COPY)
	{
		gRDPOtherMode.force_bl ? InitBlenderMode( gRDPOtherMode.blender ) : sceGuDisable( GU_BLEND );
	}

	// Initiate Alpha test
	//
	if( (gRDPOtherMode.alpha_compare == G_AC_THRESHOLD) && !gRDPOtherMode.alpha_cvg_sel )
	{
		// G_AC_THRESHOLD || G_AC_DITHER
		sceGuAlphaFunc( (mAlphaThreshold | g_ROM.ALPHA_HACK) ? GU_GEQUAL : GU_GREATER, mAlphaThreshold, 0xff);
		sceGuEnable(GU_ALPHA_TEST);
	}
	// I think this implies that alpha is coming from
	else if (gRDPOtherMode.cvg_x_alpha)
	{
		// Going over 0x70 breaks OOT, but going lesser than that makes lines on games visible...ex: Paper Mario.
		// ALso going over 0x30 breaks the birds in Tarzan :(. Need to find a better way to leverage this.
		sceGuAlphaFunc(GU_GREATER, 0x70, 0xff);
		sceGuEnable(GU_ALPHA_TEST);
	}
	else
	{
		// Use CVG for pixel alpha
        sceGuDisable(GU_ALPHA_TEST);
	}

	SBlendStateEntry		blend_entry;

	switch ( cycle_mode )
	{
		case CYCLE_COPY:		blend_entry.States = mCopyBlendStates; break;
		case CYCLE_FILL:		blend_entry.States = mFillBlendStates; break;
		case CYCLE_1CYCLE:		blend_entry = LookupBlendState( mMux, false ); break;
		case CYCLE_2CYCLE:		blend_entry = LookupBlendState( mMux, true ); break;
	}

	u32 render_flags( GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | render_mode );

#ifdef DAEDALUS_DEBUG_DISPLAYLIST
	// Used for Blend Explorer, or Nasty texture
	//
	if( DebugBlendmode( p_vertices, num_vertices, triangle_mode, render_flags, mMux ) )
		return;
#endif

	// This check is for inexact blends which were handled either by a custom blendmode or auto blendmode thing
	//
	if( blend_entry.OverrideFunction != NULL )
	{
#ifdef DAEDALUS_DEBUG_DISPLAYLIST
		// Used for dumping mux and highlight inexact blend
		//
		DebugMux( blend_entry.States, p_vertices, num_vertices, triangle_mode, render_flags, mMux );
#endif

		// Local vars for now
		SBlendModeDetails details;

		details.EnvColour = mEnvColour;
		details.PrimColour = mPrimitiveColour;
		details.InstallTexture = true;
		details.ColourAdjuster.Reset();

		blend_entry.OverrideFunction( details );

		bool installed_texture( false );

		if( details.InstallTexture )
		{
			if( mpTexture[ g_ROM.T1_HACK ] != NULL )
			{
				const CRefPtr<CNativeTexture> texture( mpTexture[ g_ROM.T1_HACK ]->GetTexture() );

				if(texture != NULL)
				{
					texture->InstallTexture();
					installed_texture = true;
				}
			}
		}

		// If no texture was specified, or if we couldn't load it, clear it out
		if( !installed_texture )
		{
			sceGuDisable( GU_TEXTURE_2D );
		}

		details.ColourAdjuster.Process( p_vertices, num_vertices );

		sceGuDrawArray( triangle_mode, render_flags, num_vertices, NULL, p_vertices );
	}
	else if( blend_entry.States != NULL )
	{
		RenderUsingRenderSettings( blend_entry.States, p_vertices, num_vertices, triangle_mode, render_flags );
	}
	else
	{
		// Set default states
		DAEDALUS_ERROR( "Unhandled blend mode" );
		sceGuDisable( GU_TEXTURE_2D );
		sceGuDrawArray( triangle_mode, render_flags, num_vertices, NULL, p_vertices );
	}
}

void RendererPSP::RenderUsingRenderSettings( const CBlendStates * states, DaedalusVtx * p_vertices, u32 num_vertices, u32 triangle_mode, u32 render_flags)
{
	DAEDALUS_PROFILE( "RendererPSP::RenderUsingRenderSettings" );

	const CAlphaRenderSettings *	alpha_settings( states->GetAlphaSettings() );

	SRenderState	state;

	state.Vertices = p_vertices;
	state.NumVertices = num_vertices;
	state.PrimitiveColour = mPrimitiveColour;
	state.EnvironmentColour = mEnvColour;

	static std::vector< DaedalusVtx >	saved_verts;

	if( states->GetNumStates() > 1 )
	{
		saved_verts.resize( num_vertices );
		memcpy( &saved_verts[0], p_vertices, num_vertices * sizeof( DaedalusVtx ) );
	}


	for( u32 i = 0; i < states->GetNumStates(); ++i )
	{
		const CRenderSettings *		settings( states->GetColourSettings( i ) );

		bool install_texture0( settings->UsesTexture0() || alpha_settings->UsesTexture0() );
		bool install_texture1( settings->UsesTexture1() || alpha_settings->UsesTexture1() );

		SRenderStateOut out;

		memset( &out, 0, sizeof( out ) );

		settings->Apply( install_texture0 || install_texture1, state, out );
		alpha_settings->Apply( install_texture0 || install_texture1, state, out );

		// TODO: this nobbles the existing diffuse colour on each pass. Need to use a second buffer...
		if( i > 0 )
		{
			memcpy( p_vertices, &saved_verts[0], num_vertices * sizeof( DaedalusVtx ) );
		}

		if(out.VertexExpressionRGB != NULL)
		{
			out.VertexExpressionRGB->ApplyExpressionRGB( state );
		}
		if(out.VertexExpressionA != NULL)
		{
			out.VertexExpressionA->ApplyExpressionAlpha( state );
		}

		bool installed_texture = false;

		u32 texture_idx( 0 );

		if(install_texture0 || install_texture1)
		{
			u32	tfx( GU_TFX_MODULATE );
			switch( out.BlendMode )
			{
			case PBM_MODULATE:		tfx = GU_TFX_MODULATE; break;
			case PBM_REPLACE:		tfx = GU_TFX_REPLACE; break;
			case PBM_BLEND:			tfx = GU_TFX_BLEND; sceGuTexEnvColor( out.TextureFactor.GetColour() ); break;
			}

			sceGuTexFunc( tfx, out.BlendAlphaMode == PBAM_RGB ? GU_TCC_RGB :  GU_TCC_RGBA );

			if( g_ROM.T1_HACK )
			{
				// NB if install_texture0 and install_texture1 are both set, 1 wins out
				texture_idx = install_texture1;

				if( install_texture1 & mTnL.Flags.Texture && (mTnL.Flags._u32 & (TNL_LIGHT|TNL_TEXGEN)) != (TNL_LIGHT|TNL_TEXGEN) )
				{
					sceGuTexOffset( -mTileTopLeft[ 1 ].x * mTileScale[ 1 ].x, -mTileTopLeft[ 1 ].y * mTileScale[ 1 ].y );
					sceGuTexScale( mTileScale[ 1 ].x, mTileScale[ 1 ].y );
				}
			}
			else
			{
				// NB if install_texture0 and install_texture1 are both set, 0 wins out
				texture_idx = install_texture0 ? 0 : 1;
			}

			if( mpTexture[texture_idx] != NULL )
			{
				CRefPtr<CNativeTexture> texture( mpTexture[ texture_idx ]->GetTexture() );

				if(out.MakeTextureWhite)
				{
					texture = mpTexture[ texture_idx ]->GetRecolouredTexture( c32::White );
				}

				if(texture != NULL)
				{
					texture->InstallTexture();
					installed_texture = true;
				}
			}
		}

		// If no texture was specified, or if we couldn't load it, clear it out
		if( !installed_texture ) sceGuDisable(GU_TEXTURE_2D);

		sceGuTexWrap( mTexWrap[texture_idx].u, mTexWrap[texture_idx].v );

		sceGuDrawArray( triangle_mode, render_flags, num_vertices, NULL, p_vertices );
	}
}

void RendererPSP::Draw2DTexture(f32 frameX, f32 frameY, f32 frameW, f32 frameH, f32 imageX, f32 imageY, f32 imageW, f32 imageH)
{
	DAEDALUS_PROFILE( "RendererPSP::Draw2DTexture" );
	TextureVtx *p_verts = (TextureVtx*)sceGuGetMemory(4*sizeof(TextureVtx));

	sceGuDisable(GU_DEPTH_TEST);
	sceGuDepthMask(GL_TRUE);
	sceGuShadeModel(GU_FLAT);

	sceGuTexFilter(GU_LINEAR, GU_LINEAR);
	sceGuDisable(GU_ALPHA_TEST);
	sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);

	sceGuEnable(GU_BLEND);
	sceGuTexWrap(GU_CLAMP, GU_CLAMP);


	p_verts[0].pos.x = frameX * mN64ToPSPScale.x + mN64ToPSPTranslate.x; // Frame X Offset * X Scale Factor + Screen X Offset
	p_verts[0].pos.y = frameY * mN64ToPSPScale.y + mN64ToPSPTranslate.y; // Frame Y Offset * Y Scale Factor + Screen Y Offset
	p_verts[0].pos.z = 0.0f;
	p_verts[0].t0.x  = imageX;											 // X coordinates
	p_verts[0].t0.y  = imageY;

	p_verts[1].pos.x = frameW * mN64ToPSPScale.x + mN64ToPSPTranslate.x; // Translated X Offset + (Image Width  * X Scale Factor)
	p_verts[1].pos.y = frameY * mN64ToPSPScale.y + mN64ToPSPTranslate.y; // Translated Y Offset + (Image Height * Y Scale Factor)
	p_verts[1].pos.z = 0.0f;
	p_verts[1].t0.x  = imageW;											 // X dimentions
	p_verts[1].t0.y  = imageY;

	p_verts[2].pos.x = frameX * mN64ToPSPScale.x + mN64ToPSPTranslate.x; // Frame X Offset * X Scale Factor + Screen X Offset
	p_verts[2].pos.y = frameH * mN64ToPSPScale.y + mN64ToPSPTranslate.y; // Frame Y Offset * Y Scale Factor + Screen Y Offset
	p_verts[2].pos.z = 0.0f;
	p_verts[2].t0.x  = imageX;											 // X coordinates
	p_verts[2].t0.y  = imageH;

	p_verts[3].pos.x = frameW * mN64ToPSPScale.x + mN64ToPSPTranslate.x; // Translated X Offset + (Image Width  * X Scale Factor)
	p_verts[3].pos.y = frameH * mN64ToPSPScale.y + mN64ToPSPTranslate.y; // Translated Y Offset + (Image Height * Y Scale Factor)
	p_verts[3].pos.z = 0.0f;
	p_verts[3].t0.x  = imageW;											 // X dimentions
	p_verts[3].t0.y  = imageH;											 // Y dimentions

	sceGuDrawArray( GU_TRIANGLE_STRIP, GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_2D, 4, 0, p_verts );
}

void RendererPSP::Draw2DTextureR(f32 x0, f32 y0, f32 x1, f32 y1, f32 x2, f32 y2, f32 x3, f32 y3, f32 s, f32 t)	// With Rotation
{
	DAEDALUS_PROFILE( "RendererPSP::Draw2DTextureR" );
	TextureVtx *p_verts = (TextureVtx*)sceGuGetMemory(4*sizeof(TextureVtx));

	sceGuDisable(GU_DEPTH_TEST);
	sceGuDepthMask(GL_TRUE);
	sceGuShadeModel(GU_FLAT);

	sceGuTexFilter(GU_LINEAR, GU_LINEAR);
	sceGuDisable(GU_ALPHA_TEST);
	sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);

	sceGuEnable(GU_BLEND);
	sceGuTexWrap(GU_CLAMP, GU_CLAMP);

	p_verts[0].pos.x = x0 * mN64ToPSPScale.x + mN64ToPSPTranslate.x;
	p_verts[0].pos.y = y0 * mN64ToPSPScale.y + mN64ToPSPTranslate.y;
	p_verts[0].pos.z = 0.0f;
	p_verts[0].t0.x  = 0.0f;
	p_verts[0].t0.y  = 0.0f;

	p_verts[1].pos.x = x1 * mN64ToPSPScale.x + mN64ToPSPTranslate.x;
	p_verts[1].pos.y = y1 * mN64ToPSPScale.y + mN64ToPSPTranslate.y;
	p_verts[1].pos.z = 0.0f;
	p_verts[1].t0.x  = s;
	p_verts[1].t0.y  = 0.0f;

	p_verts[2].pos.x = x2 * mN64ToPSPScale.x + mN64ToPSPTranslate.x;
	p_verts[2].pos.y = y2 * mN64ToPSPScale.y + mN64ToPSPTranslate.y;
	p_verts[2].pos.z = 0.0f;
	p_verts[2].t0.x  = s;
	p_verts[2].t0.y  = t;

	p_verts[3].pos.x = x3 * mN64ToPSPScale.x + mN64ToPSPTranslate.x;
	p_verts[3].pos.y = y3 * mN64ToPSPScale.y + mN64ToPSPTranslate.y;
	p_verts[3].pos.z = 0.0f;
	p_verts[3].t0.x  = 0.0f;
	p_verts[3].t0.y  = t;

	sceGuDrawArray( GU_TRIANGLE_FAN, GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_2D, 4, 0, p_verts );
}

//*****************************************************************************
//
//	The following blitting code was taken from The TriEngine.
//	See http://www.assembla.com/code/openTRI for more information.
//
//*****************************************************************************
void RendererPSP::Draw2DTextureBlit(f32 x, f32 y, f32 width, f32 height, f32 u0, f32 v0, f32 u1, f32 v1, CNativeTexture * texture)
{
	sceGuDisable(GU_DEPTH_TEST);
	sceGuDepthMask(GL_TRUE);
	sceGuShadeModel(GU_FLAT);

	sceGuTexFilter(GU_LINEAR, GU_LINEAR);
	sceGuDisable(GU_ALPHA_TEST);
	sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);

	sceGuEnable(GU_BLEND);
	sceGuTexWrap(GU_CLAMP, GU_CLAMP);

// 0 Simpler blit algorithm, but doesn't handle big textures as good? (see StarSoldier)
// 1 More complex algorithm. used in newer versions of TriEngine, fixes the main screen in StarSoldier
// Note : We ignore handling height > 512 textures for now
#if 1
	if ( u1 > 512.f )
	{
		s32 off = (u1>u0) ? ((int)u0 & ~31) : ((int)u1 & ~31);

		const u8* data = static_cast<const u8*>( texture->GetData()) + off * GetBitsPerPixel( texture->GetFormat() );
		u1 -= off;
		u0 -= off;
		sceGuTexImage( 0, Min<u32>(512,texture->GetCorrectedWidth()), Min<u32>(512,texture->GetCorrectedHeight()), texture->GetBlockWidth(), data );
	}

	f32 start, end;
	f32 cur_u = u0;
	f32 cur_x = x;
	f32 x_end = width;
	f32 slice = 64.f;
	f32 ustep = (u1-u0)/width * slice;

	// blit maximizing the use of the texture-cache
	for( start=0, end=width; start<end; start+=slice )
	{
		TextureVtx *p_verts = (TextureVtx*)sceGuGetMemory(2*sizeof(TextureVtx));

		f32 poly_width = ((cur_x+slice) > x_end) ? (x_end-cur_x) : slice;
		f32 source_width = ((cur_u+ustep) > u1) ? (u1-cur_u) : ustep;

		p_verts[0].t0.x = cur_u;
		p_verts[0].t0.y = v0;
		p_verts[0].pos.x = cur_x * mN64ToPSPScale.x + mN64ToPSPTranslate.x;
		p_verts[0].pos.y = y	 * mN64ToPSPScale.y + mN64ToPSPTranslate.y;
		p_verts[0].pos.z = 0;

		cur_u += source_width;
		cur_x += poly_width;

		p_verts[1].t0.x = cur_u;
		p_verts[1].t0.y = v1;
		p_verts[1].pos.x = cur_x * mN64ToPSPScale.x + mN64ToPSPTranslate.x;
		p_verts[1].pos.y = height* mN64ToPSPScale.y + mN64ToPSPTranslate.y;
		p_verts[1].pos.z = 0;

		sceGuDrawArray( GU_SPRITES, GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_2D, 2, 0, p_verts );
	}
#else
	f32 cur_v = v0;
	f32 cur_y = y;
	f32 v_end = v1;
	f32 y_end = height;
	f32 vslice = 512.f;
	f32 ystep = (height/(v1-v0) * vslice);
	f32 vstep = ((v1-v0) > 0 ? vslice : -vslice);

	f32 x_end = width;
	f32 uslice = 64.f;
	//f32 ustep = (u1-u0)/width * xslice;
	f32 xstep = (width/(u1-u0) * uslice);
	f32 ustep = ((u1-u0) > 0 ? uslice : -uslice);

	const u8* data = static_cast<const u8*>(texture->GetData());

	for ( ; cur_y < y_end; cur_y+=ystep, cur_v+=vstep )
	{
		f32 cur_u = u0;
		f32 cur_x = x;
		f32 u_end = u1;

		f32 poly_height = ((cur_y+ystep) > y_end) ? (y_end-cur_y) : ystep;
		f32 source_height = vstep;

		// support negative vsteps
		if ((vstep > 0) && (cur_v+vstep > v_end))
		{
			source_height = (v_end-cur_v);
		}
		else if ((vstep < 0) && (cur_v+vstep < v_end))
		{
			source_height = (cur_v-v_end);
		}

		const u8* udata = data;
		// blit maximizing the use of the texture-cache
		for( ; cur_x < x_end; cur_x+=xstep, cur_u+=ustep )
		{
			// support large images (width > 512)
			if (cur_u>512.f || cur_u+ustep>512.f)
			{
				s32 off = (ustep>0) ? ((int)cur_u & ~31) : ((int)(cur_u+ustep) & ~31);

				udata += off * GetBitsPerPixel( texture->GetFormat() );
				cur_u -= off;
				u_end -= off;
				sceGuTexImage(0, Min<u32>(512,texture->GetCorrectedWidth()), Min<u32>(512,texture->GetCorrectedHeight()), texture->GetBlockWidth(), udata);
			}
			TextureVtx *p_verts = (TextureVtx*)sceGuGetMemory(2*sizeof(TextureVtx));

			//f32 poly_width = ((cur_x+xstep) > x_end) ? (x_end-cur_x) : xstep;
			f32 poly_width = xstep;
			f32 source_width = ustep;

			// support negative usteps
			if ((ustep > 0) && (cur_u+ustep > u_end))
			{
				source_width = (u_end-cur_u);
			}
			else if ((ustep < 0) && (cur_u+ustep < u_end))
			{
				source_width = (cur_u-u_end);
			}

			p_verts[0].t0.x = cur_u;
			p_verts[0].t0.y = cur_v;
			p_verts[0].pos.x = cur_x;
			p_verts[0].pos.y = cur_y;
			p_verts[0].pos.z = 0;

			p_verts[1].t0.x = cur_u + source_width;
			p_verts[1].t0.y = cur_v + source_height;
			p_verts[1].pos.x = cur_x + poly_width;
			p_verts[1].pos.y = cur_y + poly_height;
			p_verts[1].pos.z = 0;

			sceGuDrawArray( GU_SPRITES, GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_2D, 2, 0, p_verts );
		}
	}
#endif
}

//*****************************************************************************
//
//*****************************************************************************
#ifdef DAEDALUS_DEBUG_DISPLAYLIST
void RendererPSP::SelectPlaceholderTexture( EPlaceholderTextureType type )
{
	switch( type )
	{
	case PTT_WHITE:			sceGuTexImage(0,gPlaceholderTextureWidth,gPlaceholderTextureHeight,gPlaceholderTextureWidth,gWhiteTexture); break;
	case PTT_SELECTED:		sceGuTexImage(0,gPlaceholderTextureWidth,gPlaceholderTextureHeight,gPlaceholderTextureWidth,gSelectedTexture); break;
	case PTT_MISSING:		sceGuTexImage(0,gPlaceholderTextureWidth,gPlaceholderTextureHeight,gPlaceholderTextureWidth,gPlaceholderTexture); break;
	default:
		DAEDALUS_ERROR( "Unhandled type" );
		break;
	}
}
#endif // DAEDALUS_DEBUG_DISPLAYLIST

#ifdef DAEDALUS_DEBUG_DISPLAYLIST
//*****************************************************************************
// Used for Blend Explorer, or Nasty texture
//*****************************************************************************
bool RendererPSP::DebugBlendmode( DaedalusVtx * p_vertices, u32 num_vertices, u32 triangle_mode, u32 render_flags, u64 mux )
{
	if( IsCombinerStateDisabled( mux ) )
	{
		if( mNastyTexture )
		{
			// Use the nasty placeholder texture
			//
			sceGuEnable(GU_TEXTURE_2D);
			SelectPlaceholderTexture( PTT_SELECTED );
			sceGuTexFunc(GU_TFX_REPLACE,GU_TCC_RGBA);
			sceGuTexMode(GU_PSM_8888,0,0,GL_TRUE);		// maxmips/a2/swizzle = 0
			sceGuDrawArray( triangle_mode, render_flags, num_vertices, NULL, p_vertices );
		}
		else
		{
			//Allow Blend Explorer
			//
			SBlendModeDetails		details;

			details.InstallTexture = true;
			details.EnvColour = mEnvColour;
			details.PrimColour = mPrimitiveColour;
			details.ColourAdjuster.Reset();

			//Insert the Blend Explorer
			BLEND_MODE_MAKER

			bool	installed_texture( false );

			if( details.InstallTexture )
			{
				if( mpTexture[ 0 ] != NULL )
				{
					const CRefPtr<CNativeTexture> texture( mpTexture[ 0 ]->GetTexture() );

					if(texture != NULL)
					{
						texture->InstallTexture();
						installed_texture = true;
					}
				}
			}

			// If no texture was specified, or if we couldn't load it, clear it out
			if( !installed_texture ) sceGuDisable( GU_TEXTURE_2D );

			details.ColourAdjuster.Process( p_vertices, num_vertices );
			sceGuDrawArray( triangle_mode, render_flags, num_vertices, NULL, p_vertices );
		}

		return true;
	}

	return false;
}
#endif // DAEDALUS_DEBUG_DISPLAYLIST


#ifdef DAEDALUS_DEBUG_DISPLAYLIST
void RendererPSP::DebugMux( const CBlendStates * states, DaedalusVtx * p_vertices, u32 num_vertices, u32 triangle_mode, u32 render_flags, u64 mux)
{
	// Only dump missing_mux when we awant to search for inexact blends aka HighlightInexactBlendModes is enabled.
	// Otherwise will dump lotsa of missing_mux even though is not needed since was handled correctly by auto blendmode thing - Salvy
	//
	if( gGlobalPreferences.HighlightInexactBlendModes && states->IsInexact() )
	{
		if(mUnhandledCombinerStates.find( mux ) == mUnhandledCombinerStates.end())
		{
			char szFilePath[MAX_PATH+1];

			Dump_GetDumpDirectory(szFilePath, g_ROM.settings.GameName.c_str());

			IO::Path::Append(szFilePath, "missing_mux.txt");

			FILE * fh( fopen(szFilePath, mUnhandledCombinerStates.empty() ? "w" : "a") );
			if(fh != NULL)
			{
				PrintMux( fh, mux );
				fclose(fh);
			}

			mUnhandledCombinerStates.insert( mux );
		}

		sceGuEnable( GU_TEXTURE_2D );
		sceGuTexMode( GU_PSM_8888, 0, 0, GL_TRUE );		// maxmips/a2/swizzle = 0

		// Use the nasty placeholder texture
		SelectPlaceholderTexture( PTT_MISSING );
		sceGuTexFunc( GU_TFX_REPLACE, GU_TCC_RGBA );
		sceGuDrawArray( triangle_mode, render_flags, num_vertices, NULL, p_vertices );
	}
}

#endif // DAEDALUS_DEBUG_DISPLAYLIST





bool CreateRenderer()
{
	DAEDALUS_ASSERT_Q(gRenderer == NULL);
	gRendererPSP = new RendererPSP();
	gRenderer    = gRendererPSP;
	return true;
}
void DestroyRenderer()
{
	delete gRendererPSP;
	gRendererPSP = NULL;
	gRenderer    = NULL;
}
