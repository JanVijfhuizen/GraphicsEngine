#pragma once

namespace game
{
	struct NewGameState final
	{
		jv::Array<uint32_t> monsterDiscoverOptions;
		jv::Array<uint32_t> artifactDiscoverOptions;
	};
}