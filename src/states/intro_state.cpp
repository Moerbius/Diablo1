#include "game.hpp"

#include "graphics/cel.hpp"
#include "graphics/pal.hpp"
#include "pcx.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <string>
#include <vector>

namespace {
constexpr bool kPreserveTitleAspectRatio = true;
constexpr int kLogoOffsetYNative = 48;
constexpr double kTitleScreenDurationSeconds = 5.0;
constexpr double kMagballFrameDurationSeconds = 0.12;

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

bool LoadTitleScreen(
	StormLib& mpq,
	std::vector<std::uint32_t>& titleImage,
	std::vector<std::uint32_t>& logoImage,
	std::vector<std::uint32_t>& mainMenuImage,
	std::vector<std::uint32_t>& selheroImage,
	std::vector<std::uint32_t>& herosImage,
	std::vector<std::uint32_t>& smallPortraitImage,
	std::vector<std::uint32_t>& focusImage,
	std::vector<std::uint32_t>& focus16Image,
	std::vector<std::uint32_t>& focus42Image,
	CELAnimation& magballAnimation,
	int& titleWidth,
	int& titleHeight,
	int& logoWidth,
	int& logoHeight,
	int& mainMenuWidth,
	int& mainMenuHeight,
	int& selheroWidth,
	int& selheroHeight,
	int& herosWidth,
	int& herosHeight,
	int& smallPortraitWidth,
	int& smallPortraitHeight,
	int& focusWidth,
	int& focusHeight,
	int& focus16Width,
	int& focus16Height,
	int& focus42Width,
	int& focus42Height)
{
	PCXImage titlePcx = PCX::LoadFromMPQ(mpq, "ui_art\\title.pcx");
	PCXImage logoPcx = PCX::LoadFromMPQ(mpq, "ui_art\\logo.pcx");
	PCXImage mainMenuPcx = PCX::LoadFromMPQ(mpq, "ui_art\\mainmenu.pcx");
	PCXImage selheropcx = PCX::LoadFromMPQ(mpq, "ui_art\\selhero.pcx");
	PCXImage herosPcx = PCX::LoadFromMPQ(mpq, "ui_art\\heros.pcx");
	PCXImage smallPortraitPcx = PCX::LoadFromMPQ(mpq, "ui_art\\heroport.pcx");
	PCXImage focusPcx = PCX::LoadFromMPQ(mpq, "ui_art\\focus.pcx");
	PCXImage focus16Pcx = PCX::LoadFromMPQ(mpq, "ui_art\\focus16.pcx");
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

	if (!selheropcx.pixels.empty() && selheropcx.width > 0 && selheropcx.height > 0) {
		selheroWidth = static_cast<int>(selheropcx.width);
		selheroHeight = static_cast<int>(selheropcx.height);
		selheroImage = PCX::ConvertToRGBA32(selheropcx);
	} else {
		selheroWidth = 0;
		selheroHeight = 0;
		selheroImage.clear();
	}

	if (!herosPcx.pixels.empty() && herosPcx.width > 0 && herosPcx.height > 0) {
		herosWidth = static_cast<int>(herosPcx.width);
		herosHeight = static_cast<int>(herosPcx.height);
		herosImage = PCX::ConvertToRGBA32(herosPcx);
	} else {
		herosWidth = 0;
		herosHeight = 0;
		herosImage.clear();
	}

	if (!smallPortraitPcx.pixels.empty() && smallPortraitPcx.width > 0 && smallPortraitPcx.height > 0) {
		smallPortraitWidth = static_cast<int>(smallPortraitPcx.width);
		smallPortraitHeight = static_cast<int>(smallPortraitPcx.height);
		smallPortraitImage = PCX::ConvertToRGBA32(smallPortraitPcx);
	} else {
		smallPortraitWidth = 0;
		smallPortraitHeight = 0;
		smallPortraitImage.clear();
	}

	if (!focusPcx.pixels.empty() && focusPcx.width > 0 && focusPcx.height > 0) {
		focusWidth = static_cast<int>(focusPcx.width);
		focusHeight = static_cast<int>(focusPcx.height);
		focusImage = PCX::ConvertToRGBA32(focusPcx);
	} else {
		focusWidth = 0;
		focusHeight = 0;
		focusImage.clear();
	}

	if (!focus16Pcx.pixels.empty() && focus16Pcx.width > 0 && focus16Pcx.height > 0) {
		focus16Width = static_cast<int>(focus16Pcx.width);
		focus16Height = static_cast<int>(focus16Pcx.height);
		focus16Image = PCX::ConvertToRGBA32(focus16Pcx);
	} else {
		focus16Width = 0;
		focus16Height = 0;
		focus16Image.clear();
	}

	if (!focus42Pcx.pixels.empty() && focus42Pcx.width > 0 && focus42Pcx.height > 0) {
		focus42Width = static_cast<int>(focus42Pcx.width);
		focus42Height = static_cast<int>(focus42Pcx.height);
		focus42Image = PCX::ConvertToRGBA32(focus42Pcx);
	} else {
		focus42Width = 0;
		focus42Height = 0;
		focus42Image.clear();
	}

	CELImage magballCel = CEL::LoadFromMPQ(mpq, "monsters\\magma\\magball1.cel");
	std::vector<std::uint8_t> l1Palette = PAL::LoadFromMPQ(mpq, "levels\\l1data\\l1.pal");
	magballAnimation.Clear();

	if (!magballCel.frames.empty()) {
		const CELFrame& firstFrame = magballCel.frames[0];
		const int magballWidth = static_cast<int>(firstFrame.width);
		const int magballHeight = static_cast<int>(firstFrame.height);
		std::vector<std::vector<std::uint32_t>> magballFrames;
		magballFrames.reserve(magballCel.frames.size());
		if (!l1Palette.empty()) {
			for (const CELFrame& frame : magballCel.frames) {
				magballFrames.push_back(CEL::ConvertFrameToRGBA32(frame, l1Palette, 0));
			}
		} else {
			for (const CELFrame& frame : magballCel.frames) {
				magballFrames.push_back(CEL::ConvertFrameToRGBA32(frame, mainMenuPcx.palette, 0));
			}
		}
		magballAnimation.SetFrames(std::move(magballFrames), magballWidth, magballHeight,
			kMagballFrameDurationSeconds);
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
		if (!LoadTitleScreen(
				mpq_,
				titleImage_,
				logoImage_,
				mainMenuImage_,
				selheroImage_,
				herosImage_,
				smallPortraitImage_,
				focusImage_,
				focus16Image_,
				focus42Image_,
				magballAnimation_,
				titleWidth_,
				titleHeight_,
				logoWidth_,
				logoHeight_,
				mainMenuWidth_,
				mainMenuHeight_,
				selheroWidth_,
				selheroHeight_,
				herosWidth_,
				herosHeight_,
				smallPortraitWidth_,
				smallPortraitHeight_,
				focusWidth_,
				focusHeight_,
				focus16Width_,
				focus16Height_,
				focus42Width_,
				focus42Height_)) {
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