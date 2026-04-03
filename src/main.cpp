#include "game.hpp"

int main(int argc, char** argv)
{
	(void)argc;
	(void)argv;

	Game game;
	if (!game.Init()) {
		return 1;
	}

	return game.Run();
}
