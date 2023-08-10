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

	EngineMemory::EngineMemory(jv::Arena& arena, jv::Arena& tempArena, jv::Arena& frameArena) :
		arena(arena), tempArena(tempArena), frameArena(frameArena)
	{

	}

	void* ITaskInterpreter::GetTaskSystemPtr() const
	{
		return _taskSystem;
	}

	bool Engine::Update()
	{
		const bool waitForImage = jv::ge::WaitForImage();
		if (!waitForImage)
			return false;

		const auto memory = GetMemory();
		
		for (const auto& interpreter : _taskInterpreters)
			interpreter->Update(memory);

		// Update renderer.
		jv::ge::RenderFrameInfo renderFrameInfo{};
		if (!RenderFrame(renderFrameInfo))
			return false;

		// Clear tasks.
		for (const auto& taskSystem : _taskSystems)
			if(taskSystem->autoClear)
				taskSystem->ClearTasks();

		// Clear frame arena.
		_frameArena.Clear();
		return true;
	}

	Engine Engine::Create(const EngineCreateInfo& info)
	{
		// Set up renderer.
		jv::ge::CreateInfo createInfo{};
		createInfo.onKeyCallback = info.onKeyCallback;
		createInfo.onMouseCallback = info.onMouseCallback;
		createInfo.onScrollCallback = info.onScrollCallback;
		createInfo.resolution = info.resolution;
		Initialize(createInfo);

		Engine engine{};
		engine._arenaMem = malloc(info.arenaSize);
		engine._tempArenaMem = malloc(info.frameArenaSize);
		engine._frameArenaMem = malloc(info.tempArenaSize);

		jv::ArenaCreateInfo arenaCreateInfo{};
		arenaCreateInfo.alloc = Alloc;
		arenaCreateInfo.free = Free;
		arenaCreateInfo.memorySize = info.arenaSize;
		arenaCreateInfo.memory = engine._arenaMem;
		engine._arena = jv::Arena::Create(arenaCreateInfo);
		arenaCreateInfo.memorySize = info.tempArenaSize;
		arenaCreateInfo.memory = engine._tempArenaMem;
		engine._tempArena = jv::Arena::Create(arenaCreateInfo);
		arenaCreateInfo.memorySize = info.frameArenaSize;
		arenaCreateInfo.memory = engine._frameArenaMem;
		engine._frameArena = jv::Arena::Create(arenaCreateInfo);
		return engine;
	}

	void Engine::Destroy(const Engine& engine)
	{
		jv::Arena::Destroy(engine._frameArena);
		jv::Arena::Destroy(engine._tempArena);
		jv::Arena::Destroy(engine._arena);

		// Shut down renderer.
		jv::ge::Shutdown();
	}

	EngineMemory Engine::GetMemory()
	{
		return {_arena, _tempArena, _frameArena };
	}

	jv::Arena Engine::CreateSubArena(const uint32_t size)
	{
		jv::ArenaCreateInfo info{};
		info.memorySize = size;
		info.memory = _arena.Alloc(size);
		info.alloc = Alloc;
		info.free = Free;
		return jv::Arena::Create(info);
	}
}
