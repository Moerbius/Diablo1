#include "game.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <string>

namespace {
constexpr bool kPreserveTitleAspectRatio = true;
constexpr int kLogoOffsetYNative = 48;
constexpr double kTitleScreenDurationSeconds = 5.0;

bool PlayIntroSequence(libSmackker& smackker, StormLib& mpq, Video& video)
{
	SDL_Renderer* renderer = video.GetRenderer();
	if (renderer == nullptr) {
		return false;
	}

	const libSmackker::PlaybackResult logoResult =
		smackker.PlayWithControl(mpq, "gendata\\logo.smk", true, renderer, video.GetWindow());
	if (logoResult == libSmackker::PlaybackResult::Quit || logoResult == libSmackker::PlaybackResult::Error) {
		return false;
	}

	const libSmackker::PlaybackResult diabloResult =
		smackker.PlayWithControl(mpq, "gendata\\diablo1.smk", true, renderer, video.GetWindow());
	if (diabloResult == libSmackker::PlaybackResult::Quit || diabloResult == libSmackker::PlaybackResult::Error) {
		return false;
	}

	return true;
}
}

void Game::UpdateIntroState(double dt)
{
	if (!introDone_) {
		if (introSequenceMode_ == IntroSequenceMode::ReplayDiabloOnly) {
			SDL_Renderer* renderer = video_.GetRenderer();
			if (renderer == nullptr) {
				EnterState(State::Exiting);
				return;
			}

			const libSmackker::PlaybackResult replayResult =
			smackker_.PlayWithControl(mpq_, "gendata\\diablo1.smk", true, renderer, video_.GetWindow());
			if (replayResult == libSmackker::PlaybackResult::Quit || replayResult == libSmackker::PlaybackResult::Error) {
				EnterState(State::Exiting);
				return;
			}

			introDone_ = true;
			introSequenceMode_ = IntroSequenceMode::Startup;
			EnterState(State::MainMenu);
			resetFrameTimer_ = true;
			return;
		}

		if (!PlayIntroSequence(smackker_, mpq_, video_)) {
			EnterState(State::Exiting);
			return;
		}

		introDone_ = true;
		if (!LoadSharedFrontendAssets()) {
			EnterState(State::Exiting);
			return;
		}

		titlePresentationActive_ = true;
		titlePresentationTimeSeconds_ = 0.0;
		logoRenderer_.Reset();
		menuMusicPlaying_ = menuMusic_.PlayMusic(true);
		resetFrameTimer_ = true;
		return;
	}

	if (!titlePresentationActive_) {
		return;
	}

	menuMusic_.Update();
	titlePresentationTimeSeconds_ += dt;
	logoRenderer_.Update(dt);

	if (titlePresentationTimeSeconds_ >= kTitleScreenDurationSeconds) {
		EnterState(State::MainMenu);
	}
}

bool Game::RenderIntroState()
{
	if (!titlePresentationActive_) {
		return true;
	}

	if (titleImage_.empty() || titleWidth_ <= 0 || titleHeight_ <= 0 ||
	    logoImage_.empty() || logoWidth_ <= 0 || logoHeight_ <= 0) {
		return true;
	}

	SDL_Renderer* renderer = video_.GetRenderer();
	if (renderer == nullptr) {
		return false;
	}

	double scaleX = static_cast<double>(windowWidth_) / titleWidth_;
	double scaleY = static_cast<double>(windowHeight_) / titleHeight_;
	int titleRenderX = 0;
	int titleRenderY = 0;
	int titleRenderWidth = windowWidth_;
	int titleRenderHeight = windowHeight_;

	if (kPreserveTitleAspectRatio) {
		const double uniformScale = std::min(scaleX, scaleY);
		scaleX = uniformScale;
		scaleY = uniformScale;
		titleRenderWidth = static_cast<int>(titleWidth_ * uniformScale);
		titleRenderHeight = static_cast<int>(titleHeight_ * uniformScale);
		titleRenderX = (windowWidth_ - titleRenderWidth) / 2;
		titleRenderY = (windowHeight_ - titleRenderHeight) / 2;
	}

	if (!video_.RenderPCXImageAt(
		titleImage_.data(), titleWidth_, titleHeight_,
		titleRenderX, titleRenderY,
		titleRenderWidth, titleRenderHeight)) {
		return false;
	}

	if (menuFontLoaded_) {
		const float textScale = static_cast<float>(scaleY);
		const std::string promptText = "Copyright © 1996-2001 Blizzard Entertainment";
		const int textWidth = menuFont_.MeasureTextWidth(promptText, textScale);
		const int textX = titleRenderX + ((titleRenderWidth - textWidth) / 2);
		const int textY = titleRenderY + static_cast<int>(titleRenderHeight * 0.86);
		if (!menuFont_.RenderText(renderer, promptText, textX, textY, textScale)) {
			return false;
		}
	}

	if (!logoRenderer_.RenderCenteredWithNativeOffset(
			video_,
			logoImage_,
			logoWidth_,
			logoHeight_,
			scaleX,
			scaleY,
			titleRenderX,
			titleRenderY,
			titleRenderWidth,
			titleRenderHeight,
			kLogoOffsetYNative)) {
		return false;
	}

	return SDL_RenderPresent(renderer);
}