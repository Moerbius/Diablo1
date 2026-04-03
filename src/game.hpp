#pragma once

#include "audio/music.hpp"
#include "font.hpp"
#include "smacker/libsmackker.hpp"
#include "audio/sfx.hpp"
#include "storm/stormlib.hpp"
#include "video.hpp"

#include <cstdint>
#include <vector>

class Game {
public:
	bool Init();
	int Run();

private:
	enum class State {
		Intro,
		MainMenu,
		NewHero,
		SelectHero,
		Playing,
		Paused,
		Exiting
	};

	enum class IntroSequenceMode {
		Startup,
		ReplayDiabloOnly
	};

	bool HandleInput(bool& isRunning);
	void EnterState(State nextState);
	void UpdateIntroState(double dt);
	bool RenderIntroState();
	void UpdateMainMenuState(double dt);
	bool RenderMainMenuState();
	void UpdateNewHeroState(double dt);
	bool RenderNewHeroState();
	void UpdateSelectHeroState(double dt);
	bool RenderSelectHeroState();
	void UpdatePlayingState(double dt);
	void Update(double dt);
	bool Render();

	Video video_;
	StormLib mpq_;
	Audio menuMusic_;
	Sfx menuMoveSfx_;
	bool menuMoveSfxLoaded_;
	Sfx menuSelectSfx_;
	bool menuSelectSfxLoaded_;
	Font menuFont_;
	bool menuFontLoaded_;
	Font menuButtonFont_;
	bool menuButtonFontLoaded_;
	Font heroCreationFont_;
	bool heroCreationFontLoaded_;
	Font heroClassFont_;
	bool heroClassFontLoaded_;
	libSmackker smackker_;
	State state_;
	IntroSequenceMode introSequenceMode_;
	double stateTimeSeconds_;
	double titlePresentationTimeSeconds_;
	bool introDone_;
	bool titlePresentationActive_;
	bool resetFrameTimer_;
	int windowWidth_;
	int windowHeight_;
	int rectSize_;
	double rectX_;
	double rectY_;
	double rectVelocityX_;
	double rectVelocityY_;
	
	// Title screen images
	std::vector<std::uint32_t> titleImage_;
	std::vector<std::uint32_t> logoImage_;
	std::vector<std::uint32_t> mainMenuImage_;
	std::vector<std::uint32_t> selheroImage_;
	std::vector<std::uint32_t> focus42Image_;
	int titleWidth_;
	int titleHeight_;
	int logoWidth_;
	int logoHeight_;
	int mainMenuWidth_;
	int mainMenuHeight_;
	int selheroWidth_;
	int selheroHeight_;
	int focus42Width_;
	int focus42Height_;
	int mainMenuSelectionIndex_;
	int currentLogoFrame_;
	double logoAnimationTime_;
};