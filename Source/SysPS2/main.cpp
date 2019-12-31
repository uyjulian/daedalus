
#include "stdafx.h"

#include <stdlib.h>
#include <stdio.h>

#include <kernel.h>
#include <sbv_patches.h>
#include <loadfile.h>
#include <iopcontrol.h>
#include <iopheap.h>
#include <sifrpc.h>
#include <libpad.h>
#include <libps2time.h>
#include <fileXio_rpc.h>

#include "Config/ConfigOptions.h"
#include "Core/Cheats.h"
#include "Core/CPU.h"
#include "Core/CPU.h"
#include "Core/Memory.h"
#include "Core/PIF.h"
#include "Core/RomSettings.h"
#include "Core/Save.h"
#include "Debug/DBGConsole.h"
#include "Debug/DebugLog.h"
#include "Graphics/GraphicsContext.h"
#include "HLEGraphics/TextureCache.h"
#include "Input/InputManager.h"
#include "Interface/RomDB.h"
#include "SysPS2/Graphics/DrawText.h"
#include "SysPS2/UI/MainMenuScreen.h"
#include "SysPS2/UI/PauseScreen.h"
#include "SysPS2/UI/SplashScreen.h"
#include "SysPS2/UI/UIContext.h"
#include "SysPS2/Utility/PathsPS2.h"
#include "System/Paths.h"
#include "System/System.h"
#include "Test/BatchTest.h"
#include "Utility/IO.h"
#include "Utility/Preferences.h"
#include "Utility/Profiler.h"
#include "Utility/Thread.h"
#include "Utility/Translate.h"
#include "Utility/Timer.h"
#include "Plugins/AudioPlugin.h"

extern u8 iomanX_irx[];
extern int size_iomanX_irx;

extern u8 usbhdfsd_irx[];
extern int size_usbhdfsd_irx;

extern u8 usbd_irx[];
extern int size_usbd_irx;

extern u8 fileXio_irx[];
extern int size_fileXio_irx;

extern u8 libsd_irx[];
extern int size_libsd_irx;

extern u8 audsrv_irx[];
extern int size_audsrv_irx;

static char padBuf_t[2][256] __attribute__((aligned(64)));

bool g32bitColorMode = false;
bool PSP_IS_SLIM = false;

void WaitPadReady(int port, int slot)
{
	int state, lastState;
	char stateString[16];

	state = padGetState(port, slot);
	lastState = -1;
	while ((state != PAD_STATE_DISCONN)
		&& (state != PAD_STATE_STABLE)
		&& (state != PAD_STATE_FINDCTP1)) {
		if (state != lastState)
			padStateInt2String(state, stateString);
		lastState = state;
		state = padGetState(port, slot);
	}
}
void Wait_Pad_Ready(void)
{
	int state_1, state_2;

	state_1 = padGetState(0, 0);
	state_2 = padGetState(1, 0);
	while ((state_1 != PAD_STATE_DISCONN) && (state_2 != PAD_STATE_DISCONN)
		&& (state_1 != PAD_STATE_STABLE) && (state_2 != PAD_STATE_STABLE)
		&& (state_1 != PAD_STATE_FINDCTP1) && (state_2 != PAD_STATE_FINDCTP1)) {
		state_1 = padGetState(0, 0);
		state_2 = padGetState(1, 0);
	}
}
int Setup_Pad(void)
{
	int ret, i, port, state, modes;

	//padEnd();
	padInit(0);

	for (port = 0; port < 2; port++) {
		if ((ret = padPortOpen(port, 0, &padBuf_t[port][0])) == 0)
			return 0;
		WaitPadReady(port, 0);
		state = padGetState(port, 0);
		if (state != PAD_STATE_DISCONN) {
			modes = padInfoMode(port, 0, PAD_MODETABLE, -1);
			if (modes != 0) {
				i = 0;
				do {
					if (padInfoMode(port, 0, PAD_MODETABLE, i) == PAD_TYPE_DUALSHOCK) {
						padSetMainMode(port, 0, PAD_MMODE_DUALSHOCK, PAD_MMODE_LOCK);
						break;
					}
					i++;
				} while (i < modes);
			}
		}
	}
	return 1;
}

void Ps2IcacheInvalidateRange(void* start, int size)
{
	u32 inval_start = ((u32)start) & ~63;
	u32 inval_end = (((u32)start) + size + 64) & ~63;
	for (; inval_start < inval_end; inval_start += 64) {
		__asm__ __volatile__(
			".set noreorder\n\t"

			"sync.l \n"
			"cache 0x18, (%0) \n"
			"sync.l \n"
			"sync.p \n"
			"cache 0x0B, (%0) \n"
			"sync.p \n"
			".set reorder\n\t"

			: "=r" (inval_start)
			: "0" (inval_start)
		);
	}
}

void nop_delay(int count)
{
	int i, ret;
	for (i = 0; i < count; i++)
	{
		ret = 0x01000000;
		while (ret--)
		{
			asm("nop\nnop\nnop\nnop");
		}
	}
}

static bool	Initialize()
{
#ifdef USE_FILEXIO
	SifInitRpc(0);

	while (!SifIopReset(NULL, 0)) {};
	while (!SifIopSync()) {};

	fioExit();
	SifExitIopHeap();
	SifLoadFileExit();
	SifExitRpc();
	SifExitCmd();
#endif

	SifInitRpc(0);
	FlushCache(0);
	FlushCache(2);
	
	sbv_patch_enable_lmb();
	sbv_patch_disable_prefix_check();
	sbv_patch_fileio();

	SifLoadModule("rom0:SIO2MAN", 0, NULL);
	SifLoadModule("rom0:MCMAN", 0, NULL);
	SifLoadModule("rom0:MCSERV", 0, NULL);
	SifLoadModule("rom0:PADMAN", 0, NULL);

	SifExecModuleBuffer(iomanX_irx, size_iomanX_irx, 0, NULL, NULL);

#ifdef USE_FILEXIO
	SifExecModuleBuffer(fileXio_irx, size_fileXio_irx, 0, NULL, NULL);
	fileXioInit();
#endif

	SifExecModuleBuffer(usbd_irx, size_usbd_irx, 0, NULL, NULL);
	SifExecModuleBuffer(usbhdfsd_irx, size_usbhdfsd_irx, 0, NULL, NULL);
	SifExecModuleBuffer(libsd_irx, size_libsd_irx, 0, NULL, NULL);
	SifExecModuleBuffer(audsrv_irx, size_audsrv_irx, 0, NULL, NULL);

	nop_delay(2);

	ps2time_init();
	Setup_Pad();
	Wait_Pad_Ready();

	strcpy(gDaedalusExePath, DAEDALUS_PS2_PATH(""));
	
	struct padButtonStatus pad;
	u32 buttons = 0;

	if (padRead(0, 0, &pad))
	{
		buttons = pad.btns ^ 0xFFFF;
	}


	if (buttons & PAD_CIRCLE)
		g32bitColorMode = true;
	else
		g32bitColorMode = false;

	// Init the savegame directory -- We probably should create this on the fly if not as it causes issues.
	strcpy(g_DaedalusConfig.mSaveDir, DAEDALUS_PS2_PATH("SaveGames/"));

	/*int size = 18 * 1024 * 1024;
	char *p = (char *)malloc(size);
	printf("malloc %p size %d\n", p, size/(1024*1024));*/

	if (!System_Init())
		return false;

#ifdef DAEDALUS_DEBUG_CONSOLE	
	Debug_SetLoggingEnabled(true);
#endif

	//gGlobalPreferences.DisplayFramerate = 1;

	return true;
}

void HandleEndOfFrame()
{
	//
	//	Enter the debug menu as soon as select is newly pressed
	//
	static u32 oldButtons = 0;
	struct padButtonStatus pad;
	bool		activate_pause_menu = false;
	u32 buttons = 0;

	Wait_Pad_Ready();

	if (padRead(0, 0, &pad))
	{
		buttons = pad.btns ^ 0xFFFF;
	}

	// If kernelbuttons.prx couldn't be loaded, allow select button to be used instead
	//
	if (oldButtons != buttons)
	{
		if (gCheatsEnabled && (buttons & PAD_SELECT))
		{
			CheatCodes_Activate(GS_BUTTON);
		}

		if (buttons & PAD_L3)
		{
			activate_pause_menu = true;

			if (gAudioPlugin != nullptr)
				gAudioPlugin->StopAudio();
		}
	}

	if (activate_pause_menu)
	{

		CGraphicsContext::Get()->SwitchToLcdDisplay();
		CGraphicsContext::Get()->ClearAllSurfaces();

		CDrawText::Initialise();

		CUIContext* p_context(CUIContext::Create());

		if (p_context != NULL)
		{
			// Already set in ClearBackground() @ UIContext.h
			//p_context->SetBackgroundColour( c32( 94, 188, 94 ) );		// Nice green :)

			CPauseScreen* pause(CPauseScreen::Create(p_context));
			pause->Run();
			delete pause;
			delete p_context;
		}

		CDrawText::Destroy();

		//
		// Commit the preferences database before starting to run
		//
		CPreferences::Get()->Commit();
	}
}

static void DisplayRomsAndChoose(bool show_splash)
{
	// switch back to the LCD display
	CGraphicsContext::Get()->SwitchToLcdDisplay();

	CDrawText::Initialise();

	CUIContext* p_context(CUIContext::Create());

	if (p_context != NULL)
	{

		if (show_splash)
		{
			CSplashScreen* p_splash(CSplashScreen::Create(p_context));
			p_splash->Run();
			delete p_splash;
		}

		CMainMenuScreen* p_main_menu(CMainMenuScreen::Create(p_context));
		p_main_menu->Run();
		delete p_main_menu;
	}

	delete p_context;

	CDrawText::Destroy();
}

int main(int argc, char* argv[])
{
	//argc = 2;
	//argv[1] = "Roms/rom6.z64";
	if (Initialize())
	{
#ifdef DAEDALUS_BATCH_TEST_ENABLED
		if (argc > 1)
		{
			BatchTestMain(argc, argv);
		}
#else
		//Makes it possible to load a ROM directly without using the GUI
		//There are no checks for wrong file name so be careful!!!
		//Ex. from PSPLink -> ./Daedalus.prx "Roms/StarFox 64.v64" //Corn
		if (argc > 1)
		{
			printf("Loading %s\n", argv[1]);
			System_Open(argv[1]);
			CPU_Run();
			System_Close();
			System_Finalize();
			Exit(0);
			return 0;
		}
#endif
		//Translate_Init();
		bool show_splash = true;
		for (;;)
		{
			DisplayRomsAndChoose(show_splash);
			show_splash = false;

			CRomDB::Get()->Commit();
			CPreferences::Get()->Commit();

			CPU_Run();
			System_Close();
		}

		System_Finalize();
	}

	Exit(0);
	return 0;
}
