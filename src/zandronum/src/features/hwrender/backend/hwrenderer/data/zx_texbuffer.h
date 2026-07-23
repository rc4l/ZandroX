// [rc4l] FTextureBuffer and the CTF_ flags the ported backend's texture uploader references (see issue #4).
#pragma once

#include <stdint.h>

enum ECreateTexBufferFlags
{
	CTF_Expand = 1,
	CTF_Upscale = 2,
	CTF_CreateMask = 3,
	CTF_Indexed = 4,
	CTF_CheckOnly = 8,
	CTF_ProcessData = 16,
};

struct FTextureBuffer
{
	uint8_t *mBuffer = nullptr;
	bool mFreeBuffer = true;
	int mWidth = 0;
	int mHeight = 0;
	uint64_t mContentId = 0;

	FTextureBuffer() = default;

	~FTextureBuffer()
	{
		if (mBuffer && mFreeBuffer) delete[] mBuffer;
	}

	FTextureBuffer(const FTextureBuffer &) = delete;
	FTextureBuffer &operator=(const FTextureBuffer &) = delete;

	FTextureBuffer(FTextureBuffer &&other) noexcept
		: mBuffer(other.mBuffer), mFreeBuffer(other.mFreeBuffer), mWidth(other.mWidth), mHeight(other.mHeight), mContentId(other.mContentId)
	{
		other.mBuffer = nullptr;
	}

	FTextureBuffer &operator=(FTextureBuffer &&other) noexcept
	{
		if (this != &other)
		{
			if (mBuffer && mFreeBuffer) delete[] mBuffer;
			mBuffer = other.mBuffer; mFreeBuffer = other.mFreeBuffer;
			mWidth = other.mWidth; mHeight = other.mHeight; mContentId = other.mContentId;
			other.mBuffer = nullptr;
		}
		return *this;
	}
};
