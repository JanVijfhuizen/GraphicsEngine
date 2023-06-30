#include "pch_game.h"
#include "Interpreters/TextInterpreter.h"

#include "JLib/Math.h"
#include "Tasks/RenderTask.h"

namespace game
{
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

				uint32_t lineLength = 0;

				const auto len = static_cast<uint32_t>(strlen(job.text));

				uint32_t nextLineStart = 0;

				float xOffset = 0;
				if (job.center && len > job.lineLength)
					xOffset = spacing * static_cast<float>(jv::Min<uint32_t>(len, job.lineLength)) * -.5f;

				for (uint32_t i = 0; i < len; ++i)
				{
					if (nextLineStart == i)
					{
						// Define line length.
						for (uint32_t j = i; j < len; ++j)
						{
							const auto& c = job.text[j];
							if (c == ' ')
								if (j - i >= job.lineLength)
								{
									nextLineStart = j;
									break;
								}
						}
						
						lineLength = 0;
						task.position.x = job.position.x + xOffset;
						if(i != 0)
							task.position.y += lineSpacing;
					}

					const auto& c = job.text[i];

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
