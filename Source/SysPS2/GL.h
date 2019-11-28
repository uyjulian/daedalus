#ifndef SYSPS2_GL_H_
#define SYSPS2_GL_H_

#include "Utility/DaedalusTypes.h"
#include <gsKit.h>

// FIXME: burn all of this with fire.

void sceGuFog(float mn, float mx, u32 col);

enum EGuTextureWrapMode
{
	GU_CLAMP			= GS_CMODE_CLAMP,
	GU_REPEAT			= GS_CMODE_REPEAT,
};

enum EGuMatrixType
{
	GU_PROJECTION		= 0,
};

struct ScePspFMatrix4
{
	float m[16];
};

void sceGuSetMatrix(EGuMatrixType type, const ScePspFMatrix4 * mtx);


#endif // SYSPS2_GL_H_
