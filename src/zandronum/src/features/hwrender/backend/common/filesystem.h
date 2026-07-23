// [rc4l] Shim for UZDoom's filesystem.h: the ported backend only needs lump lookup by full name plus lump-to-string, both of which our FWadCollection already provides.
#pragma once

#include "w_wad.h"
#include "zstring.h"

// [rc4l] UZDoom renamed ZDoom's Wads collection to fileSystem; the backend uses the new name.
#define fileSystem Wads

// [rc4l] Their second CheckNumForFullName argument picks which container to search, which they use to ignore user overrides of core shaders; ours has no equivalent, so a mod-supplied shader of the same name would win.
FString GetStringFromLump(int lump, bool zerotruncate = true);
