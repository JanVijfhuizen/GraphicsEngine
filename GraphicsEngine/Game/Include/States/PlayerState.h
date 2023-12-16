#pragma once

namespace game
{
	struct PlayerState final
	{
		uint32_t monsterIds[PARTY_CAPACITY]{};
		uint32_t partySize = 0;
		
		[[nodiscard]] static PlayerState Create();
	};
}