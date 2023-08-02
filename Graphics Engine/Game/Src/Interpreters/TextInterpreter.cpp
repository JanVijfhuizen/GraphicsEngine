#include "pch_game.h"
#include "Interpreters/TextInterpreter.h"

#include "JLib/Math.h"
#include "Tasks/RenderTask.h"

namespace game
{
	const char* TextInterpreter::IntToConstCharPtr(const uint32_t i, jv::Arena& arena)
	{
		char s[9 + sizeof(char)];
		const uint32_t size = GetNumberOfDigits(i);
		const auto ptr = static_cast<char*>(arena.Alloc(size + 1));
		sprintf_s(s, "%d", i);
		memcpy(ptr, s, size + 1);
		return ptr;
	}

	void TextInterpreter::OnStart(const TextInterpreterCreateInfo& createInfo, const EngineMemory& memory)
	{
		_createInfo = createInfo;
	}

	void TextInterpreter::OnUpdate(const EngineMemory& memory, const jv::LinkedList<jv::Vector<TextTask>>& tasks)
	{
		const float symbolPctSize = static_cast<float>(_createInfo.symbolSize) / static_cast<float>(_createInfo.atlasResolution.x);
		
		PixelPerfectRenderTask task{};

		const auto charScale = _createInfo.numberAtlasTexture.resolution / glm::ivec2(10, 1);

		for (const auto& batch : tasks)
			for (const auto& job : batch)
			{
				assert(job.text);

				const auto len = static_cast<uint32_t>(strlen(job.text));

				task.position = job.position;
				task.scale = charScale;

				uint32_t lineLength = 0;
				uint32_t nextLineStart = 0;

				for (uint32_t i = 0; i < len; ++i)
				{
					if (nextLineStart == i)
					{
						uint32_t previousBreak = i;

						// Define line length.
						for (uint32_t j = i + 1; j < len; ++j)
						{
							const auto& c = job.text[j];
							if (c == ' ')
								previousBreak = j;
							if (j - i >= job.lineLength && previousBreak != i)
							{
								nextLineStart = previousBreak;
								break;
							}
						}
						
						lineLength = 0;

						if(i != 0)
						{
							task.position.y -= charScale.y;
							task.position.x = job.position.x - job.spacing - _createInfo.spacing - _createInfo.symbolSize;
						}
					}

					auto c = job.text[i];
					if (i >= job.maxLength)
					{
						c = '.';
						if (!job.drawDotsOnMaxLengthReached || i >= job.maxLength + 3)
							break;
					}

					if (c != ' ')
					{
						const bool isSymbol = c < '0';
						const bool isInteger = !isSymbol && c < 'a';

						// Assert if it's a valid character.
						assert(isInteger ? c >= '0' && c <= '9' : isSymbol ? c >= ',' && c <= '/' : c >= 'a' && c <= 'z');
						const auto position = c - (isInteger ? '0' : isSymbol ? ',' : 'a');

						auto subTexture = isInteger ? _createInfo.numberAtlasTexture.subTexture : isSymbol ? 
							_createInfo.symbolAtlasTexture.subTexture : _createInfo.alphabetAtlasTexture.subTexture;
						subTexture.lTop.x += symbolPctSize * static_cast<float>(position);
						subTexture.rBot.x = subTexture.lTop.x + symbolPctSize;

						task.subTexture = subTexture;

						_createInfo.renderTasks->Push(task);
					}

					task.position.x += charScale.x + job.spacing + _createInfo.spacing;
					lineLength++;
				}
			}
	}

	void TextInterpreter::OnExit(const EngineMemory& memory)
	{
	}
}
