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
#include <stdio.h>
#include <new>

#include <kernel.h>
#include <audsrv.h>
#include <malloc.h>

#include "Plugins/AudioPlugin.h"
#include "HLEAudio/audiohle.h"

#include "Config/ConfigOptions.h"
#include "Core/CPU.h"
#include "Core/Interrupt.h"
#include "Core/Memory.h"
#include "Core/ROM.h"
#include "Core/RSP_HLE.h"
#include "Debug/DBGConsole.h"
#include "HLEAudio/AudioBuffer.h"
#include "Utility/FramerateLimiter.h"
#include "Utility/Thread.h"

#define RSP_AUDIO_INTR_CYCLES     1
extern u32 gSoundSync;

static const u32	kOutputFrequency = 44100;
static const u32	MAX_OUTPUT_FREQUENCY = kOutputFrequency * 4;


static bool audio_open = false;


// Large kAudioBufferSize creates huge delay on sound //Corn
static const u32	kAudioBufferSize = 1024 * 4; // OSX uses a circular buffer length, 1024 * 1024


class AudioPluginPS2 : public CAudioPlugin
{
public:

	AudioPluginPS2();
	virtual ~AudioPluginPS2();
	virtual bool			StartEmulation();
	virtual void			StopEmulation();

	virtual void			DacrateChanged(int system_type);
	virtual void			LenChanged();
	virtual u32				ReadLength() { return 0; }
	virtual EProcessResult	ProcessAList();
	virtual void			Update(bool wait);

	//virtual void SetFrequency(u32 frequency);
	virtual void AddBuffer(u8* start, u32 length);
	virtual void FillBuffer(Sample* buffer, u32 num_samples);

	virtual void StopAudio();
	virtual void StartAudio();
	static u32 AudioThread(void* arg);
public:
	CAudioBuffer* mAudioBufferUncached;
	ThreadHandle mAudioThread;
	s32 mSemaphore;
private:
	CAudioBuffer* mAudioBuffer;
	bool mKeepRunning;
	bool mExitAudioThread;
	u32 mFrequency;
//	ThreadHandle mAudioThread;
	//s32 mSemaphore;
	//	u32 mBufferLenMs;
};

static AudioPluginPS2* ac;

void AudioPluginPS2::FillBuffer(Sample* buffer, u32 num_samples)
{
	PollSema(mSemaphore);

	mAudioBufferUncached->Drain(buffer, num_samples);

	SignalSema(mSemaphore);
}


EAudioPluginMode gAudioPluginEnabled(APM_DISABLED);


AudioPluginPS2::AudioPluginPS2()
	:mKeepRunning(false)
	//: mAudioBuffer( kAudioBufferSize )
	, mFrequency(44100)
	, mAudioThread ( kInvalidThreadHandle )
	//, mKeepRunning( false )
	//, mBufferLenMs ( 0 )
{
	ee_sema_t sema;
	sema.init_count = 1;
	sema.max_count = 1;
	sema.option = 0;
	mSemaphore = CreateSema(&sema);

	// Allocate audio buffer with malloc_64 to avoid cached/uncached aliasing
	void* mem = memalign(128, sizeof(CAudioBuffer));
	mAudioBuffer = new(mem) CAudioBuffer(kAudioBufferSize);
	mAudioBufferUncached = (CAudioBuffer*)MAKE_UNCACHED_PTR(mem);
	// Ideally we could just invalidate this range?
	//SyncDCache(mAudioBuffer, mAudioBuffer + sizeof(CAudioBuffer));
}

AudioPluginPS2::~AudioPluginPS2()
{
	mAudioBuffer->~CAudioBuffer();
	free(mAudioBuffer);
	DeleteSema(mSemaphore);
	audsrv_quit();
}

bool	AudioPluginPS2::StartEmulation()
{
	return true;
}


void	AudioPluginPS2::StopEmulation()
{
	Audio_Reset();
	StopAudio();
	DeleteSema(mSemaphore);
	audsrv_quit();
}

void	AudioPluginPS2::DacrateChanged(int system_type)
{
	u32 clock = (system_type == ST_NTSC) ? VI_NTSC_CLOCK : VI_PAL_CLOCK;
	u32 dacrate = Memory_AI_GetRegister(AI_DACRATE_REG);
	u32 frequency = clock / (dacrate + 1);

#ifdef DAEDALUS_DEBUG_CONSOLE
	DBGConsole_Msg(0, "Audio frequency: %d", frequency);
#endif
	mFrequency = frequency;
}


void	AudioPluginPS2::LenChanged()
{
	if (gAudioPluginEnabled > APM_DISABLED)
	{
		u32 address = Memory_AI_GetRegister(AI_DRAM_ADDR_REG) & 0xFFFFFF;
		u32	length = Memory_AI_GetRegister(AI_LEN_REG);

		AddBuffer(g_pu8RamBase + address, length);
	}
	else
	{
		StopAudio();
	}
}


EProcessResult	AudioPluginPS2::ProcessAList()
{
	Memory_SP_SetRegisterBits(SP_STATUS_REG, SP_STATUS_HALT);

	EProcessResult	result = PR_NOT_STARTED;

	switch (gAudioPluginEnabled)
	{
		case APM_DISABLED:
			result = PR_COMPLETED;
			break;
		case APM_ENABLED_ASYNC:
			DAEDALUS_ERROR("Async audio is unimplemented");
			Audio_Ucode();
			result = PR_COMPLETED;
			break;
		case APM_ENABLED_SYNC:
			Audio_Ucode();
			result = PR_COMPLETED;
			break;
	}

	return result;
}

static char audio_buf[kAudioBufferSize];

int audioCallback(void* userdata)
{
	AudioPluginPS2* ac(reinterpret_cast<AudioPluginPS2*>(userdata));

	printf("callback\n");

	//ac->FillBuffer(reinterpret_cast<Sample*>(audio_buf), kAudioBufferSize/sizeof(Sample));
	if (ac->mAudioBufferUncached->GetNumBufferedSamples() > 0)
		iWakeupThread(ac->mAudioThread);

	return 0;
}

u32 AudioPluginPS2::AudioThread(void* arg)
{
	AudioPluginPS2* plugin = static_cast<AudioPluginPS2*>(arg);
	int ret;

	while (1)
	{
		printf("thread\n");
		
		SleepThread();

		WaitSema(plugin->mSemaphore);
		ret = plugin->mAudioBufferUncached->Drain(reinterpret_cast<Sample*>(audio_buf), kAudioBufferSize / sizeof(Sample));
		audsrv_wait_audio(ret * sizeof(Sample));
		audsrv_play_audio(audio_buf, ret * sizeof(Sample));
		SignalSema(plugin->mSemaphore);
	}
}

void AudioPluginPS2::Update(bool Wait)
{
	if (audio_open)
	{
		int ret = mAudioBufferUncached->Drain(reinterpret_cast<Sample*>(audio_buf), kAudioBufferSize / sizeof(Sample));
		//printf("ret %d %d\n", ret, ret * sizeof(Sample));
		audsrv_wait_audio(ret * sizeof(Sample));
		audsrv_play_audio(audio_buf, ret * sizeof(Sample));
	}
}

void AudioPluginPS2::StartAudio()
{
	struct audsrv_fmt_t format;
	int ret;

	if (mKeepRunning)
		return;

	mKeepRunning = true;

	ac = this;

	ret = audsrv_init();

	if (ret != 0) {
		printf("Failed to initialize audsrv: %s\n", audsrv_get_error_string());
		mKeepRunning = false;
		audio_open = false;
		return;
	}

	format.bits = 16;
	format.freq = kOutputFrequency;
	format.channels = 2;

	ret = audsrv_set_format(&format);

	if (ret != 0) {
		printf("Set format returned: %s\n", audsrv_get_error_string());
		mKeepRunning = false;
		audio_open = false;
		return;
	}

	audsrv_set_volume(MAX_VOLUME);

	/*mAudioThread = dCreateThread("Audio", &AudioThread, this);

	ret = audsrv_on_fillbuf(kAudioBufferSize, audioCallback, this);
	if (ret != AUDSRV_ERR_NOERROR)
	{
		printf("audsrv_on_fillbuf failed with err=%d\n", ret);
		mKeepRunning = false;
		audio_open = false;
		return;
	}*/

	// Everything OK
	audio_open = true;
}

void AudioPluginPS2::AddBuffer(u8* start, u32 length)
{
	if (length == 0)
		return;

	if (!mKeepRunning)
		StartAudio();

	u32 num_samples = length / sizeof(Sample);

	switch (gAudioPluginEnabled)
	{
		case APM_DISABLED:
			break;

		case APM_ENABLED_ASYNC:
			break;

		case APM_ENABLED_SYNC:
			mAudioBufferUncached->AddSamples(reinterpret_cast<const Sample*>(start), num_samples, mFrequency, kOutputFrequency);
			break;
	}

	/*
	u32 remaining_samples = mAudioBuffer.GetNumBufferedSamples();
	mBufferLenMs = (1000 * remaining_samples) / kOutputFrequency);
	float ms = (float) num_samples * 1000.f / (float)mFrequency;
	#ifdef DAEDALUS_DEBUG_CONSOLE
	DPF_AUDIO("Queuing %d samples @%dHz - %.2fms - bufferlen now %d\n", num_samples, mFrequency, ms, mBufferLenMs);
	#endif
	*/
}

void AudioPluginPS2::StopAudio()
{
	if (!mKeepRunning)
		return;

	if (audio_open)
		audsrv_stop_audio();

	mKeepRunning = false;
	audio_open = false;
}

CAudioPlugin* CreateAudioPlugin()
{
	return new AudioPluginPS2();
}
