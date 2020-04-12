/*
Copyright (C) 2003 Azimer
Copyright (C) 2001,2006 StrmnNrmn

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

//
//	N.B. This source code is derived from Azimer's Audio plugin (v0.55?)
//	and modified by StrmnNrmn to work with Daedalus PSP. Thanks Azimer!
//	Drop me a line if you get chance :)
//

#include "stdafx.h"

#include <pspkernel.h>

#include "AudioPluginPSP.h"
#include "AudioOutput.h"
#include "HLEAudio/audiohle.h"

#include "Config/ConfigOptions.h"
#include "Core/CPU.h"
#include "Core/Interrupt.h"
#include "Core/Memory.h"
#include "Core/ROM.h"
#include "Core/RSP_HLE.h"
#include "SysPSP/Utility/JobManager.h"

#define RSP_AUDIO_INTR_CYCLES     5 // This can be adjusted accordingly. Not sure on best value yet

/* This sets default frequency what is used if rom doesn't want to change it.
   Probably only game that needs this is Zelda: Ocarina Of Time Master Quest
   *NOTICE* We should try to find out why Demos' frequencies are always wrong
   They tend to rely on a default frequency, apparently, never the same one ;)*/

#define DEFAULT_FREQUENCY 44100	// Taken from Mupen64 : )


EAudioPluginMode gAudioPluginEnabled( APM_DISABLED );
//bool gAdaptFrequency( false );


CAudioPluginPSP::CAudioPluginPSP()
:	mAudioOutput( new AudioOutput )
{
	//mAudioOutput->SetAdaptFrequency( gAdaptFrequency );
	//gAudioPluginEnabled = APM_ENABLED_SYNC; // for testing
}


CAudioPluginPSP::~CAudioPluginPSP()
{
	delete mAudioOutput;
}


CAudioPluginPSP *CAudioPluginPSP::Create()
{
	return new CAudioPluginPSP();
}


bool		CAudioPluginPSP::StartEmulation()
{
	return true;
}


void	CAudioPluginPSP::StopEmulation()
{
	mAudioOutput->StopAudio();
}

void	CAudioPluginPSP::DacrateChanged( int SystemType )
{
	auto type {(u32)((SystemType == ST_NTSC) ? VI_NTSC_CLOCK : VI_PAL_CLOCK)};
	auto dacrate {Memory_AI_GetRegister(AI_DACRATE_REG)};
	auto	frequency {type / (dacrate + 1)};

	mAudioOutput->SetFrequency( frequency );
}



void	CAudioPluginPSP::LenChanged()
{
	if( gAudioPluginEnabled > APM_DISABLED )
	{

		auto		address( Memory_AI_GetRegister(AI_DRAM_ADDR_REG) & 0xFFFFFF );
		auto		length(Memory_AI_GetRegister(AI_LEN_REG));

		mAudioOutput->AddBuffer( g_pu8RamBase + address, length );

	}
	else
	{
		mAudioOutput->StopAudio();
	}
}


u32		CAudioPluginPSP::ReadLength()
{
	return 0;
}

// struct SHLEStartJob : public SJob
// {
// 	SHLEStartJob()
// 	{
// 		 InitJob = nullptr;
// 		 DoJob = &DoHLEStartStatic;
// 		 FiniJob = &DoHLEFinishedStatic;
// 	}
//
// static int DoHLEStartStatic( SJob * arg )
// 	{
// 		 SHLEStartJob *  job( static_cast< SHLEStartJob * >( arg ) );
// 		 return job->DoHLEStart();
// 	}
//
// 	static int DoHLEFinishedStatic( SJob * arg )
// 	{
// 		 SHLEStartJob *  job( static_cast< SHLEStartJob * >( arg ) );
// 		 return job->DoHLEFinish();
// 	}
//
// 	int DoHLEStart()
// 	{
// 		 Audio_Ucode();
// 		 return 0;
// 	}
//
// 	int DoHLEFinish()
// 	{
// 		 CPU_AddEvent(RSP_AUDIO_INTR_CYCLES, CPU_EVENT_AUDIO);
// 		 return 0;
// 	}
// };


EProcessResult	CAudioPluginPSP::ProcessAList()
{
	Memory_SP_SetRegisterBits(SP_STATUS_REG, SP_STATUS_HALT);

	EProcessResult	result( PR_NOT_STARTED );

	switch( gAudioPluginEnabled )
	{
		case APM_DISABLED:
			result = PR_COMPLETED;
			break;
		case APM_ENABLED_ASYNC:
			{
				// SHLEStartJob	job;
				// gJobManager.AddJob( &job, sizeof( job ) );
			}
			result = PR_STARTED;
			break;
		case APM_ENABLED_SYNC:
			Audio_Ucode();
			result = PR_COMPLETED;
			break;
	}

	return result;
}


CAudioPlugin *		CreateAudioPlugin()
{
	return CAudioPluginPSP::Create();
}
