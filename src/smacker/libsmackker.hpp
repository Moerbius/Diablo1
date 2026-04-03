#pragma once

#include <string>

class StormLib;

struct SDL_Renderer;

class libSmackker {
public:
	enum class PlaybackResult {
		Completed,
		Skipped,
		Quit,
		Error
	};

	libSmackker();
	~libSmackker();

	bool Play(StormLib& mpq, const std::string& smkPath, SDL_Renderer* renderer);
	PlaybackResult PlayWithControl(StormLib& mpq, const std::string& smkPath, bool allowSkip, SDL_Renderer* renderer);
};