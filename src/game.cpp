#include "game.hpp"

#include "pcx.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>

namespace {
constexpr int kWindowWidth = 640;
constexpr int kWindowHeight = 480;
constexpr int kRectSize = 80;
constexpr bool kPreserveTitleAspectRatio = false;
constexpr int kLogoOffsetYNative = 48;
constexpr double kTitleScreenDurationSeconds = 5.0;
constexpr double kRectSpeedX = 280.0;
constexpr double kRectSpeedY = 190.0;
constexpr int kMainMenuItemCount = 5;
constexpr int kNewHeroClassCount = 3;
constexpr int kSinglePlayerMenuIndex = 0;
constexpr int kReplayIntroMenuIndex = 2;
constexpr int kShowSavegameMenuIndex = 3;
constexpr int kExitDiabloMenuIndex = 4;

int CountSingleSavegamesInExecutableDir()
{
	std::filesystem::path basePath = std::filesystem::current_path();
	if (const char* sdlBasePath = SDL_GetBasePath(); sdlBasePath != nullptr) {
		basePath = sdlBasePath;
	}

	int count = 0;
	for (int i = 0; i <= 9; ++i) {
		const std::filesystem::path savePath = basePath / ("single_" + std::to_string(i) + ".sv");
		if (std::filesystem::exists(savePath)) {
			++count;
		}
	}

	return count;
}

const char* GetMainMenuItemName(int index)
{
	static constexpr std::array<const char*, kMainMenuItemCount> kItems = {
		"Single Player",
		"Multi Player",
		"Replay Intro",
		"Show Credits",
		"Exit Diablo"
	};

	if (index < 0 || index >= kMainMenuItemCount) {
		return "Unknown";
	}

	return kItems[static_cast<std::size_t>(index)];
}

const char* GetNewHeroClassName(int index)
{
	static constexpr std::array<const char*, kNewHeroClassCount> kClasses = {
		"Warrior",
		"Rogue",
		"Sorcerer"
	};

	if (index < 0 || index >= kNewHeroClassCount) {
		return "Unknown";
	}

	return kClasses[static_cast<std::size_t>(index)];
}
} // namespace

bool Game::Init()
{
	windowWidth_ = kWindowWidth;
	windowHeight_ = kWindowHeight;
	rectSize_ = kRectSize;
	rectX_ = 40.0;
	rectY_ = 40.0;
	rectVelocityX_ = kRectSpeedX;
	rectVelocityY_ = kRectSpeedY;
	state_ = State::Intro;
	introSequenceMode_ = IntroSequenceMode::Startup;
	stateTimeSeconds_ = 0.0;
	titlePresentationTimeSeconds_ = 0.0;
	introDone_ = false;
	titlePresentationActive_ = false;
	menuMusicPlaying_ = false;
	resetFrameTimer_ = false;
	returnToShowSavegameOnNewHeroEscape_ = false;
	titleWidth_ = 0;
	titleHeight_ = 0;
	logoWidth_ = 0;
	logoHeight_ = 0;
	mainMenuWidth_ = 0;
	mainMenuHeight_ = 0;
	selheroWidth_ = 0;
	selheroHeight_ = 0;
	herosWidth_ = 0;
	herosHeight_ = 0;
	smallPortraitWidth_ = 0;
	smallPortraitHeight_ = 0;
	focusWidth_ = 0;
	focusHeight_ = 0;
	focus16Width_ = 0;
	focus16Height_ = 0;
	focus42Width_ = 0;
	focus42Height_ = 0;
	mainMenuSelectionIndex_ = 0;
	newHeroClassSelectionIndex_ = 0;
	logoRenderer_.Reset();
	focusRenderer_.Reset();
	menuMoveSfxLoaded_ = false;
	menuSelectSfxLoaded_ = false;
	menuFontLoaded_ = false;
	menuButtonFontLoaded_ = false;
	heroCreationFontLoaded_ = false;
	heroClassFontLoaded_ = false;
	heroStatsFontLoaded_ = false;

	if (!mpq_.OpenArchive("DIABDAT.MPQ") && !mpq_.OpenArchive("../DIABDAT.MPQ")) {
		std::fprintf(stderr, "Failed to open DIABDAT.MPQ\n");
		return false;
	}

	if (!video_.Init("Diablo1", windowWidth_, windowHeight_)) {
		return false;
	}

	if (!menuMusic_.LoadMusicFromMpq(mpq_, Audio::MusicTrack::Dintro)) {
		// Music loading is not critical - game can run without it
		std::fprintf(stderr, "Note: Background music failed to load (optional)\n");
	}

	if (!menuMoveSfx_.LoadFromMpq(mpq_, Sfx::Items::TitleMove)) {
		std::fprintf(stderr, "Note: Menu move SFX failed to load (optional)\n");
	} else {
		menuMoveSfxLoaded_ = true;
	}

	if (!menuSelectSfx_.LoadFromMpq(mpq_, Sfx::Items::TitleSelect)) {
		std::fprintf(stderr, "Note: Menu select SFX failed to load (optional)\n");
	} else {
		menuSelectSfxLoaded_ = true;
	}

	menuFontLoaded_ = menuFont_.LoadPresetWithFallback(mpq_, Font::Preset::Font24s);
	menuButtonFontLoaded_ = menuButtonFont_.LoadPresetWithFallback(mpq_, Font::Preset::Font42g);
	heroCreationFontLoaded_ = heroCreationFont_.LoadPresetWithFallback(mpq_, Font::Preset::Font30s);
	heroClassFontLoaded_ = heroClassFont_.LoadPresetWithFallback(mpq_, Font::Preset::Font30g);
	heroStatsFontLoaded_ = heroStatsFont_.LoadPresetWithFallback(mpq_, Font::Preset::Font16s);

	return true;
}

bool Game::LoadSharedFrontendAssets()
{
	PCXImage titlePcx = PCX::LoadFromMPQ(mpq_, "ui_art\\title.pcx");
	PCXImage logoPcx = PCX::LoadFromMPQ(mpq_, "ui_art\\logo.pcx");
	PCXImage mainMenuPcx = PCX::LoadFromMPQ(mpq_, "ui_art\\mainmenu.pcx");
	PCXImage selheropcx = PCX::LoadFromMPQ(mpq_, "ui_art\\selhero.pcx");
	PCXImage herosPcx = PCX::LoadFromMPQ(mpq_, "ui_art\\heros.pcx");
	PCXImage smallPortraitPcx = PCX::LoadFromMPQ(mpq_, "ui_art\\heroport.pcx");
	PCXImage focusPcx = PCX::LoadFromMPQ(mpq_, "ui_art\\focus.pcx");
	PCXImage focus16Pcx = PCX::LoadFromMPQ(mpq_, "ui_art\\focus16.pcx");
	PCXImage focus42Pcx = PCX::LoadFromMPQ(mpq_, "ui_art\\focus42.pcx");

	if (titlePcx.pixels.empty() || logoPcx.pixels.empty() || mainMenuPcx.pixels.empty()) {
		return false;
	}

	titleWidth_ = static_cast<int>(titlePcx.width);
	titleHeight_ = static_cast<int>(titlePcx.height);
	logoWidth_ = static_cast<int>(logoPcx.width);
	logoHeight_ = static_cast<int>(logoPcx.height);
	mainMenuWidth_ = static_cast<int>(mainMenuPcx.width);
	mainMenuHeight_ = static_cast<int>(mainMenuPcx.height);

	titleImage_ = PCX::ConvertToRGBA32(titlePcx);
	logoImage_ = PCX::ConvertToRGBA32(logoPcx);
	mainMenuImage_ = PCX::ConvertToRGBA32(mainMenuPcx);

	if (!selheropcx.pixels.empty() && selheropcx.width > 0 && selheropcx.height > 0) {
		selheroWidth_ = static_cast<int>(selheropcx.width);
		selheroHeight_ = static_cast<int>(selheropcx.height);
		selheroImage_ = PCX::ConvertToRGBA32(selheropcx);
	} else {
		selheroWidth_ = 0;
		selheroHeight_ = 0;
		selheroImage_.clear();
	}

	if (!herosPcx.pixels.empty() && herosPcx.width > 0 && herosPcx.height > 0) {
		herosWidth_ = static_cast<int>(herosPcx.width);
		herosHeight_ = static_cast<int>(herosPcx.height);
		herosImage_ = PCX::ConvertToRGBA32(herosPcx);
	} else {
		herosWidth_ = 0;
		herosHeight_ = 0;
		herosImage_.clear();
	}

	if (!smallPortraitPcx.pixels.empty() && smallPortraitPcx.width > 0 && smallPortraitPcx.height > 0) {
		smallPortraitWidth_ = static_cast<int>(smallPortraitPcx.width);
		smallPortraitHeight_ = static_cast<int>(smallPortraitPcx.height);
		smallPortraitImage_ = PCX::ConvertToRGBA32(smallPortraitPcx);
	} else {
		smallPortraitWidth_ = 0;
		smallPortraitHeight_ = 0;
		smallPortraitImage_.clear();
	}

	if (!focusPcx.pixels.empty() && focusPcx.width > 0 && focusPcx.height > 0) {
		focusWidth_ = static_cast<int>(focusPcx.width);
		focusHeight_ = static_cast<int>(focusPcx.height);
		focusImage_ = PCX::ConvertToRGBA32(focusPcx);
	} else {
		focusWidth_ = 0;
		focusHeight_ = 0;
		focusImage_.clear();
	}

	if (!focus16Pcx.pixels.empty() && focus16Pcx.width > 0 && focus16Pcx.height > 0) {
		focus16Width_ = static_cast<int>(focus16Pcx.width);
		focus16Height_ = static_cast<int>(focus16Pcx.height);
		focus16Image_ = PCX::ConvertToRGBA32(focus16Pcx);
	} else {
		focus16Width_ = 0;
		focus16Height_ = 0;
		focus16Image_.clear();
	}

	if (!focus42Pcx.pixels.empty() && focus42Pcx.width > 0 && focus42Pcx.height > 0) {
		focus42Width_ = static_cast<int>(focus42Pcx.width);
		focus42Height_ = static_cast<int>(focus42Pcx.height);
		focus42Image_ = PCX::ConvertToRGBA32(focus42Pcx);
	} else {
		focus42Width_ = 0;
		focus42Height_ = 0;
		focus42Image_.clear();
	}

	return true;
}

bool Game::GetFocusAtlas(
	FocusAtlas atlas,
	const std::vector<std::uint32_t>*& image,
	int& width,
	int& height,
	int& frameCount) const
{
	image = nullptr;
	width = 0;
	height = 0;
	frameCount = 0;

	switch (atlas) {
	case FocusAtlas::Focus:
		image = &focusImage_;
		width = focusWidth_;
		height = focusHeight_;
		frameCount = 8;
		return true;
	case FocusAtlas::Focus16:
		image = &focus16Image_;
		width = focus16Width_;
		height = focus16Height_;
		frameCount = 8;
		return true;
	case FocusAtlas::Focus42:
		image = &focus42Image_;
		width = focus42Width_;
		height = focus42Height_;
		frameCount = 8;
		return true;
	}

	return false;
}

bool Game::GetPortraitAtlas(
	PortraitAtlas atlas,
	const std::vector<std::uint32_t>*& image,
	int& width,
	int& height,
	int& frameCount,
	bool& preferVertical) const
{
	image = nullptr;
	width = 0;
	height = 0;
	frameCount = 0;
	preferVertical = true;

	switch (atlas) {
	case PortraitAtlas::Heros:
		image = &herosImage_;
		width = herosWidth_;
		height = herosHeight_;
		frameCount = 4;
		preferVertical = true;
		return true;
	case PortraitAtlas::SmallPortrait:
		image = &smallPortraitImage_;
		width = smallPortraitWidth_;
		height = smallPortraitHeight_;
		frameCount = 12;
		preferVertical = true;
		return true;
	}

	return false;
}

int Game::Run()
{
	bool isRunning = true;
	auto previousFrameTime = std::chrono::steady_clock::now();

	while (isRunning) {
		auto currentFrameTime = std::chrono::steady_clock::now();
		double dt = std::chrono::duration<double>(currentFrameTime - previousFrameTime).count();
		previousFrameTime = currentFrameTime;

		if (resetFrameTimer_) {
			dt = 0.0;
			resetFrameTimer_ = false;
		}

		if (!HandleInput(isRunning)) {
			return 1;
		}

		Update(dt);

		if (state_ == State::Exiting) {
			isRunning = false;
			continue;
		}

		if (!Render()) {
			return 1;
		}
	}

	return 0;
}

bool Game::HandleInput(bool& isRunning)
{
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		if (event.type == SDL_EVENT_QUIT) {
			isRunning = false;
			continue;
		}

		if (event.type == SDL_EVENT_WINDOW_RESIZED) {
			windowWidth_ = event.window.data1;
			windowHeight_ = event.window.data2;
			continue;
		}

		if (event.type == SDL_EVENT_KEY_DOWN) {
			// Handle Alt+Enter for fullscreen toggle
			if ((event.key.mod & (SDL_KMOD_LALT | SDL_KMOD_RALT)) && 
			    (event.key.key == SDLK_RETURN || event.key.key == SDLK_KP_ENTER)) {
				video_.ToggleFullscreen();
				continue;
			}

			if (state_ == State::Intro && titlePresentationActive_) {
				EnterState(State::MainMenu);
			} else if (state_ == State::MainMenu) {
				if (event.key.key == SDLK_ESCAPE) {
					isRunning = false;
				} else if (event.key.key == SDLK_UP) {
					const int previousIndex = mainMenuSelectionIndex_;
					mainMenuSelectionIndex_ = (mainMenuSelectionIndex_ + kMainMenuItemCount - 1) % kMainMenuItemCount;
					if (menuMoveSfxLoaded_ && mainMenuSelectionIndex_ != previousIndex) {
						menuMoveSfx_.PlayOneShot();
					}
				} else if (event.key.key == SDLK_DOWN) {
					const int previousIndex = mainMenuSelectionIndex_;
					mainMenuSelectionIndex_ = (mainMenuSelectionIndex_ + 1) % kMainMenuItemCount;
					if (menuMoveSfxLoaded_ && mainMenuSelectionIndex_ != previousIndex) {
						menuMoveSfx_.PlayOneShot();
					}
				} else if (event.key.key == SDLK_RETURN || event.key.key == SDLK_KP_ENTER) {
					if (menuSelectSfxLoaded_) {
						menuSelectSfx_.PlayOneShot();
					}
					std::fprintf(stdout, "Selected menu item: %s\n", GetMainMenuItemName(mainMenuSelectionIndex_));
					std::fflush(stdout);
					if (mainMenuSelectionIndex_ == kSinglePlayerMenuIndex) {
						if (CountSingleSavegamesInExecutableDir() > 0) {
							EnterState(State::ShowSavegame);
						} else {
							returnToShowSavegameOnNewHeroEscape_ = false;
							EnterState(State::NewHero);
						}
					} else if (mainMenuSelectionIndex_ == kReplayIntroMenuIndex) {
						introSequenceMode_ = IntroSequenceMode::ReplayDiabloOnly;
						EnterState(State::Intro);
					} else if (mainMenuSelectionIndex_ == kShowSavegameMenuIndex) {
						EnterState(State::ShowSavegame);
					} else if (mainMenuSelectionIndex_ == kExitDiabloMenuIndex) {
						EnterState(State::Exiting);
						isRunning = false;
					}
				}
			} else if (state_ == State::NewHero) {
				if (event.key.key == SDLK_ESCAPE) {
					if (menuSelectSfxLoaded_) {
						menuSelectSfx_.PlayOneShot();
					}
					if (returnToShowSavegameOnNewHeroEscape_) {
						returnToShowSavegameOnNewHeroEscape_ = false;
						EnterState(State::ShowSavegame);
					} else {
						EnterState(State::MainMenu);
					}
				} else if (event.key.key == SDLK_UP) {
					const int previousIndex = newHeroClassSelectionIndex_;
					newHeroClassSelectionIndex_ = (newHeroClassSelectionIndex_ + kNewHeroClassCount - 1) % kNewHeroClassCount;
					if (menuMoveSfxLoaded_ && newHeroClassSelectionIndex_ != previousIndex) {
						menuMoveSfx_.PlayOneShot();
					}
				} else if (event.key.key == SDLK_DOWN) {
					const int previousIndex = newHeroClassSelectionIndex_;
					newHeroClassSelectionIndex_ = (newHeroClassSelectionIndex_ + 1) % kNewHeroClassCount;
					if (menuMoveSfxLoaded_ && newHeroClassSelectionIndex_ != previousIndex) {
						menuMoveSfx_.PlayOneShot();
					}
				} else if (event.key.key == SDLK_RETURN || event.key.key == SDLK_KP_ENTER) {
					if (menuSelectSfxLoaded_) {
						menuSelectSfx_.PlayOneShot();
					}
					std::fprintf(stdout, "Selected hero class: %s\n", GetNewHeroClassName(newHeroClassSelectionIndex_));
					std::fflush(stdout);
				}
			} else if (state_ == State::SelectHero) {
				if (event.key.key == SDLK_ESCAPE) {
					if (menuSelectSfxLoaded_) {
						menuSelectSfx_.PlayOneShot();
					}
					EnterState(State::MainMenu);
				}
			} else if (state_ == State::ShowSavegame) {
				const int slotCount = CountSingleSavegamesInExecutableDir() + 1; // + New Hero
				const int selectedIndex = std::clamp(newHeroClassSelectionIndex_, 0, std::max(1, slotCount) - 1);
				const bool isNewHeroEntry = selectedIndex == (std::max(1, slotCount) - 1);
				if (event.key.key == SDLK_UP) {
					const int previousIndex = newHeroClassSelectionIndex_;
					newHeroClassSelectionIndex_ =
						(newHeroClassSelectionIndex_ + slotCount - 1) % std::max(1, slotCount);
					if (menuMoveSfxLoaded_ && newHeroClassSelectionIndex_ != previousIndex) {
						menuMoveSfx_.PlayOneShot();
					}
				} else if (event.key.key == SDLK_DOWN) {
					const int previousIndex = newHeroClassSelectionIndex_;
					newHeroClassSelectionIndex_ =
						(newHeroClassSelectionIndex_ + 1) % std::max(1, slotCount);
					if (menuMoveSfxLoaded_ && newHeroClassSelectionIndex_ != previousIndex) {
						menuMoveSfx_.PlayOneShot();
					}
				} else if (event.key.key == SDLK_RETURN || event.key.key == SDLK_KP_ENTER) {
					if (menuSelectSfxLoaded_) {
						menuSelectSfx_.PlayOneShot();
					}
					if (isNewHeroEntry) {
						returnToShowSavegameOnNewHeroEscape_ = true;
						EnterState(State::NewHero);
					} else {
						EnterState(State::MainMenu);
					}
				} else if (event.key.key == SDLK_ESCAPE) {
					if (menuSelectSfxLoaded_) {
						menuSelectSfx_.PlayOneShot();
					}
					EnterState(State::MainMenu);
				}
			}
		}
	}

	return true;
}

void Game::EnterState(State nextState)
{
	if (nextState != State::MainMenu && nextState != State::NewHero && nextState != State::SelectHero &&
		nextState != State::ShowSavegame) {
		menuMusic_.StopMusic();
		menuMusicPlaying_ = false;
	}

	state_ = nextState;
	stateTimeSeconds_ = 0.0;
	titlePresentationTimeSeconds_ = 0.0;

	if (state_ == State::Intro) {
		rectX_ = (windowWidth_ - rectSize_) / 2.0;
		rectY_ = (windowHeight_ - rectSize_) / 2.0;
		logoRenderer_.Reset();
		focusRenderer_.Reset();
		introDone_ = false;
		titlePresentationActive_ = false;
	}

	if (state_ == State::MainMenu) {
		bool needsMusic = !titlePresentationActive_;
		titlePresentationActive_ = false;
		returnToShowSavegameOnNewHeroEscape_ = false;
		mainMenuSelectionIndex_ = std::clamp(mainMenuSelectionIndex_, 0, kMainMenuItemCount - 1);
		focusRenderer_.Reset();
		if (needsMusic && !menuMusicPlaying_) {
			menuMusicPlaying_ = menuMusic_.PlayMusic(true);
		}
	}

	if (state_ == State::NewHero || state_ == State::SelectHero || state_ == State::ShowSavegame) {
		logoRenderer_.Reset();
		focusRenderer_.Reset();
		if (state_ == State::NewHero) {
			newHeroClassSelectionIndex_ = 0;
		} else if (state_ == State::ShowSavegame) {
			newHeroClassSelectionIndex_ = 0;
		}
	}
}

void Game::Update(double dt)
{
	stateTimeSeconds_ += dt;

	switch (state_) {
	case State::Intro:
		UpdateIntroState(dt);
		return;
	case State::MainMenu:
		UpdateMainMenuState(dt);
		return;
	case State::NewHero:
		UpdateNewHeroState(dt);
		return;
	case State::SelectHero:
		UpdateSelectHeroState(dt);
		return;
	case State::ShowSavegame:
		UpdateShowSavegameState(dt);
		return;
	case State::Playing:
	case State::Paused:
		UpdatePlayingState(dt);
		return;
	case State::Exiting:
		return;
	}
}

bool Game::Render()
{
	switch (state_) {
	case State::Intro:
		return RenderIntroState();
	case State::MainMenu:
		return RenderMainMenuState();
	case State::NewHero:
		return RenderNewHeroState();
	case State::SelectHero:
		return RenderSelectHeroState();
	case State::ShowSavegame:
		return RenderShowSavegameState();
	case State::Playing:
	case State::Paused:
	case State::Exiting:
		return true;
	}

	return true;
}