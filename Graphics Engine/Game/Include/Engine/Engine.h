#pragma once

namespace game
{
	struct EngineCreateInfo final
	{
		uint32_t arenaSize = 4096;
		uint32_t tempArenaSize = 4096;
		uint32_t frameArenaSize = 4096;
	};

	struct Engine final
	{
		void* arenaMem;
		void* tempArenaMem;
		void* frameArenaMem;
		jv::Arena arena;
		jv::Arena tempArena;
		jv::Arena frameArena;

		[[nodiscard]] bool Update();

		[[nodiscard]] static Engine Create(const EngineCreateInfo& info);
		static void Destroy(const Engine& engine);
	};
}
