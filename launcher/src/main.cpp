// The launcher (see app.h). hotel-luigi-game [--play] [--returned <exit code>]: --play starts the game at once and
// ends when it does (a Steam shortcut, Playnite, a desktop entry), unless an update is out or the game needs a build;
// --returned is how the launcher starts itself again after the game ended in its window (game_host.cpp).
// hotel-luigi-game --prepare --dump=<folder>: the build alone, with no window (RecompLauncher's setup step, prepare_build).
// --data <folder> (or HL_DATA): everything the game writes goes there instead of this folder (config.h, Paths).
#include "app.h"
#include "platform.h"
#include <cstring>

int main(int argc, char** argv)
{
	std::vector<char*> args;                     // without --data, which is passed on as HL_DATA
	for (int i = 0; i < argc; i++)
		if (!std::strcmp(argv[i], "--data") && i + 1 < argc)
			set_env("HL_DATA", argv[++i], true);
		else if (!std::strncmp(argv[i], "--data=", 7))
			set_env("HL_DATA", argv[i] + 7, true);
		else
			args.push_back(argv[i]);
	const int n = int(args.size());
	args.push_back(nullptr);
	if (n > 1 && !std::strcmp(args[1], "--prepare"))
		return prepare_build(args[0], std::vector<std::string>(args.begin() + 2, args.end() - 1));
	App app;
	if (!app.init(n, args.data()))
		return 1;
	return app.run();
}
