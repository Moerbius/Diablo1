#pragma once

#include "audio/music.hpp"
#include "font.hpp"
#include "focus_renderer.hpp"
#include "logo_renderer.hpp"
#include "portrait_renderer.hpp"
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
		ShowSavegame,
		Playing,
		Paused,
		Exiting
	};

	enum class IntroSequenceMode {
		Startup,
		ReplayDiabloOnly
	};

	enum class FocusAtlas {
		Focus,
		Focus16,
		Focus42
	};

	enum class PortraitAtlas {
		Heros,
		SmallPortrait
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
	void UpdateShowSavegameState(double dt);
	bool RenderShowSavegameState();
	void UpdatePlayingState(double dt);
	bool RenderPlayingState();
	bool LoadSharedFrontendAssets();
	bool GetFocusAtlas(
		FocusAtlas atlas,
		const std::vector<std::uint32_t>*& image,
		int& width,
		int& height,
		int& frameCount) const;
	bool GetPortraitAtlas(
		PortraitAtlas atlas,
		const std::vector<std::uint32_t>*& image,
		int& width,
		int& height,
		int& frameCount,
		bool& preferVertical) const;
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
	Font heroStatsFont_;
	bool heroStatsFontLoaded_;
	LogoRenderer logoRenderer_;
	FocusRenderer focusRenderer_;
	PortraitRenderer portraitRenderer_;
	libSmackker smackker_;
	State state_;
	IntroSequenceMode introSequenceMode_;
	double stateTimeSeconds_;
	double titlePresentationTimeSeconds_;
	bool introDone_;
	bool titlePresentationActive_;
	bool menuMusicPlaying_;
	bool resetFrameTimer_;
	bool returnToShowSavegameOnNewHeroEscape_;
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
	std::vector<std::uint32_t> herosImage_;
	std::vector<std::uint32_t> smallPortraitImage_;
	std::vector<std::uint32_t> focusImage_;
	std::vector<std::uint32_t> focus16Image_;
	std::vector<std::uint32_t> focus42Image_;
	int titleWidth_;
	int titleHeight_;
	int logoWidth_;
	int logoHeight_;
	int mainMenuWidth_;
	int mainMenuHeight_;
	int selheroWidth_;
	int selheroHeight_;
	int herosWidth_;
	int herosHeight_;
	int smallPortraitWidth_;
	int smallPortraitHeight_;
	int focusWidth_;
	int focusHeight_;
	int focus16Width_;
	int focus16Height_;
	int focus42Width_;
	int focus42Height_;
	int mainMenuSelectionIndex_;
	int newHeroClassSelectionIndex_;
};