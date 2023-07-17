#include "pch_game.h"
#include "Interpreters/TextInterpreter.h"

#include "JLib/Math.h"
#include "Tasks/RenderTask.h"

namespace game
{
	const char* TextInterpreter::IntToConstCharPtr(const uint32_t i, jv::Arena& arena)
	{
		constexpr uint32_t size = (sizeof i * CHAR_BIT + 2) / 3 + 2;
		const auto ptr = static_cast<char*>(arena.Alloc(size));
		char s[size];
		sprintf_s(s, "%d", i);
		memcpy(ptr, s, size);
		return ptr;
	}

	void TextInterpreter::OnStart(const TextInterpreterCreateInfo& createInfo, const EngineMemory& memory)
	{
		_createInfo = createInfo;
	}

	void TextInterpreter::OnUpdate(const EngineMemory& memory, const jv::LinkedList<jv::Vector<TextTask>>& tasks)
	{
		const float symbolPctSize = static_cast<float>(_createInfo.symbolSize) / static_cast<float>(_createInfo.atlasResolution.x);
		
		RenderTask task{};

		for (const auto& batch : tasks)
			for (const auto& job : batch)
			{
				assert(job.text);
				task.position = job.position;
				task.scale = glm::vec2(job.scale);
				const float spacing = job.scale * (1.f + static_cast<float>(job.spacing) / static_cast<float>(_createInfo.symbolSize));
				const float lineSpacing = job.scale * (1.f + static_cast<float>(job.lineSpacing) / static_cast<float>(_createInfo.symbolSize));

				const auto len = static_cast<uint32_t>(strlen(job.text));

				uint32_t lineLength = 0;
				uint32_t nextLineStart = 0;

				float xOffset = 0;
				if (job.center)
				{
					const auto l = jv::Min<uint32_t>(len, job.lineLength);
					xOffset = spacing * static_cast<float>(l - 1) * -.5f;
				}

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
						task.position.x = job.position.x + xOffset;

						if(i != 0)
						{
							task.position.y += lineSpacing;
							task.position.x -= spacing;
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

						auto subTexture = isInteger ? _createInfo.numberSubTexture : isSymbol ? _createInfo.symbolSubTexture : _createInfo.alphabetSubTexture;
						subTexture.lTop.x += symbolPctSize * static_cast<float>(position);
						subTexture.rBot.x = subTexture.lTop.x + symbolPctSize;

						task.subTexture = subTexture;

						_createInfo.instancedRenderTasks->Push(task);
					}

					task.position.x += spacing;
					lineLength++;
				}
			}
	}

	void TextInterpreter::OnExit(const EngineMemory& memory)
	{
	}
}
