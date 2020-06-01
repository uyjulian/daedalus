 #CMake File for DaedalusX64

#Build instructions
# Create a directory in the DaedalusX64 root directory and cd into it 'eg: mkdir tmp ~/DaedalusX64'
# ensure you have cmake installed
# For PSP:
# cmake -DCMAKE_TOOLCHAIN_FILE=../Tools/psptoolchain.cmake ../Source/ -DPSP_OPTION

#Options
# PSP_RELEASE - Builds PSP Release

cmake_minimum_required(VERSION 3.7)
set(CMAKE_CXX_STANDARD 14)
project (DaedalusX64)

				#Default Files for build
				set (CONFIG Config/ConfigOptions.cpp)
				set (CORE_FILES Core/HvqmTask.cpp Core/RDRam.cpp Core/Cheats.cpp Core/CPU.cpp Core/DMA.cpp Core/Dynamo.cpp Core/FlashMem.cpp  Core/FramerateLimiter.cpp Core/Interpret.cpp Core/Interrupt.cpp Core/JpegTask.cpp Core/Memory.cpp Core/PIF.cpp Core/PrintOpCode.cpp Core/R4300.cpp Core/Registers.cpp Core/ROM.cpp Core/ROMBuffer.cpp Core/ROMImage.cpp Core/RomSettings.cpp Core/RSP_HLE.cpp Core/Save.cpp Core/TLB.cpp)
				set (DEBUG_FILES Debug/DebugConsoleImpl.cpp Debug/DebugLog.cpp Debug/Dump.cpp Debug/Synchroniser.cpp )
				set (DYNAREC_FILES DynaRec/BranchType.cpp DynaRec/DynaRecProfile.cpp DynaRec/Fragment.cpp DynaRec/FragmentCache.cpp DynaRec/IndirectExitMap.cpp DynaRec/StaticAnalysis.cpp DynaRec/TraceRecorder.cpp)
				set (GRAPHICS_FILES Graphics/ColourValue.cpp Graphics/PngUtil.cpp Graphics/TextureTransform.cpp)
				set (HLEAUDIO_FILES HLEAudio/AudioHLEProcessor.cpp HLEAudio/ABI1.cpp HLEAudio/ABI2.cpp HLEAudio/ABI3.cpp HLEAudio/ABI3mp3.cpp HLEAudio/AudioBuffer.cpp HLEAudio/HLEMain.cpp HLEAudio/ABI_ADPCM.cpp HLEAudio/ABI_Buffers.cpp HLEAudio/ABI_Filters.cpp HLEAudio/ABI_MixerInterleave.cpp HLEAudio/ENV_Mixer.cpp HLEAudio/ABI_Resample.cpp)
				set (HLEGRAPHICS_FILES HLEGraphics/BaseRenderer.cpp HLEGraphics/CachedTexture.cpp HLEGraphics/ConvertImage.cpp HLEGraphics/ConvertTile.cpp HLEGraphics/DLDebug.cpp HLEGraphics/DLParser.cpp HLEGraphics/GraphicsPlugin.cpp HLEGraphics/Microcode.cpp HLEGraphics/RDP.cpp  HLEGraphics/RDPStateManager.cpp  HLEGraphics/TextureCache.cpp HLEGraphics/TextureCacheWebDebug HLEGraphics/TextureInfo.cpp HLEGraphics/uCodes/Ucode.cpp)
				set (INTERFACE_FILES Interface/Preferences.cpp Interface/RomDB.cpp Interface/SaveState.cpp)
				set (MATH_FILES Math/Matrix4x4.cpp)
				set (OSHLE_FILES OSHLE/OS.cpp OSHLE/patch.cpp)
				set (ROMFILES RomFile/RomFile.cpp RomFile/RomFileCache.cpp RomFile/RomFileCompressed.cpp RomFile/RomFileMemory.cpp RomFile/RomFileUncompressed.cpp)
				set (SYSTEM_FILES System/CompressedStream.cpp System/DataSink.cpp System/Paths.cpp System/SystemInit.cpp)
				set (TEST_FILES Test/BatchTest.cpp)
				set (UTILITY_FILES Utility/CRC.cpp  Utility/FastMemcpy.cpp Utility/Hash.cpp Utility/IniFile.cpp Utility/MemoryHeap.cpp Utility/Profiler.cpp Utility/Stream.cpp Utility/StringUtil.cpp Utility/Timer.cpp  Utility/Translate.cpp)
				set (UNKNOWN_FILES Utility/FastMemcpy_test.cpp Utility/MemoryPool.cpp)
				set (DEBUG_ONLY Core/Registers.cpp)
				set (BUILD ${CONFIG} ${CORE_FILES} ${DEBUG_FILES} ${DYNAREC_FILES} ${GRAPHICS_FILES} ${HLEAUDIO_FILES} ${HLEGRAPHICS_FILES} ${INTERFACE_FILES} ${MATH_FILES} ${OSHLE_FILES} ${ROMFILES} ${SYSTEM_FILES} ${TEST_FILES} ${UTILITY_FILES})
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
				set(PSPSDK_LIBS -lpspdebug -lpspdisplay -lpspge -lpspctrl -lpspsdk -lpsplibc -lpsputility -lpspuser -lpspkernel -lpspaudiolib)
				set (PSP_DEFINES ${BASE_DEFINES} -DDAEDALUS_PSP -DDAEDALUS_PSP_USE_ME -DDAEDALUS_PSP_USE_VFPU -DDAEDALUS_ENDIAN_MODE=DAEDALUS_ENDIAN_LITTLE)

				#Handle .S files - Probably a better way to do this
				set_property(SOURCE SysPSP/HLEGraphics/ConvertVertices.S PROPERTY LANGUAGE C)
				set_property(SOURCE DynaRec/PSP/DynaRecStubs.S PROPERTY LANGUAGE C)
				set_property(SOURCE SysPSP/HLEGraphics/TnLVFPU.S PROPERTY LANGUAGE C)
				set_property(SOURCE SysPSP/HLEGraphics/VectorClipping.S PROPERTY LANGUAGE C)
				set_property(SOURCE SysPSP/Utility/AtomicPrimitives.S PROPERTY LANGUAGE C)
				set_property(SOURCE SysPSP/Utility/DisableFPUExceptions.S PROPERTY LANGUAGE C)
				set_property(SOURCE SysPSP/Debug/mcount.S PROPERTY LANGUAGE C)
				set_property(SOURCE SysPSP/PRX/DveMgr/pspDveManager.S PROPERTY LANGUAGE C)
				set_property(SOURCE SysPSP/PRX/KernelButtons/imposectrl.S PROPERTY LANGUAGE C)
				set_property(SOURCE SysPSP/PRX/MediaEngine/MediaEngine.S PROPERTY LANGUAGE C)


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
				# These will remain separate for now..
				set (LINUX_AUDIO HLEAudio/Linux/AudioPlugin.cpp)
				set (MAC_AUDIO HLEAudio/macOS/AudioPlugin.cpp)

if (PSP_RELEASE)
	message("PSP Release Build..")
	add_definitions("-O2 -W -DNDEBUG")
	add_definitions(${PSP_DEFINES} -DDAEDALUS_CONFIG_RELEASE)
	include_directories(${PROJECT_SOURCE_DIR})
	add_library(pspkubridge STATIC IMPORTED)
	set_property(TARGET pspkubridge PROPERTY IMPORTED_LOCATION ${PROJECT_SOURCE_DIR}/SysPSP/Include/lib/libpspkubridge.a)
	include_directories(BEFORE ${PROJECT_SOURCE_DIR}/SysPSP/Include/)
	add_library(daedaluslib STATIC ${BUILD} ${PSP_BUILD})
	add_executable(daedalus ${PSP_MAIN_FILES})
	target_link_libraries(daedalus daedaluslib -lstdc++ -lpsppower -lpspgu -lpspaudiolib -lpspaudio -lpsprtc -lpng -lz -lc -lg -lm -lpspfpu pspkubridge ${PSP_LIBRARIES})
	create_pbp_file(TARGET daedalus TITLE DaedalusX64 ICON_PATH ${PROJECT_SOURCE_DIR}/SysPSP/Resources/eboot_icons/icon0.png BACKGROUND_PATH ${PROJECT_SOURCE_DIR}/SysPSP/Resources/eboot_icons/pic1.png)
endif (PSP_RELEASE)

#Only difference between PSP Release and Debug is the definitions and Config Dev File - Try and merge

if (PSP_DEBUG)
	message("PSP Debug Build..")
	include_directories(${PROJECT_SOURCE_DIR})
	add_definitions("-g -O2 -fno-omit-frame-pointer -G0 -D_DEBUG -MD -Wall")
	add_definitions(${PSP_DEFINES} -DDAEDALUS_CONFIG_DEV)
	add_library(pspkubridge STATIC IMPORTED)
	set_property(TARGET pspkubridge PROPERTY IMPORTED_LOCATION ${PROJECT_SOURCE_DIR}/SysPSP/Include/lib/libpspkubridge.a)
	include_directories(BEFORE ${PROJECT_SOURCE_DIR}/Main/PSP ${PROJECT_SOURCE_DIR}/SysPSP/Include/)
	add_library(daedalus STATIC ${BUILD} ${PSP_BUILD} ${DEBUG_ONLY})
	add_executable(daedalus.elf ${PSP_MAIN_FILES})
	set_target_properties(daedalus.elf PROPERTIES LINK_FLAGS "-specs=${PSPSDK}/lib/prxspecs -Wl,-q,-T${PSPSDK}/lib/linkfile.prx ${PSPSDK}/lib/prxexports.o -L${PSPSDK}/lib")
	target_include_directories(daedalus.elf PUBLIC /usr/local/pspdev/psp/sdk/include )
	target_link_libraries(daedalus.elf daedalus -lstdc++ -lpsppower -lpspgu -lpspaudiolib -lpspaudio -lpsprtc -lpng -lz -lc -lg -lm -lpspfpu pspkubridge ${PSPSDK_LIBS})
	create_pbp_file(TARGET daedalus.elf TITLE DaedalusX64 ICON_PATH ${PROJECT_SOURCE_DIR}/SysPSP/Resources/eboot_icons/icon0.png BACKGROUND_PATH ${PROJECT_SOURCE_DIR}/SysPSP/Resources/eboot_icons/pic1.png)

endif (PSP_DEBUG)

if (MAC_RELEASE OR LINUX_RELEASE)

add_definitions("-DNDEBUG -O3 -DDAEDALUS_POSIX")
	add_definitions(${POSIX_DEFINES} -DDAEDALUS_CONFIG_RELEASE -DDAEDALUS_ENDIAN_MODE=DAEDALUS_ENDIAN_LITTLE)
include_directories(${PROJECT_SOURCE_DIR/HLEGraphics})
include_directories(${PROJECT_SOURCE_DIR})

endif (MAC_RELEASE OR LINUX_RELEASE)

if (MAC_DEBUG OR LINUX_DEBUG)

add_definitions("-D_DEBUG -g -DDAEDALUS_POSIX")
	add_definitions(${POSIX_DEFINES} -DDAEDALUS_ENDIAN_MODE=DAEDALUS_ENDIAN_LITTLE)
include_directories(${PROJECT_SOURCE_DIR/HLEGraphics})
include_directories(${PROJECT_SOURCE_DIR})

endif (MAC_DEBUG OR LINUX_DEBUG)


if (LINUX_RELEASE OR LINUX_DEBUG)


		message("Linux Release Build..")
		    add_definitions("-DDAEDALUS_LINUX" "-DDAEDALUS_GL" -g)

		#Build SysGL Lib
		add_library(sysGL STATIC ${SYSGL_BUILD})
		target_link_libraries(sysGL GL GLEW -lSDL2 dl X11  )

		#Build Daedalus Lib
		add_library(daedalus.lib STATIC ${BUILD} ${POSIX_BUILD} ${LINUX_AUDIO} )
	target_link_libraries(daedalus.lib sysGL -lGL  -lSDL2 -lGLEW png z minizip pthread)

		#Build and Link Executable
			add_executable(daedalus ${POSIX_MAIN_FILES})
		target_link_libraries(daedalus LINK_PUBLIC daedalus.lib )
endif (LINUX_RELEASE OR LINUX_DEBUG)


if (MAC_RELEASE OR MAC_DEBUG)

		message("Mac Release Build..")
FIND_PATH(OPENGL_INCLUDE_DIR gl.h)
	FIND_LIBRARY(OPENGL_LIBRARY OpenGL)
		include_directories(/usr/local/include -framework OpenGL)
		link_directories(/usr/local/lib)

    add_definitions("-DDAEDALUS_OSX -DDAEDALUS_GL -g -Wno-narrowing")
		#Build SysGL Lib
		add_library(sysGL STATIC ${SYSGL_BUILD})
		target_link_libraries(sysGL -lGLEW -lSDL2 -ldl "-framework OpeNGL -framework AudioToolbox -framework Cocoa -framework IOKit -framework CoreVideo"  )

		#Build Daedalus Lib
		add_library(daedalus.lib STATIC ${BUILD} ${POSIX_BUILD} ${MAC_AUDIO} )
	target_link_libraries(daedalus.lib sysGL png z minizip pthread)

		#Build and Link Executable
			add_executable(daedalus ${POSIX_MAIN_FILES})
		target_link_libraries(daedalus LINK_PUBLIC daedalus.lib )
endif (MAC_RELEASE OR MAC_DEBUG)
