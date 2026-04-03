#include "game.hpp"

#include "pcx.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>

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
constexpr int kSinglePlayerMenuIndex = 0;
constexpr int kReplayIntroMenuIndex = 2;
constexpr int kExitDiabloMenuIndex = 4;

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
	resetFrameTimer_ = false;
	titleWidth_ = 0;
	titleHeight_ = 0;
	logoWidth_ = 0;
	logoHeight_ = 0;
	mainMenuWidth_ = 0;
	mainMenuHeight_ = 0;
	selheroWidth_ = 0;
	selheroHeight_ = 0;
	focus42Width_ = 0;
	focus42Height_ = 0;
	mainMenuSelectionIndex_ = 0;
	currentLogoFrame_ = 0;
	logoAnimationTime_ = 0.0;
	menuMoveSfxLoaded_ = false;
	menuSelectSfxLoaded_ = false;
	menuFontLoaded_ = false;
	menuButtonFontLoaded_ = false;
	heroCreationFontLoaded_ = false;
	heroClassFontLoaded_ = false;

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

	return true;
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
						EnterState(State::NewHero);
					} else if (mainMenuSelectionIndex_ == kReplayIntroMenuIndex) {
						introSequenceMode_ = IntroSequenceMode::ReplayDiabloOnly;
						EnterState(State::Intro);
					} else if (mainMenuSelectionIndex_ == kExitDiabloMenuIndex) {
						EnterState(State::Exiting);
						isRunning = false;
					}
				}
			} else if (state_ == State::NewHero || state_ == State::SelectHero) {
				if (event.key.key == SDLK_ESCAPE) {
					EnterState(State::MainMenu);
				}
			}
		}
	}

	return true;
}

void Game::EnterState(State nextState)
{
	if (nextState != State::MainMenu) {
		menuMusic_.StopMusic();
	}

	state_ = nextState;
	stateTimeSeconds_ = 0.0;
	titlePresentationTimeSeconds_ = 0.0;

	if (state_ == State::Intro) {
		rectX_ = (windowWidth_ - rectSize_) / 2.0;
		rectY_ = (windowHeight_ - rectSize_) / 2.0;
		logoAnimationTime_ = 0.0;
		currentLogoFrame_ = 0;
		introDone_ = false;
		titlePresentationActive_ = false;
	}

	if (state_ == State::MainMenu) {
		bool needsMusic = !titlePresentationActive_;
		titlePresentationActive_ = false;
		mainMenuSelectionIndex_ = std::clamp(mainMenuSelectionIndex_, 0, kMainMenuItemCount - 1);
		if (needsMusic) {
			menuMusic_.PlayMusic(true);
		}
	}

	if (state_ == State::NewHero || state_ == State::SelectHero) {
		logoAnimationTime_ = 0.0;
		currentLogoFrame_ = 0;
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
	case State::Playing:
	case State::Paused:
	case State::Exiting:
		return true;
	}

	return true;
}