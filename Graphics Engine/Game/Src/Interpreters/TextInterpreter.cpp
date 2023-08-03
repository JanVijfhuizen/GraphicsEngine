#include "pch_game.h"
#include "Interpreters/TextInterpreter.h"
#include "JLib/Curve.h"
#include "JLib/Math.h"

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

		const auto bounceUpCurve = je::CreateCurveOvershooting();
		const auto bounceDownCurve = je::CreateCurveDecelerate();
		
		for (const auto& batch : tasks)
			for (const auto& job : batch)
			{
				assert(job.text);
				
				const auto len = static_cast<uint32_t>(strlen(job.text));
				auto maxLen = job.maxLength == UINT32_MAX ? len : job.maxLength;

				const bool fadeIn = job.lifetime > 0 && job.lifetime < _createInfo.fadeInSpeed* static_cast<float>(len);
				if(fadeIn)
					maxLen = jv::Min<uint32_t>(len, static_cast<uint32_t>(job.lifetime * _createInfo.fadeInSpeed) + 1);

				const auto s = glm::ivec2(static_cast<int32_t>(_createInfo.symbolSize)) * glm::ivec2(static_cast<int32_t>(job.scale));
				const auto spacing = (_createInfo.spacing + job.spacing + _createInfo.symbolSize) * job.scale;

				task.position = job.position;
				task.scale = s;

				uint32_t lineLength = 0;
				uint32_t nextLineStart = 0;
				uint32_t xStart = 0;
				
				for (uint32_t i = 0; i < len; ++i)
				{
					if (i == maxLen)
						break;

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
						if (nextLineStart == i)
							nextLineStart = len;
						
						lineLength = 0;
						if(job.center)
							xStart = (nextLineStart - i) * (_createInfo.symbolSize + _createInfo.spacing)* job.scale / 2;
						task.position.x = static_cast<int32_t>(job.position.x - xStart);

						if(i != 0)
						{
							task.position.y -= static_cast<int32_t>(_createInfo.symbolSize * job.scale);
							task.position.x -= static_cast<int32_t>(spacing);
						}
					}

					auto c = job.text[i];

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

						float yMod = 0;

						float lifeTime = job.lifetime;
						if (job.loop)
							lifeTime = fmodf(lifeTime, (static_cast<float>(len) + _createInfo.bounceDuration) / _createInfo.fadeInSpeed);

						const float timeDiff = lifeTime * _createInfo.fadeInSpeed - static_cast<float>(i);
						if(fadeIn || job.loop && timeDiff > 0 && timeDiff < _createInfo.bounceDuration)
							yMod = DoubleCurveEvaluate(timeDiff / _createInfo.bounceDuration, bounceUpCurve, bounceDownCurve);

						PixelPerfectRenderTask cpyTask = task;
						cpyTask.position.y += static_cast<int32_t>(yMod * 3);
						_createInfo.renderTasks->Push(cpyTask);
					}

					task.position.x += static_cast<int32_t>(spacing);
					lineLength++;
				}
			}
	}

	void TextInterpreter::OnExit(const EngineMemory& memory)
	{
	}
}
