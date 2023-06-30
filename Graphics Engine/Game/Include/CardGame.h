#pragma once
#include "CardGame.h"
#include "Engine/Engine.h"
#include "GE/SubTexture.h"
#include "JLib/Array.h"
#include "States/BoardState.h"
#include "States/GameState.h"
#include "States/PlayerState.h"
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
		enum class LevelState
		{
			mainMenu,
			newGame,
			inGame
		} _sceneState = LevelState::mainMenu;
		bool _levelLoading = true;

		Engine _engine;
		jv::Arena _arena;
		jv::Arena _levelArena;
		jv::ge::Resource _scene;
		jv::ge::Resource _atlas;
		jv::Array<jv::ge::SubTexture> _subTextures;
		TaskSystem<RenderTask>* _renderTasks;
		TaskSystem<DynamicRenderTask>* _dynamicRenderTasks;
		TaskSystem<TextTask>* _textTasks;
		InstancedRenderInterpreter* _renderInterpreter;
		DynamicRenderInterpreter* _dynamicRenderInterpreter;
		TextInterpreter* _textInterpreter;

		GameState _gameState{};
		PlayerState _playerState{};
		BoardState _boardState{};
		void* _levelState;

		void LoadMainMenu();
		void UpdateMainMenu();
	};
}
