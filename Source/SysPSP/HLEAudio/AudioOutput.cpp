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
#include "AudioOutput.h"

#include <stdio.h>
#include <new>

#include <pspkernel.h>
#include <pspaudio.h>
#include <psptypes.h>

#include "Config/ConfigOptions.h"
#include "Debug/DBGConsole.h"
#include "HLEAudio/AudioBuffer.h"
#include "SysPSP/Utility/CacheUtil.h"
#include "SysPSP/Utility/JobManager.h"
#include "Utility/FramerateLimiter.h"
#include "Utility/Thread.h"

extern u32 gSoundSync;

const u32	DESIRED_OUTPUT_FREQUENCY {44100};

const u32	BUFFER_SIZE {1024 * 2};

const u32	PSP_NUM_SAMPLES {512};

// Global variables
SceUID bufferEmpty {};

auto sound_channel {PSP_AUDIO_NEXT_CHANNEL};
auto sound_volume {PSP_AUDIO_VOLUME_MAX};
u32 sound_status {0};

int pcmflip {0};

bool audio_open {false};

static AudioOutput * ac;

CAudioBuffer *mAudioBuffer;


int audioOutput(SceSize args, void *argp)
{
	uint16_t *playbuf = (uint16_t*)malloc(BUFFER_SIZE);

	SceUID	sound_channel = sceAudioChReserve(sound_channel, PSP_NUM_SAMPLES, PSP_AUDIO_FORMAT_STEREO);
	while(sound_status != 0xDEADBEEF)
	{
		mAudioBuffer->Drain( reinterpret_cast< Sample * >( playbuf ), PSP_NUM_SAMPLES );
		sceAudioOutputBlocking(sound_channel, sound_volume, playbuf);
	}
	sceAudioChRelease (sound_channel);
	sceKernelExitDeleteThread(0);
	return 0;
}

void AudioInit()
{
	sound_status = 0; // threads running

	SceUID audioThid = sceKernelCreateThread("audioOutput", audioOutput, 0x15, 0x1800, PSP_THREAD_ATTR_USER, NULL);
	sceKernelStartThread(audioThid, 0, NULL);
	audio_open = true;
}

void AudioExit()
{
	// Stop stream
	if (audio_open)
	{
		sound_status = 0xDEADBEEF;
		sceKernelDelayThread(100*1000);
	}

	audio_open = false;
}

AudioOutput::AudioOutput()
:	mAudioPlaying( false )
,	mFrequency( 44100 )
{
	void *mem = malloc( sizeof(CAudioBuffer) );
	mAudioBuffer = new( mem ) CAudioBuffer( BUFFER_SIZE );
}

AudioOutput::~AudioOutput( )
{
	StopAudio();

	mAudioBuffer->~CAudioBuffer();
	free( mAudioBuffer );
}

void AudioOutput::SetFrequency( u32 frequency )
{
	DBGConsole_Msg( 0, "Audio frequency: %d", frequency );
	mFrequency = frequency;
}

struct SAddSamplesJob : public SJob
{
	CAudioBuffer *		mBuffer;
	const Sample *		mSamples;
	u32					mNumSamples;
	u32					mFrequency;
	u32					mOutputFreq;

	SAddSamplesJob( CAudioBuffer * buffer, const Sample * samples, u32 num_samples, u32 frequency, u32 output_freq )
		:	mBuffer( buffer )
		,	mSamples( samples )
		,	mNumSamples( num_samples )
		,	mFrequency( frequency )
		,	mOutputFreq( output_freq )
	{
		InitJob = NULL;
		DoJob = &DoAddSamplesStatic;
		FiniJob = &DoJobComplete;
	}

	static int DoAddSamplesStatic( SJob * arg )
	{
		SAddSamplesJob *	job( static_cast< SAddSamplesJob * >( arg ) );
		return job->DoAddSamples();
	}

	static int DoJobComplete( SJob * arg )
	{
		SAddSamplesJob *	job( static_cast< SAddSamplesJob * >( arg ) );
		return job->DoJobComplete();
	}

	int DoAddSamples()
	{
		mBuffer->AddSamples( mSamples, mNumSamples, mFrequency, mOutputFreq );
		return 0;
	}

	int DoJobComplete()
	{
	return 0;
	}

};

void AudioOutput::AddBuffer( u8 *start, u32 length )
{
	if (length == 0)
		return;

	if (!mAudioPlaying)
		StartAudio();

	u32 num_samples {length / sizeof( Sample )};

	switch( gAudioPluginEnabled )
	{
	case APM_DISABLED:
		break;

	case APM_ENABLED_ASYNC:
		{
		SAddSamplesJob	job( mAudioBuffer, reinterpret_cast< const Sample * >( start ), num_samples, mFrequency, 44100 );

	 gJobManager.AddJob( &job, sizeof( job ) );
		}
		break;

	case APM_ENABLED_SYNC:
		{
			mAudioBuffer->AddSamples( reinterpret_cast< const Sample * >( start ), num_samples, mFrequency, 44100 );
		}
		break;
	}
}

void AudioOutput::StartAudio()
{
	if (mAudioPlaying)
		return;

	mAudioPlaying = true;

	ac = this;

	AudioInit();
}

void AudioOutput::StopAudio()
{
	if (!mAudioPlaying)
		return;

	mAudioPlaying = false;

	AudioExit();
}
