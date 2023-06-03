#include "pch_game.h"
#include "Engine/Engine.h"

int main()
{
	game::EngineCreateInfo engineCreateInfo{};
	auto engine = game::Engine::Create(engineCreateInfo);

	while(true)
	{
		const bool result = engine.Update();
		if (!result)
			break;
	}

	game::Engine::Destroy(engine);
}
