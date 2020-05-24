#include "Base/Daedalus.h"
#include "HLEAudio/HLEAudio.h"
#include "Config/ConfigOptions.h"

CAudioPlugin* gHLEAudio = nullptr;
EAudioMode gAudioMode = AM_DISABLED;


bool CreateAudioPlugin()
{
	return true;
}

void DestroyAudioPlugin()
{
	
}
