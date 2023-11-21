#include "pch_game.h"
#include "CardGame.h"

bool Loop()
{
	game::Start();
	bool valid = true;
	bool ret;
	while (valid)
		valid = game::Update(ret);
	game::Stop();
	return ret;
}

int main()
{
	while (Loop())
		;
	return 0;
}