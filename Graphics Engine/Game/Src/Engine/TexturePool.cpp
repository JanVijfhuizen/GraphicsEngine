#include "pch_game.h"
#include "Engine/TexturePool.h"

#include <stb_image.h>

#include "JLib/ArrayUtils.h"
#include "JLib/LinkedListUtils.h"
#include "JLib/VectorUtils.h"

namespace game
{
	jv::ge::Resource TexturePool::Get(const uint32_t i)
	{
		auto& id = _ids[i];
		assert(id.path);
		if (id.resource)
			return id.resource->resource;

		for (auto& pool : _pools)
		{
			bool found = false;
			for (auto& resource : pool)
			{
				if (!resource.active)
				{
					id.resource = &resource;
					found = true;
					break;
				}
			}
			if (found)
				break;
		}

		if(!id.resource)
		{
			Resource* resource = nullptr;
			for (auto& pool : _pools)
			{
				if (pool.count != pool.length)
				{
					resource = &pool.Add();
					break;
				}
			}
			if (!resource)
			{
				auto& pool = Add(*_arena, _pools);
				pool = jv::CreateVector<Resource>(*_arena, _poolChunkSize);
				resource = &pool.Add();
			}
			
			resource->resource = AddImage(_imageCreateInfo);
			id.resource = resource;
		}

		id.resource->active = true;

		int texWidth, texHeight, texChannels2;
		stbi_uc* pixels = stbi_load(id.path, &texWidth, &texHeight, &texChannels2, STBI_rgb_alpha);
		jv::ge::FillImage(id.resource->resource, pixels);
		stbi_image_free(pixels);

		id.inactiveCount = 0;
		return id.resource->resource;
	}

	uint32_t TexturePool::DefineTexturePath(const char* path)
	{
		assert(_idCount < _ids.length);
		auto& id = _ids[_idCount];
		id.path = path;
		return _idCount++;
	}

	void TexturePool::Update() const
	{
		const uint32_t frameCount = jv::ge::GetFrameCount();

		for (uint32_t i = 0; i < _idCount; ++i)
		{
			auto& id = _ids[i];
			if (!id.resource)
				continue;

			++id.inactiveCount;
			if(id.inactiveCount > frameCount)
			{
				id.resource->active = false;
				id.resource = nullptr;
			}
		}
	}

	TexturePool TexturePool::Create(jv::Arena& arena, const uint32_t poolChunkSize, const uint32_t idCount, const jv::ge::ImageCreateInfo& imageCreateInfo)
	{
		TexturePool texturePool{};
		texturePool._arena = &arena;
		texturePool._scope = arena.CreateScope();
		texturePool._imageCreateInfo = imageCreateInfo;
		texturePool._ids = jv::CreateArray<Id>(arena, idCount);
		texturePool._poolChunkSize = poolChunkSize;
		for (auto& id : texturePool._ids)
			id = {};
		return texturePool;
	}

	void TexturePool::Destroy(const TexturePool& pool)
	{
		pool._arena->DestroyScope(pool._scope);
	}
}
