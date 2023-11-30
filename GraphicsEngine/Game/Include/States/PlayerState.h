#pragma once

namespace game
{
	struct PlayerState final
	{
		uint32_t monsterIds[PARTY_CAPACITY]{};
		uint32_t partySize = 0;
		bool ironManMode = false;
		
		[[nodiscard]] static PlayerState Create();
	};
}