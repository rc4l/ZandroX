// [rc4l] glcycle_t shim: UZDoom's stats.h derives it from cycle_t with an active toggle; ours has cycle_t only.
#pragma once

#include "stats.h"

class glcycle_t : public cycle_t
{
public:
	static bool active;
	void Clock() { if (active) cycle_t::Clock(); }
	void Unclock() { if (active) cycle_t::Unclock(); }
};
