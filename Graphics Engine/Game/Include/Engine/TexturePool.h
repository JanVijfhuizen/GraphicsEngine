#pragma once
#include "GE/AtlasGenerator.h"
#include "GE/GraphicsEngine.h"
#include "JLib/LinkedList.h"
#include "JLib/Vector.h"

namespace game
{
	class TexturePool final
	{
	public:
		[[nodiscard]] jv::ge::Resource Get(uint32_t i);
		uint32_t DefineTexturePath(const char* path);
		void Update() const;

		static TexturePool Create(jv::Arena& arena, uint32_t poolChunkSize, uint32_t idCount, const jv::ge::ImageCreateInfo& imageCreateInfo);
		static void Destroy(const TexturePool& pool);

	private:
		struct Resource final
		{
			jv::ge::Resource resource = nullptr;
			bool active = false;
		};

		struct Id final
		{
			Resource* resource = nullptr;
			const char* path = nullptr;
			uint32_t inactiveCount = 0;
		};

		jv::Arena* _arena;
		uint64_t _scope;
		uint32_t _poolChunkSize;
		uint32_t _idCount = 0;
		jv::ge::ImageCreateInfo _imageCreateInfo{};
		jv::LinkedList<jv::Vector<Resource>> _pools{};
		jv::Array<Id> _ids;
	};
}
