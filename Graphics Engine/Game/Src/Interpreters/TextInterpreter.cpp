#include "pch_game.h"
#include "Interpreters/TextInterpreter.h"

#include "Tasks/InstancedRenderTask.h"

namespace game
{
	void TextInterpreter::OnStart(const TextInterpreterCreateInfo& createInfo, const EngineMemory& memory)
	{
		_createInfo = createInfo;
	}

	void TextInterpreter::OnUpdate(const EngineMemory& memory, const jv::LinkedList<jv::Vector<TextTask>>& tasks)
	{
		const float symbolPctSize = static_cast<float>(_createInfo.symbolSize) / static_cast<float>(_createInfo.atlasResolution.x);
		
		InstancedRenderTask task{};

		for (const auto& batch : tasks)
			for (const auto& job : batch)
			{
				assert(job.text);
				task.position = job.position;
				task.scale = glm::vec2(job.scale);
				const float spacing = job.scale * (1.f + static_cast<float>(job.spacing) / static_cast<float>(_createInfo.symbolSize));
				const float lineSpacing = job.scale * (1.f + static_cast<float>(job.lineSpacing) / static_cast<float>(_createInfo.symbolSize));

				uint32_t lineLength = 0;

				const size_t len = strlen(job.text);
				for (size_t i = 0; i < len; ++i)
				{
					const auto& c = job.text[i];

					if (c == ' ')
					{
						if (lineLength >= job.lineLength)
						{
							lineLength = 0;
							task.position.x = job.position.x;
							task.position.y += lineSpacing;
							continue;
						}
					}
					else
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
