
// [rc4l] hwrender: only referenced by the ported uploader's create-branch, which FMaterial::GetLayer short-circuits by adopting an existing GL id (issue #4).
#include "features/hwrender/backend/hwrenderer/data/zx_texbuffer.h"
FTextureBuffer FTexture::CreateTexBuffer(int translation, int flags)
{
	return FTextureBuffer();
}
