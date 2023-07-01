#include "pch_game.h"
#include "CardGame.h"

int main()
{
	game::CardGame cardGame;
	game::CardGame::Create(&cardGame);

	bool valid = true;
	while (valid)
		valid = cardGame.Update();
	return 0;
}