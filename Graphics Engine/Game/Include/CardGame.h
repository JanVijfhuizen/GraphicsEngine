#pragma once
#include "CardGame.h"
#include "CardGame.h"
#include "Engine/Engine.h"
#include "GE/SubTexture.h"
#include "Interpreters/MouseInterpreter.h"
#include "JLib/Array.h"
#include "States/BoardState.h"
#include "States/GameState.h"
#include "States/PlayerState.h"
#include "Tasks/DynamicRenderTask.h"
#include "Tasks/MouseTask.h"
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

		[[nodiscard]] static glm::vec2 GetConvertedMousePosition();
	private:
		enum class LevelState
		{
			mainMenu,
			newGame,
			inGame
		};

		struct KeyCallback final
		{
			uint32_t key;
			uint32_t action;
		};

		Engine _engine;
		jv::Arena _arena;
		jv::Arena _levelArena;
		jv::ge::Resource _scene;
		jv::ge::Resource _atlas;
		jv::Array<jv::ge::SubTexture> _subTextures;
		TaskSystem<RenderTask>* _renderTasks;
		TaskSystem<DynamicRenderTask>* _dynamicRenderTasks;
		TaskSystem<MouseTask>* _mouseTasks;
		TaskSystem<TextTask>* _textTasks;
		InstancedRenderInterpreter* _renderInterpreter;
		DynamicRenderInterpreter* _dynamicRenderInterpreter;
		MouseInterpreter* mouseInterpreter;
		TextInterpreter* _textInterpreter;

		GameState _gameState{};
		PlayerState _playerState{};
		BoardState _boardState{};

		LevelState _levelState = LevelState::mainMenu;
		bool _levelLoading = true;
		void* _levelStatePtr;

		jv::LinkedList<KeyCallback> _keyCallbacks{};
		jv::LinkedList<KeyCallback> _mouseCallbacks{};
		float _scrollCallback = 0;

		void LoadMainMenu();
		void UpdateMainMenu();

		static void OnKeyCallback(size_t key, size_t action);
		static void OnMouseCallback(size_t key, size_t action);
		static void OnScrollCallback(glm::vec<2, double> offset);
	};
}
