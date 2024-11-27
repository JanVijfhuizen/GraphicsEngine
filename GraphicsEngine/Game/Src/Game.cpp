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

#ifdef _DEBUG
int main()
{
	while (Loop())
		;
	return 0;
}
#endif
#ifdef NDEBUG
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
	PSTR lpCmdLine, int nCmdShow)
{
	while (Loop())
		;
	return 0;
}
#endif