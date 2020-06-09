#CMake Toolchain File for DaedalusX64
if (PSP)
set (CMAKE_TOOLCHAIN_FILE /usr/local/pspdev/psp/share/cmake/PSP.cmake)
enable_language(ASM)
endif (PSP)

cmake_minimum_required(VERSION 3.7)
set(CMAKE_CXX_STANDARD 14)
project (DaedalusX64)


	#Base Files  
		set (CONFIG Config/ConfigOptions.cpp)
		set (CORE_FILES Core/HvqmTask.cpp Core/RDRam.cpp Core/Cheats.cpp Core/CPU.cpp Core/DMA.cpp Core/Dynamo.cpp Core/FlashMem.cpp  Core/FramerateLimiter.cpp Core/Interpret.cpp Core/Interrupt.cpp Core/JpegTask.cpp Core/Memory.cpp Core/PIF.cpp Core/PrintOpCode.cpp Core/R4300.cpp Core/Registers.cpp Core/ROM.cpp Core/ROMBuffer.cpp Core/ROMImage.cpp Core/RomSettings.cpp Core/RSP_HLE.cpp Core/Save.cpp Core/TLB.cpp)
		set (DEBUG_FILES Debug/DebugConsoleImpl.cpp Debug/DebugLog.cpp Debug/Dump.cpp Debug/Synchroniser.cpp )
		set (DYNAREC_FILES DynaRec/BranchType.cpp DynaRec/DynaRecProfile.cpp DynaRec/Fragment.cpp DynaRec/FragmentCache.cpp DynaRec/IndirectExitMap.cpp DynaRec/StaticAnalysis.cpp DynaRec/TraceRecorder.cpp)
		set (GRAPHICS_FILES Graphics/ColourValue.cpp Graphics/PngUtil.cpp Graphics/TextureTransform.cpp)
		set (HLEAUDIO_FILES HLEAudio/AudioHLEProcessor.cpp HLEAudio/ABI1.cpp HLEAudio/ABI2.cpp HLEAudio/ABI3.cpp HLEAudio/ABI3mp3.cpp HLEAudio/AudioBuffer.cpp HLEAudio/HLEMain.cpp HLEAudio/ABI_ADPCM.cpp HLEAudio/ABI_Buffers.cpp HLEAudio/ABI_Filters.cpp HLEAudio/ABI_MixerInterleave.cpp HLEAudio/ENV_Mixer.cpp HLEAudio/ABI_Resample.cpp)
		set (HLEGRAPHICS_FILES HLEGraphics/BaseRenderer.cpp HLEGraphics/CachedTexture.cpp HLEGraphics/ConvertImage.cpp HLEGraphics/ConvertTile.cpp HLEGraphics/DLDebug.cpp HLEGraphics/DLParser.cpp HLEGraphics/GraphicsPlugin.cpp HLEGraphics/Microcode.cpp HLEGraphics/RDP.cpp  HLEGraphics/RDPStateManager.cpp  HLEGraphics/TextureCache.cpp HLEGraphics/TextureCacheWebDebug HLEGraphics/TextureInfo.cpp HLEGraphics/uCodes/Ucode.cpp)
		set (INTERFACE_FILES Interface/Preferences.cpp Interface/RomDB.cpp Interface/SaveState.cpp)
		set (MAIN_FILES Main/SystemInit.cpp)
		set (MATH_FILES Math/Matrix4x4.cpp)
		set (OSHLE_FILES OSHLE/OS.cpp OSHLE/patch.cpp)
		set (ROMFILES RomFile/RomFile.cpp RomFile/RomFileCache.cpp RomFile/RomFileCompressed.cpp RomFile/RomFileMemory.cpp RomFile/RomFileUncompressed.cpp)
		set (SYSTEM_FILES System/CompressedStream.cpp System/DataSink.cpp System/Paths.cpp)
		set (TEST_FILES Test/BatchTest.cpp)
		set (UTILITY_FILES Utility/CRC.cpp  Utility/FastMemcpy.cpp Utility/Hash.cpp Utility/IniFile.cpp Utility/MemoryHeap.cpp Utility/Profiler.cpp Utility/Stream.cpp Utility/StringUtil.cpp Utility/Timer.cpp  Utility/Translate.cpp)
		set (UNKNOWN_FILES Utility/FastMemcpy_test.cpp Utility/MemoryPool.cpp)
		set (BUILD ${CONFIG} ${CORE_FILES} ${DEBUG_FILES} ${DYNAREC_FILES} ${GRAPHICS_FILES} ${HLEAUDIO_FILES} ${HLEGRAPHICS_FILES} ${INTERFACE_FILES} ${MAIN_FILES} ${MATH_FILES} ${OSHLE_FILES} ${ROMFILES} ${SYSTEM_FILES} ${TEST_FILES} ${UTILITY_FILES})
		set (BASE_DEFINES -DDAEDALUS_ENABLE_DYNAREC -DDAEDALUS_ENABLE_OS_HOOKS)

	#PSP Build
		set (PSP_DEBUG_FILES Debug/PSP/Assert.cpp Debug/PSP/DebugConsole.cpp )
		set (PSP_DVEMGR_FILES SysPSP/PRX/DveMgr/pspDveManager.S)
		set (PSP_DYNAREC_FILES DynaRec/PSP/AssemblyWriterPSP.cpp DynaRec/PSP/AssemblyUtilsPSP.cpp DynaRec/PSP/CodeBufferManagerPSP.cpp DynaRec/PSP/CodeGeneratorPSP.cpp DynaRec/PSP/DynaRecStubs.S DynaRec/PSP/N64RegisterCachePSP.cpp)
		set (PSP_GRAPHICS_FILES SysPSP/Graphics/DrawText.cpp SysPSP/Graphics/GraphicsContext.cpp SysPSP/Graphics/NativeTexturePSP.cpp SysPSP/Graphics/VideoMemoryManager.cpp SysPSP/Graphics/intraFont/intraFont.c SysPSP/Graphics/intraFont/libccc.c)
		set (PSP_HLEAUDIO_FILES HLEAudio/PSP/AudioPlugin.cpp)
		set (PSP_HLEGRAPHICS_FILES SysPSP/HLEGraphics/Blender.cpp SysPSP/HLEGraphics/BlendModes.cpp SysPSP/HLEGraphics/ColourAdjuster.cpp SysPSP/HLEGraphics/ConvertVertices.S HLEGraphics/PSP/DisplayListDebugger.cpp SysPSP/HLEGraphics/GraphicsPluginPSP.cpp SysPSP/HLEGraphics/RendererPSP.cpp SysPSP/HLEGraphics/Combiner/BlendConstant.cpp SysPSP/HLEGraphics/Combiner/CombinerExpression.cpp SysPSP/HLEGraphics/Combiner/CombinerTree.cpp SysPSP/HLEGraphics/Combiner/RenderSettings.cpp SysPSP/HLEGraphics/TnLVFPU.S SysPSP/HLEGraphics/VectorClipping.S)
		set (PSP_INPUTMANAGER_FILES SysPSP/Input/InputManagerPSP.cpp)
		set (PSP_KERNELBUTTON_FILES SysPSP/PRX/KernelButtons/imposectrl.S)
        set (PSP_MEDIAENGINEFILES SysPSP/PRX/MediaEngine/me.c SysPSP/PRX/MediaEngine/MediaEngine.S )
		set (PSP_UI_FILES SysPSP/UI/AboutComponent.cpp SysPSP/UI/AdjustDeadzoneScreen.cpp SysPSP/UI/AdvancedOptionsScreen.cpp SysPSP/UI/CheatOptionsScreen.cpp SysPSP/UI/ColourPulser.cpp SysPSP/UI/Dialogs.cpp SysPSP/UI/GlobalSettingsComponent.cpp SysPSP/UI/MainMenuScreen.cpp SysPSP/UI/PauseOptionsComponent.cpp SysPSP/UI/PauseScreen.cpp SysPSP/UI/RomPreferencesScreen.cpp SysPSP/UI/RomSelectorComponent.cpp SysPSP/UI/SavestateSelectorComponent.cpp SysPSP/UI/SelectedRomComponent.cpp SysPSP/UI/SplashScreen.cpp SysPSP/UI/UICommand.cpp SysPSP/UI/UIComponent.cpp SysPSP/UI/UIContext.cpp SysPSP/UI/UIElement.cpp SysPSP/UI/UIScreen.cpp SysPSP/UI/UISetting.cpp)
		set (PSP_SYSTEM System/PSP/IO.cpp System/PSP/Thread.cpp System/PSP/Timing.cpp)
		set (PSP_UTILITY_FILES SysPSP/Utility/AtomicPrimitives.S SysPSP/Utility/BatteryPSP.cpp SysPSP/Utility/Buttons.cpp SysPSP/Utility/DebugMemory.cpp SysPSP/Utility/DisableFPUExceptions.S SysPSP/Utility/exception.cpp SysPSP/Utility/FastMemcpyPSP.cpp SysPSP/Utility/JobManager.cpp SysPSP/Utility/ModulePSP.cpp SysPSP/Utility/VolatileMemPSP.cpp)
		set (GPROF_SRCS SysPSP/Debug/prof.c SysPSP/Debug/mcount.S )
		set (PSP_MAIN_FILES Main/PSP/main.cpp)

		set (PSP_BUILD ${PSP_DEBUG_FILES} ${PSP_DVEMGR_FILES} ${PSP_DYNAREC_FILES} ${PSP_GRAPHICS_FILES} ${PSP_HLEAUDIO_FILES} ${PSP_HLEGRAPHICS_FILES} ${PSP_INPUTMANAGER_FILES} ${PSP_KERNELBUTTON_FILES} ${PSP_MEDIAENGINEFILES} ${PSP_SYSTEM} ${PSP_UI_FILES} ${PSP_UTILITY_FILES})
		set (PSPSDK_LIBS -lpspdebug -lpspdisplay -lpspge -lpspctrl -lpspsdk -lpsplibc -lpsputility -lpspuser -lpspkernel -lpspaudiolib)
		set (PSP_DEFINES ${BASE_DEFINES} -DDAEDALUS_PSP -DDAEDALUS_PSP_USE_ME -DDAEDALUS_PSP_USE_VFPU -DDAEDALUS_ENDIAN_MODE=DAEDALUS_ENDIAN_LITTLE)

	#SysGL
		set (SYSGL_GRAPHICS SysGL/Graphics/GraphicsContextGL.cpp SysGL/Graphics/NativeTextureGL.cpp)
		set (SYSGL_HLEGRAPHICS SysGL/HLEGraphics/GraphicsPluginGL.cpp SysGL/HLEGraphics/RendererGL.cpp)
		set (SYSGL_INPUT SysGL/Input/InputManagerGL.cpp)
		set (SYSGL_INTERFACE SysGL/Interface/UI.cpp)
		set (SYSGL_BUILD ${SYSGL_GRAPHICS} ${SYSGL_HLEGRAPHICS} ${SYSGL_INPUT} ${SYSGL_INTERFACE} ${PLUGIN_FILES})

	#Windows -- Not sure if it works yet
		set (WIN_AUDIO HLEAudio/Windows/AudioPlugin.cpp)
		set (WIN_DEBUG Debug/Windows/Assert.cpp)
		set (WIN_DYNAREC DynaRec/Windows/x86/AssemblyUtilsX86.cpp DynaRec/Windows/x86/AssemblyWriterX86.cpp DynaRec/Windows/x86/CodeBufferManagerX86.cpp DynaRec/Windows/x86/CodeGeneratorX86.cpp DynaRec/Windows/DISASM86.cpp)
		set (WIN_HLEGRAPHICS HLEGraphics/Posix/DisplayListDebugger.cpp)
		set (WIN_SYSTEM System/Windows/Condition.cpp System/Windows/IO.cpp System/Windows/Thread.cpp System/Windows/Timing.cpp)
		set (WIN_MAIN_FILES Main/Windows/main.cpp)
		set (WIN_BUILD ${WIN_AUDIO} ${WIN_DEBUG})
		set (WIN_DEFINES -DDAEDALUS_W32 -DDAEDALUS_ENABLE_OS_HOOKS -DDAEDALUS_GL -DDAEDALUS_ACCURATE_TMEM -DDAEDALUS_ENDIAN_MODE=DAEDALUS_ENDIAN_LITTLE)

    #Posix
		set (POSIX_DEBUG Debug/Posix/Assert.cpp Debug/Posix/DebugConsole.cpp Debug/WebDebug.cpp Debug/WebDebugTemplate.cpp third_party/webby/webby.c)
		set (POSIX_DYNAREC SysPosix/DynaRec/CodeBufferManager.cpp)
		set (POSIX_HLEGRAPHICS HLEGraphics/Posix/DisplayListDebugger.cpp)
		set (POSIX_MAIN_FILES Main/Posix/main.cpp)
		set (POSIX_SYSTEM System/Posix/Condition.cpp System/Posix/IO.cpp System/Posix/Thread.cpp System/Posix/Timing.cpp)
		set (POSIX_BUILD ${POSIX_DEBUG} ${POSIX_DYNAREC} ${POSIX_SYSTEM} ${POSIX_HLEGRAPHICS} ${POSIX_UTILITY})
		set (POSIX_DEFINES -DDAEDALUS_POSIX -DDAEDALUS_ENDIAN_MODE=DAEDALUS_ENDIAN_LITTLE -DDAEDALUS_ENABLE_OS_HOOKS -DDAEDALUS_ACCURATE_TMEM )
				
	# Audio Plugins are different per posix platform
	if (LINUX)
		set (POSIX_AUDIO HLEAudio/Linux/AudioPlugin.cpp)
	else (MAC)
		set (POSIX_AUDIO HLEAudio/macOS/AudioPlugin.cpp)
	endif (LINUX)

	if (DEBUG)
		message("Daedalus Debug Build..")
		set (CMAKE_BUILD_TYPE "Debug")
		add_definitions("-O2 -g -G0 -D_DEBUG -Wall -DDAEDALUS_CONFIG_DEBUG ")
	else (RELEASE)
		message("Daedalus Release Build..")
		set (CMAKE_BUILD_TYPE "Release")
		add_definitions("-O2 -W -DNDEBUG -DDAEDALUS_CONFIG_RELEASE ")
	endif (DEBUG)


	if (PSP)
		add_definitions(${PSP_DEFINES})
		include_directories(${PROJECT_SOURCE_DIR})
		add_library(pspkubridge STATIC IMPORTED)
		set_property(TARGET pspkubridge PROPERTY IMPORTED_LOCATION ${PROJECT_SOURCE_DIR}/SysPSP/Include/lib/libpspkubridge.a)
		include_directories(BEFORE ${PROJECT_SOURCE_DIR}/SysPSP/Include/)
		add_library(daedaluslib STATIC ${BUILD} ${PSP_BUILD})
		add_executable(daedalus ${PSP_MAIN_FILES})
		target_link_libraries(daedalus daedaluslib -lstdc++ -lpsppower -lpspgu -lpspaudiolib -lpspaudio -lpsprtc -lpng -lz -lc -lg -lm -lpspfpu pspkubridge ${PSP_LIBRARIES})
		create_pbp_file(TARGET daedalus TITLE DaedalusX64 ICON_PATH ${PROJECT_SOURCE_DIR}/SysPSP/Resources/eboot_icons/icon0.png BACKGROUND_PATH ${PROJECT_SOURCE_DIR}/SysPSP/Resources/eboot_icons/pic1.png)
	endif (PSP)

 
	if (MAC)
		add_definitions("-DDAEDALUS_OSX")
		FIND_PATH(OPENGL_INCLUDE_DIR gl.h) # is this required?
 		FIND_LIBRARY(OPENGL_LIBRARY OpenGL)
		include_directories(/usr/local/include -framework OpenGL)
		link_directories(/usr/local/lib)
	endif (MAC)

	if (LINUX)
		add_definitions("-DDAEDALUS_LINUX")
	endif (LINUX)

	if (MAC OR LINUX)
		add_definitions(${POSIX_DEFINES})
		include_directories(${PROJECT_SOURCE_DIR})

	#Build SysGL
		add_library(sysGL STATIC ${SYSGL_BUILD})
		target_link_libraries(sysGL GL GLEW -lSDL2 dl X11  )

		add_library(daedalus.lib STATIC ${BUILD} ${POSIX_BUILD} ${POSIX_AUDIO} )
		target_link_libraries(daedalus.lib sysGL -lGL  -lSDL2 -lGLEW png z minizip pthread)
	#Build and Link Executable
		add_executable(daedalus ${POSIX_MAIN_FILES})
		target_link_libraries(daedalus LINK_PUBLIC daedalus.lib )
	endif (MAC OR LINUX)