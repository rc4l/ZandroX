// [rc4l] Just the layer struct, split out so Zandronum files can include it without hw_material.h's TM_* enums, which collide with our gl_interface.h.
#pragma once

class FTexture;

struct MaterialLayerInfo
{
	FTexture *layerTexture;
	int scaleFlags;
};
