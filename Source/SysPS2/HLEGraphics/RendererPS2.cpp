#include "stdafx.h"
#include "RendererPS2.h"
#include "Core/ROM.h"
#include "Debug/Dump.h"
#include "Combiner/BlendConstant.h"
#include "Combiner/CombinerTree.h"
#include "Combiner/RenderSettings.h"
#include "Graphics/GraphicsContext.h"
#include "Graphics/NativeTexture.h"
#include "HLEGraphics/CachedTexture.h"
#include "HLEGraphics/DLDebug.h"
#include "HLEGraphics/RDPStateManager.h"
#include "HLEGraphics/TextureCache.h"
#include "Math/MathUtil.h"
#include "OSHLE/ultra_gbi.h"
#include "Utility/IO.h"
#include "Utility/Profiler.h"
#include "SysPS2/GL.h"

#include <malloc.h>
#include <gsKit.h>
#include <libvux.h>
#include <kernel.h>
#include <gsInline.h>

#define ATST_NEVER		0
#define ATST_ALWAYS		1
#define ATST_LESS		2
#define ATST_LEQUAL		3
#define ATST_EQUAL		4
#define ATST_GEQUAL		5
#define ATST_GREATER	6
#define ATST_NOTEQUAL	7

#define ZTST_NEVER		0
#define ZTST_ALWAYS		1
#define ZTST_GEQUAL		2
#define ZTST_GREATER	3

#define GS_SETREG_GS_FOGCOL(FCR, FCG, FCB) \
	((u64)(FCR)	<< 0)	| \
	((u64)(FCG)	<< 8)	| \
	((u64)(FCB)	<< 16)

#define GS_SETREG_GS_FOG(F) \
	((u64)(F)	<< 0)

extern GSGLOBAL* gsGlobal;
extern GSTEXTURE* CurrTex;
extern GSFONTM* gsFontM;

extern void InitBlenderMode(u32 blender);

BaseRenderer * gRenderer    = nullptr;
RendererPS2  * gRendererPS2 = nullptr;

static float coord_width = 300.0f;
static float coord_height = 300.0f;
static short coord_x = 640 / 2;
static short coord_y = 480 / 2;
static int coord_near = 0xFFFF;
static int coord_far = 0;
extern int gsZMax;

int gsShading = 0;

void gsViewport(int x, int y, int width, int height)
{
	gsGlobal->OffsetX = (int)((2048.0f - width / 2) * 16.0f);
	gsGlobal->OffsetY = (int)((2048.0f - height / 2) * 16.0f);

	u64* p_data;
	u64* p_store;

	p_data = p_store = (u64*)gsKit_heap_alloc(gsGlobal, 1, 16, GIF_AD);

	*p_data++ = GIF_TAG_AD(1);
	*p_data++ = GIF_AD;

	*p_data++ = GS_SETREG_XYOFFSET_1(gsGlobal->OffsetX, gsGlobal->OffsetY);
	*p_data++ = GS_XYOFFSET_1;

	coord_x = x + width / 2;
	coord_y = y + height / 2;
	coord_width = width / 2;
	coord_height = height / 2;
}

void gsDepthRange(int nearVal, int farVal)
{
	coord_near = nearVal;
	coord_far = farVal;
}

void gsScissor(int x0, int x1, int y0, int y1)
{
	u64* p_data;
	u64* p_store;

	p_data = p_store = (u64 *)gsKit_heap_alloc(gsGlobal, 1, 16, GIF_AD);

	*p_data++ = GIF_TAG_AD(1);
	*p_data++ = GIF_AD;

	*p_data++ = GS_SETREG_SCISSOR_1(x0, x1, y0, y1);
	*p_data++ = GS_SCISSOR_1;
}

void gsTexWrap(int u, int v)
{
	u64* p_data = (u64*)gsKit_heap_alloc(gsGlobal, 1, 16, GIF_AD);

	*p_data++ = GIF_TAG_AD(1);
	*p_data++ = GIF_AD;

	*p_data++ = GS_SETREG_CLAMP(u, v, 0, 0, 0, 0);

	*p_data++ = GS_CLAMP_1 + gsGlobal->PrimContext;
}

static bool useTexEnvColor = false;
static u64 TexEnvColor;

void gsTexEnvColor(int color)
{
	TexEnvColor = GS_SETREG_RGBAQ(color & 0xFF, (color >> 8) & 0xFF, (color >> 16) & 0xFF, ((color >> 24) & 0xFF + 1) / 2, 0x00);
	useTexEnvColor = true;
}

static VU_MATRIX proj;

//FILE *ff;

void sceGuSetMatrix(EGuMatrixType type, const ScePspFMatrix4* mtx)
{
	memcpy(&proj, mtx, sizeof(VU_MATRIX));

	/*char buf[1024 * 2];
	if (!ff)
		ff = fopen("dumpvtx.txt", "a+t");

	if (ff)
	{
		sprintf(buf, "mat ");
		fwrite(buf, 1, strlen(buf), ff);
		for (int i = 0; i < 16; i ++)
		{
			sprintf(buf, "%f ", mtx->m[i]);

			fwrite(buf, 1, strlen(buf), ff);
		}
		sprintf(buf, "\n");
		fwrite(buf, 1, strlen(buf), ff);

		//fclose(fd);
	}*/


	VuSetProjectionMatrix(&proj);
}

void sceGuFog(float near, float far, unsigned int color)
{
	//printf("fog %f %f\n", near, far);
#if 1
	u64 * p_data = (u64*)gsKit_heap_alloc(gsGlobal, 1, 32, GIF_AD);

	*p_data++ = GIF_TAG_AD(2);
	*p_data++ = GIF_AD;

	*p_data++ = GS_SETREG_GS_FOGCOL(color & 0xFF, (color >> 8) & 0xFF, (color >> 16) & 0xFF);

	*p_data++ = GS_FOGCOL;

	*p_data++ = GS_SETREG_GS_FOG((u8)((far - near) * 255)); //?

	*p_data++ = GS_FOG;
#endif
}

#define GS_TFX_MODULATE 0
#define GS_TFX_DECAL 1
#define GS_TFX_REPLACE 2
#define GS_TFX_ADD 3
#define GS_TFX_BLEND 4

#define GS_TCC_RGB 0
#define GS_TCC_RGBA 1

static int gstfunc = 0;
static int gstcc = 1;

void gsTexFunc(int func, int mode)
{
	if (func == GS_TFX_MODULATE || func == GS_TFX_DECAL)
		gstfunc = func;

	gstcc = mode;
}

void gsDepthMask(int mask)
{
#if 0
	u64* p_data;
	u64* p_store;

	p_data = p_store = (u64 *)gsKit_heap_alloc(gsGlobal, 1, 16, GIF_AD);

	*p_data++ = GIF_TAG_AD(1);
	*p_data++ = GIF_AD;

	*p_data++ = GS_SETREG_ZBUF_1(gsGlobal->ZBuffer / 8192, gsGlobal->PSMZ, mask);
	*p_data++ = GS_ZBUF_1;
#endif
	//*p_data++ = GS_SETREG_ZBUF_1(gsGlobal->ZBuffer / 8192, gsGlobal->PSMZ, mask);
	//*p_data++ = GS_ZBUF_2;
}

static inline u32 lzw(u32 val)
{
	u32 res;
	__asm__ __volatile__("   plzcw   %0, %1    " : "=r" (res) : "r" (val));
	return(res);
}

static inline void gsKit_set_tw_th(const GSTEXTURE* Texture, int* tw, int* th)
{
	*tw = 31 - (lzw(Texture->Width) + 1);
	if (Texture->Width > (1 << *tw))
		(*tw)++;

	*th = 31 - (lzw(Texture->Height) + 1);
	if (Texture->Height > (1 << *th))
		(*th)++;
}

void _gsKit_prim_triangle_texture_3d(GSGLOBAL* gsGlobal, GSTEXTURE* Texture,
	float x1, float y1, int iz1, float u1, float v1,
	float x2, float y2, int iz2, float u2, float v2,
	float x3, float y3, int iz3, float u3, float v3, u64 color)
{
	gsKit_set_texfilter(gsGlobal, Texture->Filter);
	u64* p_store;
	u64* p_data;
	int qsize = 5;
	int bsize = 80;

	int tw, th;
	gsKit_set_tw_th(Texture, &tw, &th);

	int ix1 = gsKit_float_to_int_x(gsGlobal, x1);
	int ix2 = gsKit_float_to_int_x(gsGlobal, x2);
	int ix3 = gsKit_float_to_int_x(gsGlobal, x3);
	int iy1 = gsKit_float_to_int_y(gsGlobal, y1);
	int iy2 = gsKit_float_to_int_y(gsGlobal, y2);
	int iy3 = gsKit_float_to_int_y(gsGlobal, y3);

	int iu1 = gsKit_float_to_int_u(Texture, u1);
	int iu2 = gsKit_float_to_int_u(Texture, u2);
	int iu3 = gsKit_float_to_int_u(Texture, u3);
	int iv1 = gsKit_float_to_int_v(Texture, v1);
	int iv2 = gsKit_float_to_int_v(Texture, v2);
	int iv3 = gsKit_float_to_int_v(Texture, v3);

	p_store = p_data = (u64*)gsKit_heap_alloc(gsGlobal, qsize, bsize, GSKIT_GIF_PRIM_TRIANGLE_TEXTURED);

	*p_data++ = GIF_TAG_TRIANGLE_TEXTURED(0);
	*p_data++ = GIF_TAG_TRIANGLE_TEXTURED_REGS(gsGlobal->PrimContext);

	if (Texture->VramClut == 0)
	{
		*p_data++ = GS_SETREG_TEX0(Texture->Vram / 256, Texture->TBW, Texture->PSM,
			tw, th, gstcc, gstfunc,
			0, 0, 0, 0, GS_CLUT_STOREMODE_NOLOAD);
	}
	else
	{
		*p_data++ = GS_SETREG_TEX0(Texture->Vram / 256, Texture->TBW, Texture->PSM,
			tw, th, gstcc, gstfunc,
			Texture->VramClut / 256, Texture->ClutPSM, 0, 0, GS_CLUT_STOREMODE_LOAD);
	}

	*p_data++ = GS_SETREG_PRIM(GS_PRIM_PRIM_TRIANGLE, 0, 1, gsGlobal->PrimFogEnable,
		gsGlobal->PrimAlphaEnable, gsGlobal->PrimAAEnable,
		1, gsGlobal->PrimContext, 0);


	*p_data++ = color;

	*p_data++ = GS_SETREG_UV(iu1, iv1);
	*p_data++ = GS_SETREG_XYZ2(ix1, iy1, iz1);

	*p_data++ = GS_SETREG_UV(iu2, iv2);
	*p_data++ = GS_SETREG_XYZ2(ix2, iy2, iz2);

	*p_data++ = GS_SETREG_UV(iu3, iv3);
	*p_data++ = GS_SETREG_XYZ2(ix3, iy3, iz3);
}

void _gsKit_prim_triangle_goraud_texture_3d(GSGLOBAL* gsGlobal, GSTEXTURE* Texture,
	float x1, float y1, int iz1, float u1, float v1,
	float x2, float y2, int iz2, float u2, float v2,
	float x3, float y3, int iz3, float u3, float v3,
	u64 color1, u64 color2, u64 color3)
{
	gsKit_set_texfilter(gsGlobal, Texture->Filter);
	u64* p_store;
	u64* p_data;
	int qsize = 6;
	int bsize = 96;

	int tw, th;
	gsKit_set_tw_th(Texture, &tw, &th);

	int ix1 = gsKit_float_to_int_x(gsGlobal, x1);
	int ix2 = gsKit_float_to_int_x(gsGlobal, x2);
	int ix3 = gsKit_float_to_int_x(gsGlobal, x3);
	int iy1 = gsKit_float_to_int_y(gsGlobal, y1);
	int iy2 = gsKit_float_to_int_y(gsGlobal, y2);
	int iy3 = gsKit_float_to_int_y(gsGlobal, y3);

	int iu1 = gsKit_float_to_int_u(Texture, u1);
	int iu2 = gsKit_float_to_int_u(Texture, u2);
	int iu3 = gsKit_float_to_int_u(Texture, u3);
	int iv1 = gsKit_float_to_int_v(Texture, v1);
	int iv2 = gsKit_float_to_int_v(Texture, v2);
	int iv3 = gsKit_float_to_int_v(Texture, v3);

	p_store = p_data = (u64 *)gsKit_heap_alloc(gsGlobal, qsize, bsize, GSKIT_GIF_PRIM_TRIANGLE_TEXTURED);

	*p_data++ = GIF_TAG_TRIANGLE_GORAUD_TEXTURED(0);
	*p_data++ = GIF_TAG_TRIANGLE_GORAUD_TEXTURED_REGS(gsGlobal->PrimContext);

	if (Texture->VramClut == 0)
	{
		*p_data++ = GS_SETREG_TEX0(Texture->Vram / 256, Texture->TBW, Texture->PSM,
			tw, th, gstcc, gstfunc,
			0, 0, 0, 0, GS_CLUT_STOREMODE_NOLOAD);
	}
	else
	{
		*p_data++ = GS_SETREG_TEX0(Texture->Vram / 256, Texture->TBW, Texture->PSM,
			tw, th, gstcc, gstfunc,
			Texture->VramClut / 256, Texture->ClutPSM, 0, 0, GS_CLUT_STOREMODE_LOAD);
	}

	*p_data++ = GS_SETREG_PRIM(GS_PRIM_PRIM_TRIANGLE, 1, 1, gsGlobal->PrimFogEnable,
		gsGlobal->PrimAlphaEnable, gsGlobal->PrimAAEnable,
		1, gsGlobal->PrimContext, 0);


	*p_data++ = color1;
	*p_data++ = GS_SETREG_UV(iu1, iv1);
	*p_data++ = GS_SETREG_XYZ2(ix1, iy1, iz1);

	*p_data++ = color2;
	*p_data++ = GS_SETREG_UV(iu2, iv2);
	*p_data++ = GS_SETREG_XYZ2(ix2, iy2, iz2);

	*p_data++ = color3;
	*p_data++ = GS_SETREG_UV(iu3, iv3);
	*p_data++ = GS_SETREG_XYZ2(ix3, iy3, iz3);
}


//#define GS_Z(x)		gsZMax - *reinterpret_cast<int*>(&x)

#define GS_X(v)		(((v.x/v.w) * coord_width) + coord_x)
#define GS_Y(v)		(-((v.y/v.w) * coord_height) + coord_y)
#define GS_Z(v)		(int)(-(v.z/v.w) * 0xffff)

void DrawPrims(DaedalusVtx* p_vertices, u32 num_vertices, u32 prim_type, bool textured)
{
	VU_VECTOR in_vect[3];
	VU_VECTOR out_vect[3];
	u64 color[3];

	VuxUpdateLocalScreenMatrix();

	if (prim_type == GS_PRIM_PRIM_SPRITE)
	{
		for (u32 i = 0; i < num_vertices; i += 2)
		{

			in_vect[0].x = p_vertices[i + 0].Position.x;
			in_vect[0].y = p_vertices[i + 0].Position.y;
			in_vect[0].z = p_vertices[i + 0].Position.z;
			in_vect[0].w = 1.0f;

			in_vect[1].x = p_vertices[i + 1].Position.x;
			in_vect[1].y = p_vertices[i + 1].Position.y;
			in_vect[1].z = p_vertices[i + 1].Position.z;
			in_vect[1].w = 1.0f;

			VuxRotTrans(&in_vect[0], &out_vect[0]);
			VuxRotTrans(&in_vect[1], &out_vect[1]);

			if (textured)
			{
				gsKit_prim_sprite_texture_3d(gsGlobal, CurrTex, 
					in_vect[0].x,
					in_vect[0].y,
					(int)in_vect[0].z,
					p_vertices[i + 0].Texture.x,  // U1
					p_vertices[i + 0].Texture.y,  // V1
					in_vect[1].x, // X2
					in_vect[1].y, // Y2
					(int)in_vect[1].z,
					p_vertices[i + 1].Texture.x, // U2
					p_vertices[i + 1].Texture.y, // V2
					GS_SETREG_RGBAQ(p_vertices[i + 0].Colour.GetR(), p_vertices[i + 0].Colour.GetG(), p_vertices[i + 0].Colour.GetB(), (p_vertices[i + 0].Colour.GetA()) / 2, 0x00));
			}
			else
			{
				gsKit_prim_sprite(gsGlobal, 
					in_vect[0].x, 
					in_vect[0].y, 
					in_vect[1].x, 
					in_vect[1].y, 
					(int)in_vect[0].z, 
					GS_SETREG_RGBAQ(p_vertices[i + 0].Colour.GetR(), p_vertices[i + 0].Colour.GetG(), p_vertices[i + 0].Colour.GetB(), (p_vertices[i + 0].Colour.GetA()) / 2, 0x00));
			}
		}
	}
	else if (prim_type == GS_PRIM_PRIM_TRIANGLE)
	{
		for (u32 i = 0; i < num_vertices; i += 3)
		{
			in_vect[0].x = p_vertices[i + 0].Position.x;
			in_vect[0].y = p_vertices[i + 0].Position.y;
			in_vect[0].z = p_vertices[i + 0].Position.z;
			in_vect[0].w = 1.0f;

			in_vect[1].x = p_vertices[i + 1].Position.x;
			in_vect[1].y = p_vertices[i + 1].Position.y;
			in_vect[1].z = p_vertices[i + 1].Position.z;
			in_vect[1].w = 1.0f;

			in_vect[2].x = p_vertices[i + 2].Position.x;
			in_vect[2].y = p_vertices[i + 2].Position.y;
			in_vect[2].z = p_vertices[i + 2].Position.z;
			in_vect[2].w = 1.0f;

			VuxRotTrans(&in_vect[0], &out_vect[0]);
			VuxRotTrans(&in_vect[1], &out_vect[1]);
			VuxRotTrans(&in_vect[2], &out_vect[2]);

			color[0] = GS_SETREG_RGBAQ(p_vertices[i + 0].Colour.GetR(), p_vertices[i + 0].Colour.GetG(), p_vertices[i + 0].Colour.GetB(), (p_vertices[i + 0].Colour.GetA()) / 2, 1.0f);
			color[1] = GS_SETREG_RGBAQ(p_vertices[i + 1].Colour.GetR(), p_vertices[i + 1].Colour.GetG(), p_vertices[i + 1].Colour.GetB(), (p_vertices[i + 1].Colour.GetA()) / 2, 1.0f);
			color[2] = GS_SETREG_RGBAQ(p_vertices[i + 2].Colour.GetR(), p_vertices[i + 2].Colour.GetG(), p_vertices[i + 2].Colour.GetB(), (p_vertices[i + 2].Colour.GetA()) / 2, 1.0f);

			if (textured)
			{
				if (gsShading) 
				{
					_gsKit_prim_triangle_goraud_texture_3d(gsGlobal, CurrTex,
						GS_X(out_vect[0]), GS_Y(out_vect[0]), GS_Z(out_vect[0]),
						p_vertices[i + 0].Texture.x, p_vertices[i + 0].Texture.y,
						GS_X(out_vect[1]), GS_Y(out_vect[1]), GS_Z(out_vect[1]),
						p_vertices[i + 1].Texture.x, p_vertices[i + 1].Texture.y,
						GS_X(out_vect[2]), GS_Y(out_vect[2]), GS_Z(out_vect[2]),
						p_vertices[i + 2].Texture.x, p_vertices[i + 2].Texture.y,
						color[0], color[1], color[2]);
				}
				else
				{
					_gsKit_prim_triangle_texture_3d(gsGlobal, CurrTex,
						GS_X(out_vect[0]), GS_Y(out_vect[0]), GS_Z(out_vect[0]),
						p_vertices[i + 0].Texture.x, p_vertices[i + 0].Texture.y,
						GS_X(out_vect[1]), GS_Y(out_vect[1]), GS_Z(out_vect[1]),
						p_vertices[i + 1].Texture.x, p_vertices[i + 1].Texture.y,
						GS_X(out_vect[2]), GS_Y(out_vect[2]), GS_Z(out_vect[2]),
						p_vertices[i + 2].Texture.x, p_vertices[i + 2].Texture.y,
						color[0]);
				}

			}
			else
			{
				if (gsShading) 
				{
					gsKit_prim_triangle_gouraud_3d(gsGlobal,
						GS_X(out_vect[0]), GS_Y(out_vect[0]), GS_Z(out_vect[0]),
						GS_X(out_vect[1]), GS_Y(out_vect[1]), GS_Z(out_vect[1]),
						GS_X(out_vect[2]), GS_Y(out_vect[2]), GS_Z(out_vect[2]),
						color[0],
						color[1],
						color[2]);
				}
				else
				{
					gsKit_prim_triangle_3d(gsGlobal,
						GS_X(out_vect[0]), GS_Y(out_vect[0]), GS_Z(out_vect[0]),
						GS_X(out_vect[1]), GS_Y(out_vect[1]), GS_Z(out_vect[1]),
						GS_X(out_vect[2]), GS_Y(out_vect[2]), GS_Z(out_vect[2]),
						color[0]);
				}
			}
		}
	}
	else if (prim_type == GS_PRIM_PRIM_TRISTRIP)
	{
		float* tris = (float*)memalign(128, sizeof(float) * num_vertices * 5);
		
		for (u32 i = 0; i < num_vertices; i++)
		{
			in_vect[0].x = p_vertices[i].Position.x;
			in_vect[0].y = p_vertices[i].Position.y;
			in_vect[0].z = p_vertices[i].Position.z;
			in_vect[0].w = 1.0f;

			VuxRotTrans(&in_vect[0], &out_vect[0]);

			tris[i * 5 + 0] = GS_X(out_vect[0]);
			tris[i * 5 + 1] = GS_Y(out_vect[0]);
			tris[i * 5 + 2] = GS_Z(out_vect[0]);
			tris[i * 5 + 3] = p_vertices[i].Texture.x;
			tris[i * 5 + 4] = p_vertices[i].Texture.y;
		}

		gsKit_prim_triangle_strip_texture_3d(gsGlobal, CurrTex,
			tris, num_vertices, GS_SETREG_RGBAQ(p_vertices[0].Colour.GetR(), p_vertices[0].Colour.GetG(), p_vertices[0].Colour.GetB(), (p_vertices[0].Colour.GetA() ) / 2, 0x00));

		free(tris);
	}
}

RendererPS2::RendererPS2()
{
	//
//	Set up RGB = T0, A = T0
//
	mCopyBlendStates = new CBlendStates;
	{
		CAlphaRenderSettings* alpha_settings(new CAlphaRenderSettings("Copy"));
		CRenderSettingsModulate* colour_settings(new CRenderSettingsModulate("Copy"));

		alpha_settings->AddTermTexel0();
		colour_settings->AddTermTexel0();

		mCopyBlendStates->SetAlphaSettings(alpha_settings);
		mCopyBlendStates->AddColourSettings(colour_settings);
	}

	//
	//	Set up RGB = Diffuse, A = Diffuse
	//
	mFillBlendStates = new CBlendStates;
	{
		CAlphaRenderSettings* alpha_settings(new CAlphaRenderSettings("Fill"));
		CRenderSettingsModulate* colour_settings(new CRenderSettingsModulate("Fill"));

		alpha_settings->AddTermConstant(new CBlendConstantExpressionValue(BC_SHADE));
		colour_settings->AddTermConstant(new CBlendConstantExpressionValue(BC_SHADE));

		mFillBlendStates->SetAlphaSettings(alpha_settings);
		mFillBlendStates->AddColourSettings(colour_settings);
	}

#ifdef DAEDALUS_DEBUG_DISPLAYLIST
	memset(gWhiteTexture, 0xff, sizeof(gWhiteTexture));

	memset(&gDBlend.TexInstall, 0, sizeof(gDBlend));
	gDBlend.TexInstall = 1;

	u32	texel_idx = 0;
	const u32	COL_MAGENTA = c32::Magenta.GetColour();
	const u32	COL_GREEN = c32::Green.GetColour();
	const u32	COL_BLACK = c32::Black.GetColour();
	for (u32 y = 0; y < kPlaceholderTextureHeight; ++y)
	{
		for (u32 x = 0; x < kPlaceholderTextureWidth; ++x)
		{
			gPlaceholderTexture[texel_idx] = ((x & 1) == (y & 1)) ? COL_MAGENTA : COL_BLACK;
			gSelectedTexture[texel_idx] = ((x & 1) == (y & 1)) ? COL_GREEN : COL_BLACK;

			texel_idx++;
		}
	}
#endif
}

RendererPS2::~RendererPS2()
{
	delete mFillBlendStates;
	delete mCopyBlendStates;
}

void RendererPS2::RestoreRenderStates()
{
	//printf("RendererPS2:: %s \n", __func__);
	
	gsGlobal->PrimFogEnable = GS_SETTING_OFF;
	gsShading = 1;

	gsTexEnvColor(c32::White.GetColour());

	//gsKit_set_primalpha(gsGlobal, GS_SETREG_ALPHA(0, 1, 0, 1, 0), 0);

	gsDepthMask(1);
	gsGlobal->Test->ATST = ATST_GEQUAL;
	gsGlobal->Test->AREF = 0x04;
	//gsGlobal->Test->AREF = 0x80;
	gsGlobal->Test->AFAIL = 0;
	gsKit_set_test(gsGlobal, GS_ATEST_ON);
	
	gsKit_set_test(gsGlobal, GS_ZTEST_ON);

	//VuSetWorldMatrix((VU_MATRIX*)& gMatrixIdentity);
	//VuSetViewMatrix((VU_MATRIX*)& gMatrixIdentity);
}

RendererPS2::SBlendStateEntry RendererPS2::LookupBlendState(u64 mux, bool two_cycles)
{
#ifdef DAEDALUS_DEBUG_DISPLAYLIST
	DAEDALUS_PROFILE("RendererPSP::LookupBlendState");
	mRecordedCombinerStates.insert(mux);
#endif

	REG64 key;
	key._u64 = mux;

	// Top 8 bits are never set - use the very top one to differentiate between 1/2 cycles
	key._u32_1 |= (two_cycles << 31);

	BlendStatesMap::const_iterator	it(mBlendStatesMap.find(key._u64));
	if (it != mBlendStatesMap.end())
	{
		return it->second;
	}

	// Blendmodes with Inexact blends either get an Override blend or a Default blend (GU_TFX_MODULATE)
	// If its not an Inexact blend then we check if we need to Force a blend mode none the less// Salvy
	//
	SBlendStateEntry entry;
	CCombinerTree tree(mux, two_cycles);
	entry.States = tree.GetBlendStates();

	if (entry.States->IsInexact())
	{
		entry.OverrideFunction = LookupOverrideBlendModeInexact(mux);
	}
	else
	{
		// This is for non-inexact blends, errg hacks and such to be more precise
		entry.OverrideFunction = LookupOverrideBlendModeForced(mux);
	}

#ifdef DAEDALUS_DEBUG_DISPLAYLIST
	printf("Adding %08x%08x - %d cycles - %s\n", u32(mux >> 32), u32(mux), two_cycles ? 2 : 1, entry.States->IsInexact() ? IsCombinerStateDefault(mux) ? "Inexact(Default)" : "Inexact(Override)" : entry.OverrideFunction == nullptr ? "Auto" : "Forced");
#endif

	//Add blend mode to the Blend States Map
	mBlendStatesMap[key._u64] = entry;

	return entry;
}

#ifdef DAEDALUS_DEBUG_DISPLAYLIST
void RendererPS2::ResetDebugState()
{

}
#endif

static FILE* fd;

void RendererPS2::RenderTriangles(DaedalusVtx* p_vertices, u32 num_vertices, bool disable_zbuffer)
{
	char buf[1024 * 2];
	//if (!fd)
		//fd = fopen("dumpvtx.txt", "a+t");

	if (fd)
	{
		for (int i = 0; i < num_vertices; i += 3)
		{
			sprintf(buf, "   x %f y %f z %f u %f v %f r %02x g %02x b %02x a %02x    x %f y %f z %f u %f v %f r %02x g %02x b %02x a %02x    x %f y %f z %f u %f v %f r %02x g %02x b %02x a %02x \n", p_vertices[i + 0].Position.x, p_vertices[i + 0].Position.y, p_vertices[i + 0].Position.z,
				p_vertices[i + 0].Texture.x, p_vertices[i + 0].Texture.y,
				p_vertices[i + 0].Colour.GetR(), p_vertices[i + 0].Colour.GetG(), p_vertices[i + 0].Colour.GetB(), p_vertices[i + 0].Colour.GetA(),
			    p_vertices[i + 1].Position.x, p_vertices[i + 1].Position.y, p_vertices[i + 1].Position.z,
				p_vertices[i + 1].Texture.x, p_vertices[i + 1].Texture.y,
				p_vertices[i + 1].Colour.GetR(), p_vertices[i + 1].Colour.GetG(), p_vertices[i + 1].Colour.GetB(), p_vertices[i + 1].Colour.GetA(),
				p_vertices[i + 2].Position.x, p_vertices[i + 2].Position.y, p_vertices[i + 2].Position.z,
				p_vertices[i + 2].Texture.x, p_vertices[i + 2].Texture.y,
				p_vertices[i + 2].Colour.GetR(), p_vertices[i + 2].Colour.GetG(), p_vertices[i + 2].Colour.GetB(), p_vertices[i + 2].Colour.GetA());

			fwrite(buf, 1, strlen(buf), fd);
		}

		//fclose(fd);
	}
	if (mTnL.Flags.Texture)
	{
		UpdateTileSnapshots(mTextureTile);

		const CNativeTexture* texture = mBoundTexture[0];

		if (texture && (mTnL.Flags._u32 & (TNL_LIGHT | TNL_TEXGEN)) != (TNL_LIGHT | TNL_TEXGEN))
		{
			//float scale_x = texture->GetScaleX();
			//float scale_y = texture->GetScaleY();

			// Hack to fix the sun in Zelda OOT/MM
			if (g_ROM.ZELDA_HACK && (gRDPOtherMode.L == 0x0c184241))	 //&& ti.GetFormat() == G_IM_FMT_I && (ti.GetWidth() == 64)
			{
				//scale_x *= 0.5f;
				//scale_y *= 0.5f;
			}
			//sceGuTexOffset(-mTileTopLeft[0].s * scale_x / 4.f, -mTileTopLeft[0].t * scale_y / 4.f);
			//sceGuTexScale(scale_x, scale_y);
		}
		else
		{
			//sceGuTexOffset(0.0f, 0.0f);
			//sceGuTexScale(1.0f, 1.0f);
		}
	}

	RenderUsingCurrentBlendMode(p_vertices, num_vertices, GS_PRIM_PRIM_TRIANGLE, 0, disable_zbuffer);
	//DrawPrims(p_vertices, num_vertices, GS_PRIM_PRIM_TRIANGLE, false);
}

inline void RendererPS2::RenderFog( DaedalusVtx * p_vertices, u32 num_vertices, u32 triangle_mode, u32 render_flags )
{
	//This will render a second pass on triangles that are fog enabled to blend in the fog color as a function of depth(alpha) //Corn
	//
	//if( gRDPOtherMode.c1_m1a==3 || gRDPOtherMode.c1_m2a==3 || gRDPOtherMode.c2_m1a==3 || gRDPOtherMode.c2_m2a==3 )
	{
		//sceGuShadeModel(GU_SMOOTH);

		//sceGuDepthFunc(GU_EQUAL);	//Make sure to only blend on pixels that has been rendered on first pass //Corn
		//sceGuDepthMask(GL_TRUE);	//GL_TRUE to disable z-writes, no need to write to zbuffer for second pass //Corn
		//sceGuEnable(GU_BLEND);
		//sceGuDisable(GU_TEXTURE_2D);	//Blend triangle without a texture
		//sceGuDisable(GU_ALPHA_TEST);
		//sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);

		gsDepthMask(1);
		gsKit_set_test(gsGlobal, GS_ZTEST_ON);
		gsGlobal->PrimAlphaEnable = GS_SETTING_ON;
		gsKit_set_test(gsGlobal, GS_ATEST_OFF);
		gsKit_set_primalpha(gsGlobal, GS_SETREG_ALPHA(0, 1, 0, 1, 0), 0);

		u32 FogColor = mFogColour.GetColour();

		//Copy fog color to vertices
		for (u32 i = 0; i < num_vertices; i++)
		{
			u32 alpha = p_vertices[i].Colour.GetColour() & 0xFF000000;
			p_vertices[i].Colour = (c32)(alpha | FogColor);
		}

		//sceGuDrawArray(triangle_mode, render_flags, num_vertices, nullptr, p_vertices);
		DrawPrims(p_vertices, num_vertices, triangle_mode, false);

		//sceGuDepthFunc(GU_GEQUAL);	//Restore default depth function
	}
}

void RendererPS2::RenderUsingCurrentBlendMode( DaedalusVtx * p_vertices, u32 num_vertices, u32 triangle_mode, u32 render_mode, bool disable_zbuffer )
{
	static bool	ZFightingEnabled = false;

	//DAEDALUS_PROFILE("RendererPS2::RenderUsingCurrentBlendMode");

	if (disable_zbuffer)
	{
		//sceGuDisable(GU_DEPTH_TEST);
		//sceGuDepthMask(GL_TRUE);	// GL_TRUE to disable z-writes

		//GsEnableZbuffer1(GS_ENABLE, GS_ZBUFF_ALWAYS);
		//GsEnableZbuffer2(GS_ENABLE, GS_ZBUFF_ALWAYS);

		gsDepthMask(1);
		gsKit_set_test(gsGlobal, GS_ZTEST_OFF);
	}
	else
	{
		// Fixes Zfighting issues we have on the PSP.
		if (gRDPOtherMode.zmode == 3)
		{
			if (!ZFightingEnabled)
			{
				ZFightingEnabled = true;
				//sceGuDepthRange(65535, 80);
				gsDepthRange(65535, 80);
			}
		}
		else if (ZFightingEnabled)
		{
			ZFightingEnabled = false;
			//sceGuDepthRange(65535, 0);
			gsDepthRange(65535, 0);
		}

		// Enable or Disable ZBuffer test
		if ((mTnL.Flags.Zbuffer & gRDPOtherMode.z_cmp) | gRDPOtherMode.z_upd)
		{
			//sceGuEnable(GU_DEPTH_TEST);
			gsKit_set_test(gsGlobal, GS_ZTEST_ON);
		}
		else
		{
			//sceGuDisable(GU_DEPTH_TEST);
			gsKit_set_test(gsGlobal, GS_ZTEST_OFF);
		}

		// GL_TRUE to disable z-writes
		//sceGuDepthMask(gRDPOtherMode.z_upd ? GL_FALSE : GL_TRUE);
		gsDepthMask(gRDPOtherMode.z_upd ? 0 : 1);
	}

	// Initiate Texture Filter
	//
	// G_TF_AVERAGE : 1, G_TF_BILERP : 2 (linear)
	// G_TF_POINT   : 0 (nearest)
	//
	if ((gRDPOtherMode.text_filt != G_TF_POINT) | (gGlobalPreferences.ForceLinearFilter))
	{
		//sceGuTexFilter(GU_LINEAR, GU_LINEAR);
		if (CurrTex)
			CurrTex->Filter = GS_FILTER_LINEAR;
	}
	else
	{
		//sceGuTexFilter(GU_NEAREST, GU_NEAREST);
		if (CurrTex)
			CurrTex->Filter = GS_FILTER_NEAREST;
	}

	u32 cycle_mode = gRDPOtherMode.cycle_type;

	// Initiate Blender
	//
	if (cycle_mode < CYCLE_COPY && gRDPOtherMode.force_bl)
	{
		InitBlenderMode(gRDPOtherMode.blender);
	}
	else
	{
		//sceGuDisable(GU_BLEND);
	}

	// Initiate Alpha test
	//
	if ((gRDPOtherMode.alpha_compare == G_AC_THRESHOLD) && !gRDPOtherMode.alpha_cvg_sel)
	{
		u8 alpha_threshold = (mBlendColour.GetA());
		//sceGuAlphaFunc((alpha_threshold | g_ROM.ALPHA_HACK) ? GU_GEQUAL : GU_GREATER, alpha_threshold, 0xff);
		//sceGuEnable(GU_ALPHA_TEST);

		gsGlobal->Test->ATST = (alpha_threshold | g_ROM.ALPHA_HACK) ? ATST_GEQUAL : ATST_GREATER;
		gsGlobal->Test->AREF = alpha_threshold/2;
		gsGlobal->Test->AFAIL = 0;
		gsKit_set_test(gsGlobal, GS_ATEST_ON);

		//printf("alpha 1 %x\n", alpha_threshold);
	}
	else if (gRDPOtherMode.cvg_x_alpha)
	{
		// Going over 0x70 breaks OOT, but going lesser than that makes lines on games visible...ex: Paper Mario.
		// Also going over 0x30 breaks the birds in Tarzan :(. Need to find a better way to leverage this.
		//sceGuAlphaFunc(GU_GREATER, 0x70, 0xff);
		//sceGuEnable(GU_ALPHA_TEST);

		gsGlobal->Test->ATST = ATST_GREATER;
		gsGlobal->Test->AREF = 0x70/2;
		gsGlobal->Test->AFAIL = 0;
		gsKit_set_test(gsGlobal, GS_ATEST_ON);

		//printf("alpha 2\n");
	}
	else
	{
		//sceGuDisable(GU_ALPHA_TEST);

		gsKit_set_test(gsGlobal, GS_ATEST_OFF);
	}

	SBlendStateEntry		blend_entry;

	switch (cycle_mode)
	{
		case CYCLE_COPY:		blend_entry.States = mCopyBlendStates; break;
		case CYCLE_FILL:		blend_entry.States = mFillBlendStates; break;
		case CYCLE_1CYCLE:		blend_entry = LookupBlendState(mMux, false); break;
		case CYCLE_2CYCLE:		blend_entry = LookupBlendState(mMux, true); break;
	}

	//u32 render_flags{ GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | render_mode };

#ifdef DAEDALUS_DEBUG_DISPLAYLIST
	// Used for Blend Explorer, or Nasty texture
	//
	if (DebugBlendmode(p_vertices, num_vertices, triangle_mode, render_flags, mMux))
		return;
#endif

	// This check is for inexact blends which were handled either by a custom blendmode or auto blendmode thing
	//
	if (blend_entry.OverrideFunction != nullptr)
	{
#ifdef DAEDALUS_DEBUG_DISPLAYLIST
		// Used for dumping mux and highlight inexact blend
		//
		DebugMux(blend_entry.States, p_vertices, num_vertices, triangle_mode, render_flags, mMux);
#endif

		// Local vars for now
		SBlendModeDetails details;

		details.EnvColour = mEnvColour;
		details.PrimColour = mPrimitiveColour;
		details.InstallTexture = true;
		details.ColourAdjuster.Reset();

		blend_entry.OverrideFunction(details);

		bool installed_texture = false;

		if (details.InstallTexture)
		{
			u32 texture_idx = g_ROM.T1_HACK ? 1 : 0;

			if (mBoundTexture[texture_idx])
			{
				mBoundTexture[texture_idx]->InstallTexture();

				gsTexWrap(mTexWrap[texture_idx].u, mTexWrap[texture_idx].v);

				installed_texture = true;
			}
		}

		// If no texture was specified, or if we couldn't load it, clear it out
		if (!installed_texture)
		{
			//sceGuDisable(GU_TEXTURE_2D);
		}

		if (mTnL.Flags.Fog)
		{
			DaedalusVtx* p_FogVtx = static_cast<DaedalusVtx*>(malloc(num_vertices * sizeof(DaedalusVtx)));
			memcpy(p_FogVtx, p_vertices, num_vertices * sizeof(DaedalusVtx));
			details.ColourAdjuster.Process(p_vertices, num_vertices);
			//sceGuDrawArray(triangle_mode, render_flags, num_vertices, nullptr, p_vertices);
			DrawPrims(p_vertices, num_vertices, triangle_mode, installed_texture);
			RenderFog(p_FogVtx, num_vertices, triangle_mode, 0);

			free(p_FogVtx);
		}
		else
		{
			details.ColourAdjuster.Process(p_vertices, num_vertices);
			//sceGuDrawArray(triangle_mode, render_flags, num_vertices, nullptr, p_vertices);
			DrawPrims(p_vertices, num_vertices, triangle_mode, installed_texture);
		}
	}
	else if (blend_entry.States != nullptr)
	{
		RenderUsingRenderSettings(blend_entry.States, p_vertices, num_vertices, triangle_mode, 0);
	}
	else
	{
#ifdef DAEDALUS_DEBUG_CONSOLE
		// Set default states
		DAEDALUS_ERROR("Unhandled blend mode");
#endif
		//sceGuDisable(GU_TEXTURE_2D);
		//sceGuDrawArray(triangle_mode, render_flags, num_vertices, nullptr, p_vertices);
		DrawPrims(p_vertices, num_vertices, triangle_mode, false);
	}
}

void RendererPS2::RenderUsingRenderSettings( const CBlendStates * states, DaedalusVtx * p_vertices, u32 num_vertices, u32 triangle_mode, u32 render_flags)
{
	//DAEDALUS_PROFILE( "RendererPS2::RenderUsingRenderSettings" );

	const CAlphaRenderSettings* alpha_settings(states->GetAlphaSettings());

	SRenderState	state;

	state.Vertices = p_vertices;
	state.NumVertices = num_vertices;
	state.PrimitiveColour = mPrimitiveColour;
	state.EnvironmentColour = mEnvColour;

	//Avoid copying vertices twice if we already save a copy to render fog //Corn
	DaedalusVtx* p_FogVtx(mVtx_Save);
	if (mTnL.Flags.Fog)
	{
		p_FogVtx = static_cast<DaedalusVtx*>(malloc(num_vertices * sizeof(DaedalusVtx)));
		memcpy(p_FogVtx, p_vertices, num_vertices * sizeof(DaedalusVtx));
	}
	else if (states->GetNumStates() > 1)
	{
		memcpy(mVtx_Save, p_vertices, num_vertices * sizeof(DaedalusVtx));
	}

	for (u32 i = 0; i < states->GetNumStates(); ++i)
	{
		const CRenderSettings* settings(states->GetColourSettings(i));

		bool install_texture0(settings->UsesTexture0() || alpha_settings->UsesTexture0());
		bool install_texture1(settings->UsesTexture1() || alpha_settings->UsesTexture1());

		SRenderStateOut out;

		memset(&out, 0, sizeof(out));

		settings->Apply(install_texture0 || install_texture1, state, out);
		alpha_settings->Apply(install_texture0 || install_texture1, state, out);

		// TODO: this nobbles the existing diffuse colour on each pass. Need to use a second buffer...
		if (i > 0)
		{
			memcpy(p_vertices, p_FogVtx, num_vertices * sizeof(DaedalusVtx));
		}

		if (out.VertexExpressionRGB != nullptr)
		{
			out.VertexExpressionRGB->ApplyExpressionRGB(state);
		}
		if (out.VertexExpressionA != nullptr)
		{
			out.VertexExpressionA->ApplyExpressionAlpha(state);
		}

		bool installed_texture = false;

		u32 texture_idx = 0;

		if (install_texture0 || install_texture1)
		{
			u32	tfx = GS_TFX_MODULATE;
			switch (out.BlendMode)
			{
			case PBM_MODULATE:		tfx = GS_TFX_MODULATE; break;
			case PBM_REPLACE:		tfx = GS_TFX_REPLACE; break;
			case PBM_BLEND:			tfx = GS_TFX_BLEND; gsTexEnvColor(out.TextureFactor.GetColour()); break;
			}

			gsTexFunc(tfx, out.BlendAlphaMode == PBAM_RGB ? GS_TCC_RGB : GS_TCC_RGBA);

			if (g_ROM.T1_HACK)
			{
				// NB if install_texture0 and install_texture1 are both set, 1 wins out
				texture_idx = install_texture1;

				const CNativeTexture* texture1 = mBoundTexture[1];

				if (install_texture1 && texture1 && mTnL.Flags.Texture && (mTnL.Flags._u32 & (TNL_LIGHT | TNL_TEXGEN)) != (TNL_LIGHT | TNL_TEXGEN))
				{
					/*float scale_x = texture1->GetScaleX();
					float scale_y = texture1->GetScaleY();

					sceGuTexOffset(-mTileTopLeft[1].s * scale_x / 4.f, -mTileTopLeft[1].t * scale_y / 4.f);
					sceGuTexScale(scale_x, scale_y);*/
				}
			}
			else
			{
				// NB if install_texture0 and install_texture1 are both set, 0 wins out
				texture_idx = install_texture0 ? 0 : 1;
			}

			CRefPtr<CNativeTexture> texture;

			if (out.MakeTextureWhite)
			{
				TextureInfo white_ti = mBoundTextureInfo[texture_idx];
				white_ti.SetWhite(true);
				texture = CTextureCache::Get()->GetOrCreateTexture(white_ti);
			}
			else
			{
				texture = mBoundTexture[texture_idx];
			}

			if (texture != nullptr)
			{
				texture->InstallTexture();
				installed_texture = true;
			}
		}

		// If no texture was specified, or if we couldn't load it, clear it out
		//if (!installed_texture) sceGuDisable(GU_TEXTURE_2D);

		gsTexWrap(mTexWrap[texture_idx].u, mTexWrap[texture_idx].v);

		//sceGuDrawArray(triangle_mode, render_flags, num_vertices, nullptr, p_vertices);
		DrawPrims(p_vertices, num_vertices, triangle_mode, installed_texture);

		if (mTnL.Flags.Fog)
		{
			RenderFog(p_FogVtx, num_vertices, triangle_mode, render_flags);
		}
		
		/*if(p_FogVt)
			free(p_FogVt);*/
	}
}

void RendererPS2::TexRect( u32 tile_idx, const v2 & xy0, const v2 & xy1, TexCoord st0, TexCoord st1 )
{
	mTnL.Flags.Fog = 0;	//For now we force fog off for textrect, normally it should be fogged when depth_source is set //Corn

	UpdateTileSnapshots(tile_idx);

	// NB: we have to do this after UpdateTileSnapshot, as it set up mTileTopLeft etc.
	PrepareTexRectUVs(&st0, &st1);

	//printf("st: %d %d %d %d\n", st0.s, st0.t, st1.s, st1.t);

	// Convert fixed point uvs back to floating point format.
	// NB: would be nice to pass these as s16 ints, and use GU_TEXTURE_16BIT
	v2 uv0((float)st0.s / 32.f, (float)st0.t / 32.f);
	v2 uv1((float)st1.s / 32.f, (float)st1.t / 32.f);

	v2 screen0;
	v2 screen1;
	if (gGlobalPreferences.ViewportType == VT_FULLSCREEN_HD)
	{
		screen0.x = roundf(roundf(HD_SCALE * xy0.x) * mN64ToScreenScale.x + 59);	//59 in translate is an ugly hack that only work on 480x272 display//Corn
		screen0.y = roundf(roundf(xy0.y) * mN64ToScreenScale.y + mN64ToScreenTranslate.y);

		screen1.x = roundf(roundf(HD_SCALE * xy1.x) * mN64ToScreenScale.x + 59); //59 in translate is an ugly hack that only work on 480x272 display//Corn
		screen1.y = roundf(roundf(xy1.y) * mN64ToScreenScale.y + mN64ToScreenTranslate.y);
	}
	else
	{
		ConvertN64ToScreen(xy0, screen0);
		ConvertN64ToScreen(xy1, screen1);
	}

#ifdef DAEDALUS_DEBUG_DISPLAYLIST
	DL_PF("    Screen:  %.1f,%.1f -> %.1f,%.1f", screen0.x, screen0.y, screen1.x, screen1.y);
	DL_PF("    Texture: %.1f,%.1f -> %.1f,%.1f", uv0.x, uv0.y, uv1.x, uv1.y);
#endif
	const f32 depth = gRDPOtherMode.depth_source ? mPrimDepth : 0.0f;

#if 1	//1->SPRITE, 0->STRIP
	DaedalusVtx * p_vertices = static_cast<DaedalusVtx*>(malloc(2 * sizeof(DaedalusVtx)));

	p_vertices[0].Position.x = screen0.x;
	p_vertices[0].Position.y = screen0.y;
	p_vertices[0].Position.z = depth;
	p_vertices[0].Colour = c32(0xffffffff);
	p_vertices[0].Texture.x = uv0.x;
	p_vertices[0].Texture.y = uv0.y;

	p_vertices[1].Position.x = screen1.x;
	p_vertices[1].Position.y = screen1.y;
	p_vertices[1].Position.z = depth;
	p_vertices[1].Colour = c32(0xffffffff);
	p_vertices[1].Texture.x = uv1.x;
	p_vertices[1].Texture.y = uv1.y;

	RenderUsingCurrentBlendMode(p_vertices, 2, GS_PRIM_PRIM_SPRITE, 0 /*GU_TRANSFORM_2D*/, gRDPOtherMode.depth_source ? false : true);

#else
	//	To be used with TRIANGLE_STRIP, which requires 40% less verts than TRIANGLE
	//	For reference for future ports and if SPRITES( which uses %60 less verts than TRIANGLE) causes issues
	DaedalusVtx* p_vertices = static_cast<DaedalusVtx*>(sceGuGetMemory(4 * sizeof(DaedalusVtx)));

	p_vertices[0].Position.x = screen0.x;
	p_vertices[0].Position.y = screen0.y;
	p_vertices[0].Position.z = depth;
	p_vertices[0].Colour = c32(0xffffffff);
	p_vertices[0].Texture.x = uv0.x;
	p_vertices[0].Texture.y = uv0.y;

	p_vertices[1].Position.x = screen1.x;
	p_vertices[1].Position.y = screen0.y;
	p_vertices[1].Position.z = depth;
	p_vertices[1].Colour = c32(0xffffffff);
	p_vertices[1].Texture.x = uv1.x;
	p_vertices[1].Texture.y = uv0.y;

	p_vertices[2].Position.x = screen0.x;
	p_vertices[2].Position.y = screen1.y;
	p_vertices[2].Position.z = depth;
	p_vertices[2].Colour = c32(0xffffffff);
	p_vertices[2].Texture.x = uv0.x;
	p_vertices[2].Texture.y = uv1.y;

	p_vertices[3].Position.x = screen1.x;
	p_vertices[3].Position.y = screen1.y;
	p_vertices[3].Position.z = depth;
	p_vertices[3].Colour = c32(0xffffffff);
	p_vertices[3].Texture.x = uv1.x;
	p_vertices[3].Texture.y = uv1.y;

	RenderUsingCurrentBlendMode(p_vertices, 4, GS_PRIM_TRI_STRIP, GU_TRANSFORM_2D, gRDPOtherMode.depth_source ? false : true);
#endif

	free(p_vertices);

#ifdef DAEDALUS_DEBUG_DISPLAYLIST
	++mNumRect;
#endif
}

void RendererPS2::TexRectFlip( u32 tile_idx, const v2 & xy0, const v2 & xy1, TexCoord st0, TexCoord st1 )
{
	mTnL.Flags.Fog = 0;	//For now we force fog off for textrect, normally it should be fogged when depth_source is set //Corn

	UpdateTileSnapshots(tile_idx);

	// NB: we have to do this after UpdateTileSnapshot, as it set up mTileTopLeft etc.
	PrepareTexRectUVs(&st0, &st1);

	// Convert fixed point uvs back to floating point format.
	// NB: would be nice to pass these as s16 ints, and use GU_TEXTURE_16BIT
	v2 uv0((float)st0.s / 32.f, (float)st0.t / 32.f);
	v2 uv1((float)st1.s / 32.f, (float)st1.t / 32.f);

	v2 screen0;
	v2 screen1;
	// FIXME(strmnnrmn): why is VT_FULLSCREEN_HD code in TexRect() not also done here?
	ConvertN64ToScreen(xy0, screen0);
	ConvertN64ToScreen(xy1, screen1);

#ifdef DAEDALUS_DEBUG_DISPLAYLIST
	DL_PF("    Screen:  %.1f,%.1f -> %.1f,%.1f", screen0.x, screen0.y, screen1.x, screen1.y);
	DL_PF("    Texture: %.1f,%.1f -> %.1f,%.1f", uv0.x, uv0.y, uv1.x, uv1.y);
#endif
	DaedalusVtx* p_vertices = static_cast<DaedalusVtx*>(malloc(4 * sizeof(DaedalusVtx)));

	p_vertices[0].Position.x = screen0.x;
	p_vertices[0].Position.y = screen0.y;
	p_vertices[0].Position.z = 0.0f;
	p_vertices[0].Colour = c32(0xffffffff);
	p_vertices[0].Texture.x = uv0.x;
	p_vertices[0].Texture.y = uv0.y;

	p_vertices[1].Position.x = screen1.x;
	p_vertices[1].Position.y = screen0.y;
	p_vertices[1].Position.z = 0.0f;
	p_vertices[1].Colour = c32(0xffffffff);
	p_vertices[1].Texture.x = uv0.x;
	p_vertices[1].Texture.y = uv1.y;

	p_vertices[2].Position.x = screen0.x;
	p_vertices[2].Position.y = screen1.y;
	p_vertices[2].Position.z = 0.0f;
	p_vertices[2].Colour = c32(0xffffffff);
	p_vertices[2].Texture.x = uv1.x;
	p_vertices[2].Texture.y = uv0.y;

	p_vertices[3].Position.x = screen1.x;
	p_vertices[3].Position.y = screen1.y;
	p_vertices[3].Position.z = 0.0f;
	p_vertices[3].Colour = c32(0xffffffff);
	p_vertices[3].Texture.x = uv1.x;
	p_vertices[3].Texture.y = uv1.y;

	// FIXME(strmnnrmn): shouldn't this pass gRDPOtherMode.depth_source ? false : true for the disable_zbuffer arg, as TextRect()?
	RenderUsingCurrentBlendMode(p_vertices, 4, GS_PRIM_PRIM_TRISTRIP, 0 /*GU_TRANSFORM_2D*/, true);

	free(p_vertices);

#ifdef DAEDALUS_DEBUG_DISPLAYLIST
	++mNumRect;
#endif
}

void RendererPS2::FillRect( const v2 & xy0, const v2 & xy1, u32 color )
{
	/*
		if ( (gRDPOtherMode._u64 & 0xffff0000) == 0x5f500000 )	//Used by Wave Racer
		{
			// this blend mode is mem*0 + mem*1, so we don't need to render it... Very odd!
			DAEDALUS_ERROR("	mem*0 + mem*1 - skipped");
			return;
		}
	*/
	// This if for C&C - It might break other stuff (I'm not sure if we should allow alpha or not..)
	//color |= 0xff000000;

	v2 screen0;
	v2 screen1;
	ConvertN64ToScreen(xy0, screen0);
	ConvertN64ToScreen(xy1, screen1);

#ifdef DAEDALUS_DEBUG_DISPLAYLIST
	DL_PF("    Screen:  %.1f,%.1f -> %.1f,%.1f", screen0.x, screen0.y, screen1.x, screen1.y);
#endif
	DaedalusVtx* p_vertices = static_cast<DaedalusVtx*>(malloc(2 * sizeof(DaedalusVtx)));

	// No need for Texture.x/y as we don't do any texturing for fillrect
	p_vertices[0].Position.x = screen0.x;
	p_vertices[0].Position.y = screen0.y;
	p_vertices[0].Position.z = 0.0f;
	p_vertices[0].Colour = c32(color);
	//p_vertices[0].Texture.x = 0.0f;
	//p_vertices[0].Texture.y = 0.0f;

	p_vertices[1].Position.x = screen1.x;
	p_vertices[1].Position.y = screen1.y;
	p_vertices[1].Position.z = 0.0f;
	p_vertices[1].Colour = c32(color);
	//p_vertices[1].Texture.x = 1.0f;
	//p_vertices[1].Texture.y = 0.0f;

	// FIXME(strmnnrmn): shouldn't this pass gRDPOtherMode.depth_source ? false : true for the disable_zbuffer arg, as TexRect()?
	RenderUsingCurrentBlendMode(p_vertices, 2, GS_PRIM_PRIM_SPRITE, 0 /*GU_TRANSFORM_2D*/, true);

	free(p_vertices);

#ifdef DAEDALUS_DEBUG_DISPLAYLIST
	++mNumRect;
#endif
}

void RendererPS2::Draw2DTexture(f32 x0, f32 y0, f32 x1, f32 y1,
								f32 u0, f32 v0, f32 u1, f32 v1,
								const CNativeTexture * texture)
{
	DAEDALUS_PROFILE("RendererPSP::Draw2DTexture");
	DaedalusVtx* p_verts = (DaedalusVtx*)malloc(4 * sizeof(DaedalusVtx));

	// Enable or Disable ZBuffer test
	if ((mTnL.Flags.Zbuffer & gRDPOtherMode.z_cmp) | gRDPOtherMode.z_upd)
	{
		//sceGuEnable(GU_DEPTH_TEST);
		gsKit_set_test(gsGlobal, GS_ZTEST_ON);
	}
	else
	{
		//sceGuDisable(GU_DEPTH_TEST);
		gsKit_set_test(gsGlobal, GS_ZTEST_OFF);
	}

	// GL_TRUE to disable z-writes
	//sceGuDepthMask(gRDPOtherMode.z_upd ? GL_FALSE : GL_TRUE);
	gsDepthMask(gRDPOtherMode.z_upd ? 0 : 1);
	//sceGuShadeModel(GU_FLAT);

	//ToDO: Set alpha/blend states according RenderUsingCurrentBlendMode?
	/*sceGuTexFilter(GU_LINEAR, GU_LINEAR);
	sceGuDisable(GU_ALPHA_TEST);
	sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);

	sceGuEnable(GU_BLEND);
	sceGuTexWrap(GU_CLAMP, GU_CLAMP);*/

	if (CurrTex)
		CurrTex->Filter = GS_FILTER_LINEAR;
	gsKit_set_test(gsGlobal, GS_ATEST_OFF);
	gsTexFunc(GS_TFX_REPLACE, GS_TCC_RGBA);
	gsTexWrap(GU_CLAMP, GU_CLAMP);

	// Handle large images (width > 512) with blitting, since the PSP HW can't handle
	// Handling height > 512 doesn't work well? Ignore for now.
	/*if (u1 >= 512)
	{

		Draw2DTextureBlit(x0, y0, x1, y1, u0, v0, u1, v1, texture);
		return;
	}*/

	p_verts[0].Position.x = N64ToScreenX(x0);
	p_verts[0].Position.y = N64ToScreenY(y0);
	p_verts[0].Position.z = 0.0f;
	p_verts[0].Colour = c32(0xffffffff);
	p_verts[0].Texture.x = u0;
	p_verts[0].Texture.y = v0;

	p_verts[1].Position.x = N64ToScreenX(x1);
	p_verts[1].Position.y = N64ToScreenY(y0);
	p_verts[1].Position.z = 0.0f;
	p_verts[1].Colour = c32(0xffffffff);
	p_verts[1].Texture.x = u1;
	p_verts[1].Texture.y = v0;

	p_verts[2].Position.x = N64ToScreenX(x0);
	p_verts[2].Position.y = N64ToScreenY(y1);
	p_verts[2].Position.z = 0.0f;
	p_verts[2].Colour = c32(0xffffffff);
	p_verts[2].Texture.x = u0;
	p_verts[2].Texture.y = v1;

	p_verts[3].Position.x = N64ToScreenX(x1);
	p_verts[3].Position.y = N64ToScreenY(y1);
	p_verts[3].Position.z = 0.0f;
	p_verts[3].Colour = c32(0xffffffff);
	p_verts[3].Texture.x = u1;
	p_verts[3].Texture.y = v1;

	//sceGuDrawArray(GU_TRIANGLE_STRIP, GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_2D, 4, 0, p_verts);

	DrawPrims(p_verts, 4, GS_PRIM_PRIM_TRISTRIP, true);

	free(p_verts);
}

void RendererPS2::Draw2DTextureR(f32 x0, f32 y0, f32 x1, f32 y1,
								 f32 x2, f32 y2, f32 x3, f32 y3,
								 f32 s, f32 t)	// With Rotation
{
	//DAEDALUS_PROFILE("RendererPSP::Draw2DTextureR");

	printf("RendererPS2::Draw2DTextureR");

	DaedalusVtx* p_verts = (DaedalusVtx*)malloc(4 * sizeof(DaedalusVtx));

	// Enable or Disable ZBuffer test
	if ((mTnL.Flags.Zbuffer & gRDPOtherMode.z_cmp) | gRDPOtherMode.z_upd)
	{
		//sceGuEnable(GU_DEPTH_TEST);
		gsKit_set_test(gsGlobal, GS_ZTEST_ON);
	}
	else
	{
		//sceGuDisable(GU_DEPTH_TEST);
		gsKit_set_test(gsGlobal, GS_ZTEST_OFF);
	}

	// GL_TRUE to disable z-writes
	/*sceGuDepthMask(gRDPOtherMode.z_upd ? GL_FALSE : GL_TRUE);*/
	gsDepthMask(gRDPOtherMode.z_upd ? 0 : 1);
	//sceGuShadeModel(GU_FLAT);

	//ToDO: Set alpha/blend states according RenderUsingCurrentBlendMode?
	/*sceGuTexFilter(GU_LINEAR, GU_LINEAR)
	sceGuDisable(GU_ALPHA_TEST);
	sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);

	sceGuEnable(GU_BLEND);*/

	if (CurrTex)
		CurrTex->Filter = GS_FILTER_LINEAR;
	gsKit_set_test(gsGlobal, GS_ATEST_OFF);
	gsTexFunc(GS_TFX_REPLACE, GS_TCC_RGBA);
	gsTexWrap(GU_CLAMP, GU_CLAMP);

	p_verts[0].Position.x = N64ToScreenX(x0);
	p_verts[0].Position.y = N64ToScreenY(y0);
	p_verts[0].Position.z = 0.0f;
	p_verts[0].Colour = c32(0xffffffff);
	p_verts[0].Texture.x = 0.0f;
	p_verts[0].Texture.y = 0.0f;

	p_verts[1].Position.x = N64ToScreenX(x1);
	p_verts[1].Position.y = N64ToScreenY(y1);
	p_verts[1].Position.z = 0.0f;
	p_verts[1].Colour = c32(0xffffffff);
	p_verts[1].Texture.x = s;
	p_verts[1].Texture.y = 0.0f;

	p_verts[2].Position.x = N64ToScreenX(x2);
	p_verts[2].Position.y = N64ToScreenY(y2);
	p_verts[2].Position.z = 0.0f;
	p_verts[2].Colour = c32(0xffffffff);
	p_verts[2].Texture.x = s;
	p_verts[2].Texture.y = t;

	p_verts[3].Position.x = N64ToScreenX(x3);
	p_verts[3].Position.y = N64ToScreenY(y3);
	p_verts[3].Colour = c32(0xffffffff);
	p_verts[3].Position.z = 0.0f;
	p_verts[3].Texture.x = 0.0f;
	p_verts[3].Texture.y = t;

	//sceGuDrawArray(GU_TRIANGLE_FAN, GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_2D, 4, 0, p_verts);
	DrawPrims(p_verts, 4, GS_PRIM_PRIM_TRISTRIP, true);

	free(p_verts);
}

// The following blitting code was taken from The TriEngine.
// See http://www.assembla.com/code/openTRI for more information.
void RendererPS2::Draw2DTextureBlit(f32 x, f32 y, f32 width, f32 height,
									f32 u0, f32 v0, f32 u1, f32 v1,
									const CNativeTexture * texture)
{
	if (!texture)
	{
#ifdef DAEDALUS_DEBUG_CONSOLE
		DAEDALUS_ERROR("No texture in Draw2DTextureBlit");
#endif
		return;
	}

	f32 cur_v = v0;
	f32 cur_y = y;
	f32 v_end = v1;
	f32 y_end = height;
	f32 vslice = 512.f;
	f32 ystep = (height / (v1 - v0) * vslice);
	f32 vstep = (v1 - v0) > 0 ? vslice : -vslice;

	f32 x_end = width;
	f32 uslice = 64.f;
	//f32 ustep = (u1-u0)/width * xslice;
	f32 xstep = (width / (u1 - u0) * uslice);
	f32 ustep = (u1 - u0) > 0 ? uslice : -uslice;

	const u8* data = static_cast<const u8*>(texture->GetData());

	for (; cur_y < y_end; cur_y += ystep, cur_v += vstep)
	{
		f32 cur_u = u0;
		f32 cur_x = x;
		f32 u_end = u1;

		f32 poly_height = ((cur_y + ystep) > y_end) ? (y_end - cur_y) : ystep;
		f32 source_height = vstep;

		// support negative vsteps
		if ((vstep > 0) && (cur_v + vstep > v_end))
		{
			source_height = (v_end - cur_v);
		}
		else if ((vstep < 0) && (cur_v + vstep < v_end))
		{
			source_height = (cur_v - v_end);
		}

		const u8* udata = data;
		// blit maximizing the use of the texture-cache
		for (; cur_x < x_end; cur_x += xstep, cur_u += ustep)
		{
			// support large images (width > 512)
			if (cur_u > 512.f || cur_u + ustep > 512.f)
			{
				s32 off = (ustep > 0) ? ((int)cur_u & ~31) : ((int)(cur_u + ustep) & ~31);

				udata += off * GetBitsPerPixel(texture->GetFormat());
				cur_u -= off;
				u_end -= off;
				//sceGuTexImage(0, Min<u32>(512, texture->GetCorrectedWidth()), Min<u32>(512, texture->GetCorrectedHeight()), texture->GetBlockWidth(), udata);
			}
			TextureVtx* p_verts = (TextureVtx*)malloc(2 * sizeof(TextureVtx));

			//f32 poly_width = ((cur_x+xstep) > x_end) ? (x_end-cur_x) : xstep;
			f32 poly_width = xstep;
			f32 source_width = ustep;

			// support negative usteps
			if ((ustep > 0) && (cur_u + ustep > u_end))
			{
				source_width = (u_end - cur_u);
			}
			else if ((ustep < 0) && (cur_u + ustep < u_end))
			{
				source_width = (cur_u - u_end);
			}

			p_verts[0].t0.x = cur_u;
			p_verts[0].t0.y = cur_v;
			p_verts[0].pos.x = N64ToScreenX(cur_x);
			p_verts[0].pos.y = N64ToScreenY(cur_y);
			p_verts[0].pos.z = 0;

			p_verts[1].t0.x = cur_u + source_width;
			p_verts[1].t0.y = cur_v + source_height;
			p_verts[1].pos.x = N64ToScreenX(cur_x + poly_width);
			p_verts[1].pos.y = N64ToScreenY(cur_y + poly_height);
			p_verts[1].pos.z = 0;

			//sceGuDrawArray(GU_SPRITES, GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_2D, 2, 0, p_verts);

			free(p_verts);
		}
	}
}

#ifdef DAEDALUS_DEBUG_DISPLAYLIST
void RendererPS2::SelectPlaceholderTexture( EPlaceholderTextureType type )
{
	//printf("RendererPS2:: %s \n", __func__);
}
#endif // DAEDALUS_DEBUG_DISPLAYLIST

#ifdef DAEDALUS_DEBUG_DISPLAYLIST
// Used for Blend Explorer, or Nasty texture
bool RendererPS2::DebugBlendmode( DaedalusVtx * p_vertices, u32 num_vertices, u32 triangle_mode, u32 render_flags, u64 mux )
{
	//printf("RendererPS2:: %s \n", __func__);
	return false;
}
#endif // DAEDALUS_DEBUG_DISPLAYLIST


#ifdef DAEDALUS_DEBUG_DISPLAYLIST
void RendererPS2::DebugMux( const CBlendStates * states, DaedalusVtx * p_vertices, u32 num_vertices, u32 triangle_mode, u32 render_flags, u64 mux)
{
	//printf("RendererPS2:: %s \n", __func__);
}

#endif // DAEDALUS_DEBUG_DISPLAYLIST

bool CreateRenderer()
{
	#ifdef DAEDALUS_ENABLE_ASSERTS
	DAEDALUS_ASSERT_Q(gRenderer == nullptr);
	#endif
	gRendererPS2 = new RendererPS2();
	gRenderer    = gRendererPS2;
	return true;
}
void DestroyRenderer()
{
	delete gRendererPS2;
	gRendererPS2 = nullptr;
	gRenderer    = nullptr;
}
