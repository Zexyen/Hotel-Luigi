// The launcher (see app.h). hotel-luigi-game [--play] [--returned <exit code>]: --play starts the game at once and
// ends when it does (a Steam shortcut, Playnite, a desktop entry), unless an update is out or the game needs a build;
// --returned is how the launcher starts itself again after the game ended in its window (game_host.cpp).
// hotel-luigi-game --prepare --dump=<folder>: the build alone, with no window (RecompLauncher's setup step, prepare_build).
#include "app.h"
#include <cstring>

int main(int argc, char** argv)
{
	if (argc > 1 && !std::strcmp(argv[1], "--prepare"))
		return prepare_build(argv[0], std::vector<std::string>(argv + 2, argv + argc));
	App app;
	if (!app.init(argc, argv))
		return 1;
	return app.run();
}
