#include "pch_game.h"
#include "CardGame.h"

int main()
{
	game::Start();

	bool valid = true;
	while (valid)
		valid = game::Update();
	game::Stop();
	return 0;
}