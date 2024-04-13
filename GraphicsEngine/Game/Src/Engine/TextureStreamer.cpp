#include "pch_game.h"
#include "Engine/TextureStreamer.h"

#include <stb_image.h>

#include "JLib/ArrayUtils.h"
#include "JLib/LinkedListUtils.h"
#include "JLib/VectorUtils.h"

namespace game
{
	jv::ge::Resource TextureStreamer::Get(const uint32_t i)
	{
		if (i == -1)
			return nullptr;

		auto& id = _ids[i];
		assert(id.path);
		if (id.resource)
		{
			id.inactiveCount = 0;
			return id.resource->resource;
		}

		// Find a new resource to link it to.
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

		// Make new resource if no available one exists.
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

		int texWidth, texHeight, texChannels2;
		stbi_uc* pixels = stbi_load(id.path, &texWidth, &texHeight, &texChannels2, STBI_rgb_alpha);
		glm::ivec2 resolution{ texWidth, texHeight };
		jv::ge::FillImage(id.resource->resource, pixels, &resolution);
		stbi_image_free(pixels);
		id.resource->active = true;
		id.inactiveCount = 0;
		return id.resource->resource;
	}

	uint32_t TextureStreamer::DefineTexturePath(const char* path)
	{
		assert(_idCount < _ids.length);
		auto& id = _ids[_idCount];
		id.path = path;
		return _idCount++;
	}

	void TextureStreamer::Update() const
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

	TextureStreamer TextureStreamer::Create(jv::Arena& arena, const uint32_t poolChunkSize, const uint32_t idCount, 
		const jv::ge::ImageCreateInfo& imageCreateInfo)
	{
		TextureStreamer texturePool{};
		texturePool._arena = &arena;
		texturePool._scope = arena.CreateScope();
		texturePool._imageCreateInfo = imageCreateInfo;
		texturePool._ids = jv::CreateArray<Id>(arena, idCount);
		texturePool._poolChunkSize = poolChunkSize;
		for (auto& id : texturePool._ids)
			id = {};
		return texturePool;
	}

	void TextureStreamer::Destroy(const TextureStreamer& pool)
	{
		pool._arena->DestroyScope(pool._scope);
	}
}
