#include "pch_game.h"
#include "Interpreters/TextInterpreter.h"
#include "JLib/Curve.h"
#include "JLib/Math.h"
#include "GE/SubTexture.h"
#include <Utils/SubTextureUtils.h>

namespace game
{
	const char* TextInterpreter::Concat(const char* a, const char* b, jv::Arena& arena)
	{
		const size_t aS = strlen(a);
		const size_t bS = strlen(b);
		arena.Alloc(aS + bS);
		
		const auto text = static_cast<char*>(arena.Alloc(aS + bS));
		memcpy(text, a, aS);
		memcpy(&text[aS], b, bS + 1);
		return text;
	}

	const char* TextInterpreter::IntToConstCharPtr(const uint32_t i, jv::Arena& arena)
	{
		char s[9 + sizeof(char)];
		const uint32_t size = GetNumberOfDigits(i);
		const auto ptr = static_cast<char*>(arena.Alloc(size + 1));
		sprintf_s(s, "%d", i);
		memcpy(ptr, s, size + 1);
		return ptr;
	}

	uint32_t TextInterpreter::GetLineCount(const char* str, const uint32_t lineLength, const uint32_t maxLength)
	{
		uint32_t lineCount = 1;
		uint32_t l = strlen(str);
		if (maxLength != -1)
			l = jv::Min<uint32_t>(l, maxLength);

		uint32_t lineStart = 0;
		uint32_t lastBreakPointFound = -1;
		for (uint32_t i = 0; i < l; ++i)
		{
			const auto c = str[i];
			if (c == ' ')
			{
				if (i - lineStart >= lineLength)
				{
					if (lastBreakPointFound != -1)
					{
						lineStart = lastBreakPointFound;
						++lineCount;
						i = lineStart - 1;
					}
				}
				else
					lastBreakPointFound = i;
			}
		}

		const uint32_t a = static_cast<int32_t>(l) - static_cast<int32_t>(lineStart) >= static_cast<int32_t>(lineLength);
		return lineCount + a + 1;
	}

	void TextInterpreter::OnStart(const TextInterpreterCreateInfo& createInfo, const EngineMemory& memory)
	{
		_createInfo = createInfo;
	}

	void TextInterpreter::OnUpdate(const EngineMemory& memory, const jv::LinkedList<jv::Vector<TextTask>>& tasks)
	{
		for (const auto& batch : tasks)
			for (const auto& job : batch)
				Draw(memory, job);
	}

	void TextInterpreter::OnExit(const EngineMemory& memory)
	{
	}

	jv::ge::SubTexture TextInterpreter::Draw(const EngineMemory& memory, TextTask job)
	{
		const float symbolPctSize = static_cast<float>(_createInfo.symbolSize) / static_cast<float>(_createInfo.atlasResolution.x);
		const float largeSymbolPctSize = static_cast<float>(_createInfo.largeSymbolSize) / static_cast<float>(_createInfo.atlasResolution.x);

		PixelPerfectRenderTask task{};

		const auto bounceUpCurve = je::CreateCurveOvershooting();
		const auto bounceDownCurve = je::CreateCurveDecelerate();

		if (!job.text)
			return {};

		task.color = job.color;
		const auto len = static_cast<uint32_t>(strlen(job.text));
		auto maxLen = job.maxLength == -1 ? len : job.maxLength;

		bool fadeIn = job.lifetime >= 0 && job.lifetime < _createInfo.fadeInSpeed * static_cast<float>(len);
		if (!job.fadeIn)
			fadeIn = false;
		if (fadeIn)
			maxLen = jv::Min<uint32_t>(maxLen, static_cast<uint32_t>(job.lifetime * _createInfo.fadeInSpeed));

		const float size = job.largeFont ? _createInfo.largeSymbolSize : _createInfo.symbolSize;
		const float pctSize = job.largeFont ? largeSymbolPctSize : symbolPctSize;

		const auto s = glm::ivec2(static_cast<int32_t>(size));
		const auto spacing = (_createInfo.spacing + job.spacing + size);

		jv::ge::SubTexture ret{};
		ret.lTop = {9999, -9999 };
		ret.rBot = { -9999, 9999 };

		task.position = job.position;
		if (job.yCenter)
			task.position.y += size * (GetLineCount(job.text, job.lineLength, maxLen) - 1);

		task.scale = s;

		uint32_t lineLength = 0;
		uint32_t nextLineStart = 0;
		uint32_t xStart = 0;

		bool isInBrackets = false;

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
				if (job.xCenter)
				{
					xStart = (nextLineStart - i) * (size + _createInfo.spacing) / 2;
					xStart += size / 4;
				}
				task.position.x = static_cast<int32_t>(job.position.x - xStart);

				if (i != 0)
				{
					task.position.y -= static_cast<int32_t>(size);
					task.position.x -= static_cast<int32_t>(spacing);
				}
			}

			auto c = job.text[i];

			if (c == ']')
				isInBrackets = false;

			if (c != ' ')
			{
				const bool isSymbol2ndRow = c > '9' && c < 'a';
				const bool isSymbol = c < '0' || isSymbol2ndRow;
				const bool isInteger = !isSymbol && c < 'a';

				// Assert if it's a valid character.
				constexpr uint32_t secondRow = '[' - 5;
				auto position = c - (isInteger ? '0' : isSymbol ? isSymbol2ndRow ? secondRow : '+' : 'a');
				auto subTexture = isInteger ? _createInfo.numberAtlasTexture.subTexture : isSymbol ?
					_createInfo.symbolAtlasTexture.subTexture : (job.largeFont ? _createInfo.largeAlphabetAtlasTexture : _createInfo.alphabetAtlasTexture).subTexture;
				subTexture.lTop.x += pctSize * static_cast<float>(position);
				subTexture.rBot.x = subTexture.lTop.x + pctSize;

				task.subTexture = subTexture;

				float yMod = 0;

				float lifeTime = job.lifetime;
				if (job.loop)
					lifeTime = fmodf(lifeTime, (static_cast<float>(len) + _createInfo.bounceDuration) / _createInfo.fadeInSpeed);

				const float timeDiff = lifeTime * _createInfo.fadeInSpeed - static_cast<float>(i);
				if (fadeIn || job.loop && timeDiff > 0 && timeDiff < _createInfo.bounceDuration)
					yMod = DoubleCurveEvaluate(timeDiff / _createInfo.bounceDuration, bounceUpCurve, bounceDownCurve);

				PixelPerfectRenderTask cpyTask = task;
				cpyTask.priority = job.priority;
				cpyTask.front = job.front;
				cpyTask.position.y += static_cast<int32_t>(yMod * _createInfo.bounceHeight);
				if (isInBrackets)
					cpyTask.color = { 0, 1, 0, 1 };
				_createInfo.renderTasks->Push(cpyTask);

				ret.lTop.x = jv::Min<float>(ret.lTop.x, task.position.x);
				ret.lTop.y = jv::Max<float>(ret.lTop.y, task.position.y);
				ret.rBot.x = jv::Max<float>(ret.rBot.x, task.position.x);
				ret.rBot.y = jv::Min<float>(ret.rBot.y, task.position.y);
			}

			if (c == '[')
				isInBrackets = true;

			task.position.x += static_cast<int32_t>(spacing);
			lineLength++;
		}

		if (job.textBubble)
		{
			jv::ge::SubTexture bubbleFrames[9];
			Divide2d(_createInfo.textBubbleAtlasTexture.subTexture, bubbleFrames, 3);

			const glm::ivec2 scale = ret.Size() + glm::vec2(size * 2);

			// Create text bubble.
			task.position = ret.Center() + glm::vec2(size / 2);
			task.scale = scale;
			task.image = nullptr;
			task.subTexture = bubbleFrames[4];
			task.position -= task.scale / 2;
			task.priority = false;
			_createInfo.renderTasks->Push(task);

			const uint32_t BORDER_SCALE = 5;

			task.scale = { BORDER_SCALE, BORDER_SCALE };

			// Right top.
			task.subTexture = bubbleFrames[8];
			task.position += scale;
			_createInfo.renderTasks->Push(task);

			// Left top.
			task.subTexture = bubbleFrames[2];
			task.position.x -= scale.x + BORDER_SCALE;
			_createInfo.renderTasks->Push(task);

			auto cpyTask = task;
			auto cpyTask2 = task;

			// Left bot.
			task.subTexture = bubbleFrames[0];
			task.position.y -= scale.y + BORDER_SCALE;
			_createInfo.renderTasks->Push(task);

			auto cpyTask3 = task;

			// Right bot.
			task.subTexture = bubbleFrames[6];
			task.position.x += scale.x + BORDER_SCALE;
			_createInfo.renderTasks->Push(task);

			cpyTask.scale.x = scale.x;
			cpyTask.position.x += BORDER_SCALE;
			cpyTask.subTexture = bubbleFrames[5];
			_createInfo.renderTasks->Push(cpyTask);

			cpyTask.position.y -= scale.y + BORDER_SCALE;
			cpyTask.subTexture = bubbleFrames[3];
			_createInfo.renderTasks->Push(cpyTask);

			cpyTask2.scale.y = scale.y;
			cpyTask2.position.y -= scale.y;
			cpyTask2.subTexture = bubbleFrames[1];
			_createInfo.renderTasks->Push(cpyTask2);

			cpyTask2.position.x += scale.x + BORDER_SCALE;
			cpyTask2.subTexture = bubbleFrames[7];
			_createInfo.renderTasks->Push(cpyTask2);

			// Tail.
			if (job.textBubbleTail)
			{
				cpyTask3.subTexture = _createInfo.textBubbleTailAtlasTexture.subTexture;
				cpyTask3.scale = _createInfo.textBubbleTailAtlasTexture.resolution;
				cpyTask3.position.y -= cpyTask3.scale.y - 2;
				_createInfo.renderTasks->Push(cpyTask3);
			}
		}

		return ret;
	}
}
