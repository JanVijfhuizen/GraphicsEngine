#pragma once

namespace game
{
	struct PlayerState;

	void Start();
	[[nodiscard]] bool Update(bool& outRestart);
	void Stop();

	[[nodiscard]] bool TryLoadSaveData(PlayerState& playerState);
	void SaveData(PlayerState& playerState);
	void ClearSaveData();
}
