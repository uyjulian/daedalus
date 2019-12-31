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

#include "stdafx.h"
#include "Graphics/GraphicsContext.h"

#include "Config/ConfigOptions.h"
#include "Core/ROM.h"
#include "Debug/DBGConsole.h"
#include "Debug/Dump.h"
#include "Graphics/ColourValue.h"
#include "Graphics/PngUtil.h"
#include "Utility/IO.h"
#include "Utility/Preferences.h"
#include "Utility/Profiler.h"
#include "Utility/VolatileMem.h"

#include <gsKit.h>
#include <dmaKit.h>
#include <gsInline.h>
#include <libvux.h>
#include <kernel.h>

namespace
{
#ifndef DAEDALUS_SILENT
	const char *	gScreenDumpRootPath = "ScreenShots";
#endif
	const char *	gScreenDumpDumpPathFormat = "sd%04d.png";
}

extern bool g32bitColorMode;
extern bool gTakeScreenshotSS;

int HAVE_DVE = -1; // default is no DVE Manager
int PSP_TV_CABLE = -1; // default is no cable
int PSP_TV_LACED = 0; // default is not interlaced

GSGLOBAL* gsGlobal;
GSFONTM* gsFontM;
u32 texture_vram, clut_vram;
u32 gsZMax;

void gsTexA()
{
	u64* p_data;
	u64* p_store;

	p_data = p_store = (u64*)gsKit_heap_alloc(gsGlobal, 1, 16, GIF_AD);

	*p_data++ = GIF_TAG_AD(1);
	*p_data++ = GIF_AD;

	*p_data++ = GS_SETREG_TEXA(0x00, 0, 0x80);
	*p_data++ = GS_TEXA;
}

// Implementation
class IGraphicsContext : public CGraphicsContext
{
public:
	IGraphicsContext();
	virtual ~IGraphicsContext();

	bool				Initialise();
	bool				IsInitialised() const { return mInitialised; }

	void				SwitchToChosenDisplay();
	void				SwitchToLcdDisplay();
	void				StoreSaveScreenData();

	void				ClearAllSurfaces();

	void				ClearToBlack();
	void				ClearZBuffer();
	void				ClearColBuffer(const c32 & colour);
	void				ClearColBufferAndDepth(const c32 & colour);

	void				BeginFrame();
	void				EndFrame();
	void				UpdateFrame( bool wait_for_vbl );
	void				GetScreenSize( u32 * width, u32 * height ) const;

	void				SetDebugScreenTarget( ETargetSurface buffer );

	void				ViewportType( u32 * d_width, u32 * d_height ) const;
	void				DumpScreenShot();
	void				DumpNextScreen()			{ mDumpNextScreen = 2; }

private:
	void				SaveScreenshot( const char* filename, s32 x, s32 y, u32 width, u32 height );

private:
	bool				mInitialised;

	void *				mpBuffers[2];
	void *				mpCurrentBackBuffer;

	void *				save_disp_rel;
	void *				save_draw_rel;
	void *				save_depth_rel;

	u32					mDumpNextScreen;
};

//*************************************************************************************
//
//*************************************************************************************
template<> bool CSingleton< CGraphicsContext >::Create()
{
	#ifdef DAEDALUS_ENABLE_ASSERTS
	DAEDALUS_ASSERT_Q(mpInstance == nullptr);
#endif
	mpInstance = new IGraphicsContext();
	return mpInstance->Initialise();
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

IGraphicsContext::IGraphicsContext()
:	mInitialised(false)
,	mpCurrentBackBuffer(nullptr)
,	mDumpNextScreen( false )
{

}

//*****************************************************************************
//
//*****************************************************************************
IGraphicsContext::~IGraphicsContext()
{

}

//*****************************************************************************
//Also Known as do PS2 Graphics Frame
//*****************************************************************************
void IGraphicsContext::ClearAllSurfaces()
{
	//for (int i = 0; i < 2; i++)
	{
		gsKit_clear(gsGlobal, GS_SETREG_RGBAQ(0x00, 0x00, 0x00, 0x80, 0x00));
		gsKit_queue_exec(gsGlobal);
		gsKit_sync_flip(gsGlobal);
	}
}

//*****************************************************************************
//Clear screen and/or Zbuffer
//*****************************************************************************
void IGraphicsContext::ClearToBlack()
{
	//for (int i = 0; i < 2; i++)
	{
		//gsKit_set_test(gsGlobal, GS_ZTEST_OFF);
		gsKit_clear(gsGlobal, GS_SETREG_RGBAQ(0x00, 0x00, 0x00, 0x80, 0x00));
		//gsKit_set_test(gsGlobal, GS_ZTEST_ON);
	}
}

void IGraphicsContext::ClearZBuffer()
{
	//for (int i = 0; i < 2; i++)
	{
		//gsKit_set_test(gsGlobal, GS_ZTEST_OFF);
		gsKit_clear(gsGlobal, GS_SETREG_RGBAQ(0x00, 0x00, 0x00, 0x80, 0x00));
		//gsKit_set_test(gsGlobal, GS_ZTEST_ON);
	}
}

void IGraphicsContext::ClearColBuffer(const c32 & colour)
{
	//for (int i = 0; i < 2; i++)
	{
		gsKit_clear(gsGlobal, GS_SETREG_RGBAQ(colour.GetR(), colour.GetG(), colour.GetB(), (colour.GetA()) / 2, 0x00));
	}
}

void IGraphicsContext::ClearColBufferAndDepth(const c32 & colour)
{
	//for (int i = 0; i < 2; i++)
	{
		//gsKit_set_test(gsGlobal, GS_ZTEST_OFF);
		gsKit_clear(gsGlobal, GS_SETREG_RGBAQ(colour.GetR(), colour.GetG(), colour.GetB(), (colour.GetA()) / 2, 0x00));
		//gsKit_set_test(gsGlobal, GS_ZTEST_ON);
	}
}

//*****************************************************************************
//
//*****************************************************************************
void IGraphicsContext::BeginFrame()
{
	//printf("IGraphicsContext:: %s \n", __func__);
}

//*****************************************************************************
//
//*****************************************************************************
void IGraphicsContext::EndFrame()
{
	//printf("IGraphicsContext:: %s \n", __func__);
	//gsKit_sync_flip(gsGlobal);
	//gsKit_queue_exec(gsGlobal);
}

// This is used in game
//*****************************************************************************
//
//*****************************************************************************
void IGraphicsContext::UpdateFrame( bool wait_for_vbl )
{
	if (wait_for_vbl)
		gsKit_vsync_wait();

	gsKit_sync_flip(gsGlobal);
	gsKit_queue_exec(gsGlobal);

	if (mDumpNextScreen)
	{
		mDumpNextScreen--;
		if (!mDumpNextScreen)
		{
			if (gTakeScreenshotSS)	// We are taking a screenshot for savestate
			{
				gTakeScreenshotSS = false;
				StoreSaveScreenData();

			}
			else
			{
				DumpScreenShot();
			}
		}
	}

	SetDebugScreenTarget(TS_BACKBUFFER);	//Used to print FPS and other stats

	if (gCleanSceneEnabled)
	{
		//sceGuScissor(0, 0, SCR_WIDTH, SCR_HEIGHT);	//Make sure we clear whole screen
		ClearColBuffer(c32(0xff000000)); // ToDo : Use gFillColor instead?
	}

	// Hack to semi-fix XG2, it uses setprimdepth for background and also does not clear zbuffer //Corn
	//
	//if (g_ROM.GameHacks == EXTREME_G2) sceGuClear(GU_DEPTH_BUFFER_BIT | GU_FAST_CLEAR_BIT);	//Clear Zbuffer
}

//*****************************************************************************
//	Set the target for the debug screen
//*****************************************************************************
void IGraphicsContext::SetDebugScreenTarget( ETargetSurface buffer )
{

}

//*****************************************************************************
// Change current viewport, either for tv out or PSP itself
//
//*****************************************************************************
void IGraphicsContext::ViewportType( u32 * d_width, u32 * d_height ) const
{
		// Fullscreen
		*d_width = gsGlobal->Width;
		*d_height = gsGlobal->Height;
}

//*****************************************************************************
// Save current visible screen as PNG
// From Shazz/71M - thanks guys!
//*****************************************************************************
void IGraphicsContext::SaveScreenshot( const char* filename, s32 x, s32 y, u32 width, u32 height )
{

}

//*****************************************************************************
//
//*****************************************************************************
void IGraphicsContext::DumpScreenShot()
{

}

//*****************************************************************************
//
//*****************************************************************************
void IGraphicsContext::StoreSaveScreenData()
{

}

//*****************************************************************************
//
//*****************************************************************************
void IGraphicsContext::GetScreenSize(u32 * p_width, u32 * p_height) const
{
	*p_width = gsGlobal->Width;
	*p_height = gsGlobal->Height;
}

//*****************************************************************************
//
//*****************************************************************************
bool IGraphicsContext::Initialise()
{
	gsGlobal = gsKit_init_global();
	gsFontM = gsKit_init_fontm();


	if (g32bitColorMode)
	{
		gsGlobal->PSM = GS_PSM_CT32;
		gsGlobal->PSMZ = GS_PSMZ_32;
		gsZMax = 0xFFFFFFFF;
	}
	else
	{
		gsGlobal->PSM = GS_PSM_CT16S;
		gsGlobal->PSMZ = GS_PSMZ_32;
		gsZMax = 0xFFFFFFFF;
	}
	
	gsGlobal->DoubleBuffering = GS_SETTING_OFF;
	gsGlobal->PrimAlphaEnable = GS_SETTING_ON;
	gsGlobal->ZBuffering = GS_SETTING_ON;

	gsGlobal->Mode = GS_MODE_NTSC;
	gsGlobal->Interlace = GS_INTERLACED;
	gsGlobal->Field = GS_FIELD;
	gsGlobal->Width = 640;
	gsGlobal->Height = 480;

	gsKit_set_test(gsGlobal, GS_ZTEST_ON);

	dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC, D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);

	// Initialize the DMAC
	dmaKit_chan_init(DMA_CHANNEL_GIF);

	gsKit_init_screen(gsGlobal);

	gsKit_mode_switch(gsGlobal, GS_ONESHOT);

	gsKit_fontm_upload(gsGlobal, gsFontM);

	VuInit();
	//VuSetGeometryXYOffset(SCREEN_CENTER_X - 100, SCREEN_CENTER_Y + 100);

	clut_vram = gsKit_vram_alloc(gsGlobal, gsKit_texture_size(16, 16, GS_PSM_CT32), GSKIT_ALLOC_USERBUFFER);
	texture_vram = gsKit_vram_alloc(gsGlobal, gsKit_texture_size(640, 480, GS_PSM_CT32), GSKIT_ALLOC_USERBUFFER);

	gsKit_set_primalpha(gsGlobal, GS_SETREG_ALPHA(0, 1, 0, 1, 128), 0);

	gsTexA();

	// The app is ready to go
	mInitialised = true;
	return true;
}

void IGraphicsContext::SwitchToChosenDisplay()
{

}

void IGraphicsContext::SwitchToLcdDisplay()
{

}
