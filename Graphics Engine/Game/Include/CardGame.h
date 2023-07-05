#pragma once
#include "CardGame.h"
#include "CardGame.h"
#include "Cards/ArtifactCard.h"
#include "Cards/MonsterCard.h"
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
			mouse,
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
		jv::ge::Resource _levelScene;
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
		uint32_t _levelLoadingFrame = 0;
		void* _levelStatePtr;

		jv::LinkedList<KeyCallback> _keyCallbacks{};
		jv::LinkedList<KeyCallback> _mouseCallbacks{};
		float _scrollCallback = 0;

		jv::Array<MonsterCard> _monsterCards;
		jv::Vector<uint32_t> _monsterCardsDeck;
		jv::Array<ArtifactCard> _artifactCards;
		jv::Vector<uint32_t> _artifactCardsDeck;

		bool _pressedEnter = false;

		void LoadMainMenu();
		void UpdateMainMenu(const MouseTask& mouseTask);
		void LoadNewGame();
		void UpdateNewGame(const MouseTask& mouseTask);

		void UpdateInput(MouseTask& outMouseTask);
		void DrawMonsterCard(uint32_t id, glm::vec2 position, glm::vec4 color = glm::vec4(1)) const;
		[[nodiscard]] uint32_t DrawMonsterChoice(const uint32_t* ids, glm::vec2 center, uint32_t count, uint32_t highlight = -1) const;
		void DrawArtifactCard(uint32_t id, glm::vec2 position, glm::vec4 color = glm::vec4(1)) const;
		[[nodiscard]] uint32_t DrawArtifactChoice(const uint32_t* ids, glm::vec2 center, uint32_t count, uint32_t highlight = -1) const;

		static void OnKeyCallback(size_t key, size_t action);
		static void OnMouseCallback(size_t key, size_t action);
		static void OnScrollCallback(glm::vec<2, double> offset);
	};
}
