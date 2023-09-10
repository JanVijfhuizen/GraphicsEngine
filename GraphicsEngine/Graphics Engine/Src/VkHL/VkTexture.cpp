#include "pch.h"
#include "VkHL/VkTexture.h"
#include <fstream>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include "JLib/ArrayUtils.h"
#include "JLib/PackingFFDH.h"
#include "VkHL/VkSubTexture.h"

namespace jv::vk
{
	void GenerateTextureAtlas(Arena& arena, Arena& tempArena, const Array<const char*>& filePaths,
		const char* imageFilePath, const char* metaFilePath, const uint32_t texChannels)
	{
		const auto _ = tempArena.CreateScope();
		const auto shapes = CreateArray<glm::ivec2>(tempArena, filePaths.length);

		{
			uint32_t i = 0;
			for (const auto& path : filePaths)
			{
				// Load pixels.
				int texWidth, texHeight, texChannels2;
				stbi_uc* pixels = stbi_load(path, &texWidth, &texHeight, &texChannels2, STBI_rgb_alpha);
				assert(pixels);
				assert(texChannels == texChannels2);
				shapes[i++] = glm::ivec2(texWidth, texHeight);

				// Free pixels.
				stbi_image_free(pixels);
			}
		}

		glm::ivec2 area;
		const auto positions = Pack(arena, tempArena, shapes, area);
		const auto atlasPixels = CreateArray<stbi_uc>(tempArena, static_cast<uint32_t>(area.x) * area.y * texChannels);

		const auto rowLength = static_cast<uint32_t>(area.x) * texChannels;

		for (uint32_t i = 0; i < filePaths.length; ++i)
		{
			const auto& path = filePaths[i];
			const auto& position = positions[i];

			// Load pixels.
			int texWidth, texHeight, texChannels2;
			stbi_uc* pixels = stbi_load(path, &texWidth, &texHeight, &texChannels2, STBI_rgb_alpha);
			assert(pixels);
			assert(texChannels == texChannels2);

			const uint32_t width = static_cast<uint32_t>(texWidth) * texChannels;
			const uint32_t start = position.y * rowLength + static_cast<uint32_t>(position.x) * texChannels;
			for (uint32_t y = 0; y < texHeight; ++y)
				memcpy(&atlasPixels[start + rowLength * y], &pixels[static_cast<uint32_t>(texWidth) * texChannels * y], width);

			// Free pixels.
			stbi_image_free(pixels);
		}

		stbi_write_png(imageFilePath, area.x, area.y, texChannels, atlasPixels.ptr, 0);

		std::ofstream outfile;
		outfile.open(metaFilePath);

		for (uint32_t i = 0; i < filePaths.length; ++i)
		{
			const auto& shape = shapes[i];
			const auto& position = positions[i];

			const auto lTop = glm::vec2(position) / glm::vec2(area);
			const auto rBot = lTop + glm::vec2(shape) / glm::vec2(area);

			outfile << lTop.x << std::endl;
			outfile << lTop.y << std::endl;

			outfile << rBot.x << std::endl;
			outfile << rBot.y << std::endl;
		}

		outfile.close();
	}

	Image LoadTexture(Arena& arena, const FreeArena& freeArena, const App& app, const ImageCreateInfo& info, const char* imageFilePath)
	{
		// Load pixels.
		int texWidth, texHeight, texChannels;
		stbi_uc* pixels = stbi_load(imageFilePath, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
		assert(pixels);

		assert(info.usageFlags | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
		auto cpyInfo = info;
		cpyInfo.resolution = glm::ivec3(texWidth, texHeight, texChannels);
		auto image = Image::Create(arena, freeArena, app, cpyInfo);
		image.FillImage(arena, freeArena, app, pixels, info.cmd);

		// Free pixels.
		stbi_image_free(pixels);

		return image;
	}

	void FillTexture(Arena& arena, const FreeArena& freeArena, const App& app, Image& image, const char* imageFilePath, const VkCommandBuffer cmd)
	{
		// Load pixels.
		int texWidth, texHeight, texChannels;
		stbi_uc* pixels = stbi_load(imageFilePath, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
		assert(pixels);

		assert(image.usageFlags | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
		image.FillImage(arena, freeArena, app, pixels, cmd);

		// Free pixels.
		stbi_image_free(pixels);
	}

	Array<SubTexture> LoadTextureAtlasMetaData(Arena& arena, const char* metaFilePath)
	{
		std::ifstream inFile;
		inFile.open(metaFilePath);

		const uint32_t lineCount = std::count(
			std::istreambuf_iterator(inFile), std::istreambuf_iterator<char>(), '\n');
		inFile.seekg(0, std::ios::beg);

		const auto metaData = CreateArray<SubTexture>(arena, lineCount / 4);

		glm::vec2 lTop;
		glm::vec2 rBot;
		uint32_t i = 0;
		while (inFile >> lTop.x >> lTop.y >> rBot.x >> rBot.y)
		{
			auto& instance = metaData[i++];
			instance.lTop = lTop;
			instance.rBot = rBot;
		}

		return metaData;
	}
}
