#include "Base/Daedalus.h"
#include "HLEAudio/AudioPlugin.h"
#include "Config/ConfigOptions.h"

CAudioPlugin* gAudioPlugin = nullptr;
EAudioMode gAudioPluginMode = AM_DISABLED;


bool CreateAudioPlugin()
{
	return true;
}

void DestroyAudioPlugin()
{
	
}
