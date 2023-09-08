#include "pch.h"
#include "JLib/FileLoader.h"
#include "JLib/ArrayUtils.h"
#include <fstream>

namespace jv::file
{
	Array<char> Load(Arena& arena, const char* path)
	{
		std::ifstream file(path, std::ios::ate | std::ios::binary);
		assert(file.is_open());

		// Dump contents in the buffer.
		const auto fileSize = static_cast<uint32_t>(file.tellg());
		const auto buffer = CreateArray<char>(arena, fileSize);

		file.seekg(0);
		file.read(buffer.ptr, fileSize);
		file.close();

		return buffer;
	}
}
