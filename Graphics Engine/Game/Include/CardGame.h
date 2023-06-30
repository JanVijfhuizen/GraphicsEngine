#pragma once
#include "CardGame.h"
#include "Engine/Engine.h"
#include "GE/SubTexture.h"
#include "JLib/Array.h"
#include "Tasks/DynamicRenderTask.h"
#include "Tasks/RenderTask.h"
#include "Tasks/TextTask.h"

namespace game
{
	class DynamicRenderInterpreter;
	class TextInterpreter;
	class InstancedRenderInterpreter;

	class CardGame final
	{
	public:
		enum class TextureId
		{
			alphabet,
			numbers,
			symbols,
			fallback,
			length
		};
		
		[[nodiscard]] bool Update();
		static void Create(CardGame* outCardGame);
		static void Destroy(const CardGame& cardGame);
	private:
		Engine _engine;
		jv::Arena _arena;
		jv::ge::Resource _scene;
		jv::ge::Resource _atlas;
		jv::Array<jv::ge::SubTexture> _subTextures;
		TaskSystem<RenderTask>* _renderTasks;
		TaskSystem<DynamicRenderTask>* _dynamicRenderTasks;
		TaskSystem<TextTask>* _textTasks;
		InstancedRenderInterpreter* _renderInterpreter;
		DynamicRenderInterpreter* _dynamicRenderInterpreter;
		TextInterpreter* _textInterpreter;
	};
}
