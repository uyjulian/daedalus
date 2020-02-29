/*
Copyright (C) 2005 StrmnNrmn

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
#include "Utility/Thread.h"

#include <kernel.h>
#include <stdio.h>

#define MAX_THREAD 5
#define STACK_SIZE 0x12000

extern void* _gp;

typedef struct {
	int thid;
	ee_thread_t thread;
} ps2_thread;

static ps2_thread th[MAX_THREAD];
static bool th_inited = false;

static const int	gThreadPriorities[ TP_NUM_PRIORITIES ] =
{
	63,		// TP_LOW
	62,		// TP_NORMAL
	61,		// TP_HIGH
	60,		// TP_TIME_CRITICAL
};

const s32	kInvalidThreadHandle = -1;

struct SDaedThreadDetails
{
	SDaedThreadDetails( DaedThread function, void * argument )
		:	ThreadFunction( function )
		,	Argument( argument )
	{
	}

	DaedThread		ThreadFunction;
	void *			Argument;
};

// The real thread is passed in as an argument. We call it and return the result
static int StartThreadFunc( void *argp )
{
	SDaedThreadDetails * thread_details( static_cast< SDaedThreadDetails * >( argp ) );

	return thread_details->ThreadFunction( thread_details->Argument );
}

s32		DCreateThread( const char * name, DaedThread function, void * argument )
{
	int i;

	if (!th_inited) {
		for (i = 0; i < MAX_THREAD; i++) {
			th[i].thid = kInvalidThreadHandle;
		}

		th_inited = true;
	}

	for (i = 0; i < MAX_THREAD; i++) {
		if (th[i].thid == kInvalidThreadHandle)
			break;
	}

	if (i == MAX_THREAD)
		return kInvalidThreadHandle;

	th[i].thread.func = (void*)StartThreadFunc;
	th[i].thread.stack = memalign(16, STACK_SIZE);
	th[i].thread.stack_size = STACK_SIZE;
	th[i].thread.gp_reg = _gp;
	th[i].thread.initial_priority = gThreadPriorities[TP_NORMAL];
	th[i].thid = ::CreateThread(&th[i].thread);

	if(th[i].thid >= 0)
	{
		SDaedThreadDetails	thread_details( function, argument );
		::StartThread(th[i].thid, &thread_details );
		return th[i].thid;
	}

	return kInvalidThreadHandle;
}

void SetThreadPriority( s32 handle, EThreadPriority pri )
{
	if(handle != kInvalidThreadHandle)
	{
		::ChangeThreadPriority( handle, gThreadPriorities[ pri ] );
	}
}

void ReleaseThreadHandle( s32 handle )
{
	int i;

	for (i = 0; i < MAX_THREAD; i++) {
		if (th[i].thid == handle)
			break;
	}

	if (i < MAX_THREAD)
	{
		::DeleteThread(handle);
		th[i].thid = kInvalidThreadHandle;
		free(th[i].thread.stack);
	}
}

// Wait the specified time for the thread to finish.
// Returns false if the thread didn't terminate
bool JoinThread( s32 handle, s32 timeout )
{
	int ret = ::TerminateThread(handle);

	return (ret >= 0);
}

void ThreadSleepMs( u32 ms )
{
	//sceKernelDelayThread( ms * 1000 );		// Delay is specified in microseconds
}

void ThreadSleepTicks( u32 ticks )
{
	//sceKernelDelayThread( ticks );		// Delay is specified in ticks
}

void ThreadYield()
{
	//sceKernelDelayThread( 1 );				// Is 0 valid?
}
