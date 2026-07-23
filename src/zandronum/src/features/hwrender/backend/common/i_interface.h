// [rc4l] Minimal stand-in for UZDoom's i_interface.h, which we cannot vendor because it redefines WadStuff (owned by our d_main.h).
#pragma once

#include "zstring.h"

// [rc4l] The ported backend only ever null-checks and calls these three callbacks; leaving them null selects the engine's default behaviour.
struct SystemCallbacks
{
	bool (*DisableTextureFilter)() = nullptr;
	bool (*DisableAnisotropicFiltering)() = nullptr;
	FString (*GetLocationDescription)() = nullptr;
};

extern SystemCallbacks sysCallbacks;
