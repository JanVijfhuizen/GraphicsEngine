#include "pch_game.h"
#include "Engine/Engine.h"

#include "GE/GraphicsEngine.h"

namespace game
{
	void* Alloc(const uint32_t size)
	{
		return malloc(size);
	}

	void Free(void* ptr)
	{
		return free(ptr);
	}

	bool Engine::Update()
	{
		// Clear frame arena.
		frameArena.Clear();

		// Update renderer.
		jv::ge::RenderFrameInfo renderFrameInfo{};
		if (!RenderFrame(renderFrameInfo))
			return false;
		return true;
	}

	Engine Engine::Create(const EngineCreateInfo& info)
	{
		// Set up renderer.
		jv::ge::CreateInfo createInfo{};
		Initialize(createInfo);

		Engine engine{};
		engine.arenaMem = malloc(info.arenaSize);
		engine.tempArenaMem = malloc(info.frameArenaSize);
		engine.frameArenaMem = malloc(info.tempArenaSize);

		jv::ArenaCreateInfo arenaCreateInfo{};
		arenaCreateInfo.alloc = Alloc;
		arenaCreateInfo.free = Free;
		arenaCreateInfo.memorySize = info.arenaSize;
		arenaCreateInfo.memory = engine.arenaMem;
		engine.arena = jv::Arena::Create(arenaCreateInfo);
		arenaCreateInfo.memorySize = info.tempArenaSize;
		arenaCreateInfo.memory = engine.tempArenaMem;
		engine.tempArena = jv::Arena::Create(arenaCreateInfo);
		arenaCreateInfo.memorySize = info.frameArenaSize;
		arenaCreateInfo.memory = engine.frameArenaMem;
		engine.frameArena = jv::Arena::Create(arenaCreateInfo);
		return engine;
	}

	void Engine::Destroy(const Engine& engine)
	{
		jv::Arena::Destroy(engine.frameArena);
		jv::Arena::Destroy(engine.tempArena);
		jv::Arena::Destroy(engine.arena);

		// Shut down renderer.
		jv::ge::Shutdown();
	}
}
