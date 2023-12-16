#pragma once

namespace game
{
	void Start();
	[[nodiscard]] bool Update(bool& outRestart);
	void Stop();
}
