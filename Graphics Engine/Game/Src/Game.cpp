#include "pch_game.h"
#include "CardGame.h"

int main()
{
	auto game = game::CardGame::Create();

	bool valid = true;
	while (valid)
		valid = game.Update();
	return 0;
}