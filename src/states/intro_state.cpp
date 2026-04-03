#include "game.hpp"

#include "pcx.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <string>
#include <vector>

namespace {
constexpr bool kPreserveTitleAspectRatio = false;
constexpr int kLogoOffsetYNative = 48;
constexpr double kTitleScreenDurationSeconds = 5.0;

bool PlayIntroSequence(libSmackker& smackker, StormLib& mpq, Video& video)
{
	SDL_Renderer* renderer = video.GetRenderer();
	if (renderer == nullptr) {
		return false;
	}

	const libSmackker::PlaybackResult logoResult =
		smackker.PlayWithControl(mpq, "gendata\\logo.smk", true, renderer);
	if (logoResult == libSmackker::PlaybackResult::Quit || logoResult == libSmackker::PlaybackResult::Error) {
		return false;
	}

	const libSmackker::PlaybackResult diabloResult =
		smackker.PlayWithControl(mpq, "gendata\\diablo1.smk", true, renderer);
	if (diabloResult == libSmackker::PlaybackResult::Quit || diabloResult == libSmackker::PlaybackResult::Error) {
		return false;
	}

	return true;
}

bool LoadTitleScreen(
	StormLib& mpq,
	std::vector<std::uint32_t>& titleImage,
	std::vector<std::uint32_t>& logoImage,
	std::vector<std::uint32_t>& mainMenuImage,
	std::vector<std::uint32_t>& focus42Image,
	int& titleWidth,
	int& titleHeight,
	int& logoWidth,
	int& logoHeight,
	int& mainMenuWidth,
	int& mainMenuHeight,
	int& focus42Width,
	int& focus42Height)
{
	PCXImage titlePcx = PCX::LoadFromMPQ(mpq, "ui_art\\title.pcx");
	PCXImage logoPcx = PCX::LoadFromMPQ(mpq, "ui_art\\logo.pcx");
	PCXImage mainMenuPcx = PCX::LoadFromMPQ(mpq, "ui_art\\mainmenu.pcx");
	PCXImage focus42Pcx = PCX::LoadFromMPQ(mpq, "ui_art\\focus42.pcx");

	if (titlePcx.pixels.empty() || logoPcx.pixels.empty() || mainMenuPcx.pixels.empty()) {
		return false;
	}

	titleWidth = static_cast<int>(titlePcx.width);
	titleHeight = static_cast<int>(titlePcx.height);
	logoWidth = static_cast<int>(logoPcx.width);
	logoHeight = static_cast<int>(logoPcx.height);
	mainMenuWidth = static_cast<int>(mainMenuPcx.width);
	mainMenuHeight = static_cast<int>(mainMenuPcx.height);

	titleImage = PCX::ConvertToRGBA32(titlePcx);
	logoImage = PCX::ConvertToRGBA32(logoPcx);
	mainMenuImage = PCX::ConvertToRGBA32(mainMenuPcx);

	if (!focus42Pcx.pixels.empty() && focus42Pcx.width > 0 && focus42Pcx.height > 0) {
		focus42Width = static_cast<int>(focus42Pcx.width);
		focus42Height = static_cast<int>(focus42Pcx.height);
		focus42Image = PCX::ConvertToRGBA32(focus42Pcx);
	} else {
		focus42Width = 0;
		focus42Height = 0;
		focus42Image.clear();
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
				smackker_.PlayWithControl(mpq_, "gendata\\diablo1.smk", true, renderer);
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
		if (!LoadTitleScreen(
				mpq_,
				titleImage_,
				logoImage_,
				mainMenuImage_,
				focus42Image_,
				titleWidth_,
				titleHeight_,
				logoWidth_,
				logoHeight_,
				mainMenuWidth_,
				mainMenuHeight_,
				focus42Width_,
				focus42Height_)) {
			EnterState(State::Exiting);
			return;
		}

		titlePresentationActive_ = true;
		titlePresentationTimeSeconds_ = 0.0;
		logoAnimationTime_ = 0.0;
		currentLogoFrame_ = 0;
		menuMusic_.PlayMusic(true);
		resetFrameTimer_ = true;
		return;
	}

	if (!titlePresentationActive_) {
		return;
	}

	menuMusic_.Update();
	titlePresentationTimeSeconds_ += dt;
	logoAnimationTime_ += dt;
	const double frameTime = 0.05;
	currentLogoFrame_ = static_cast<int>(logoAnimationTime_ / frameTime) % 15;

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

	const int logoFrameHeight = logoHeight_ / 15;
	const int logoFrameY = currentLogoFrame_ * logoFrameHeight;
	std::vector<std::uint32_t> logoFrame;
	logoFrame.reserve(logoWidth_ * logoFrameHeight);

	for (int y = 0; y < logoFrameHeight; ++y) {
		for (int x = 0; x < logoWidth_; ++x) {
			logoFrame.push_back(logoImage_[(logoFrameY + y) * logoWidth_ + x]);
		}
	}

	const int scaledLogoWidth = static_cast<int>(logoWidth_ * scaleX);
	const int scaledLogoHeight = static_cast<int>(logoFrameHeight * scaleY);
	const int logoX = titleRenderX + ((titleRenderWidth - scaledLogoWidth) / 2);
	const int logoOffsetY = static_cast<int>(kLogoOffsetYNative * scaleY);
	const int logoY = titleRenderY + ((titleRenderHeight - scaledLogoHeight) / 2) + logoOffsetY;

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

	if (!video_.RenderLogoScaled(
		logoFrame.data(), logoWidth_, logoFrameHeight,
		scaledLogoWidth, scaledLogoHeight,
		logoX, logoY)) {
		return false;
	}

	return SDL_RenderPresent(renderer);
}