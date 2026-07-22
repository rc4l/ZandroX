// [rc4l] Shim for UZDoom's printf.h: Printf/DPrintf are declared in our doomtype.h, but their DPrintf carries a verbosity level ours has no concept of.
#pragma once

#include <stdarg.h>

#include "doomtype.h"
#include "c_console.h"
#include "c_cvars.h"
#include "v_text.h"

// [rc4l] UZDoom's debug-message levels; we keep the names so ported code reads unchanged.
enum
{
	DMSG_OFF,
	DMSG_ERROR,
	DMSG_WARNING,
	DMSG_NOTIFY,
	DMSG_SPAMMY,
};

EXTERN_CVAR (Bool, developer)

// [rc4l] Overloads our level-less DPrintf; the level is discarded because our console has no per-level filtering.
inline int DPrintf (int, const char *format, ...)
{
	if (!developer) return 0;
	va_list argptr;
	va_start (argptr, format);
	int ret = VPrintf (PRINT_HIGH, format, argptr);
	va_end (argptr);
	return ret;
}
