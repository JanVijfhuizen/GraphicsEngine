#pragma once

namespace game
{
	struct NewGameState final
	{
		jv::Array<uint32_t> monsterDiscoverOptions;
		jv::Array<uint32_t> artifactDiscoverOptions;

		uint32_t monsterChoice = -1;
		uint32_t artifactChoice = -1;
	};
}