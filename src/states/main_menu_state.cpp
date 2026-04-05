#include "game.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
namespace {
constexpr bool kPreserveTitleAspectRatio = true;
}

void Game::UpdateMainMenuState(double dt)
{
	menuMusic_.Update();
	logoRenderer_.Update(dt);
	focusRenderer_.Update(dt);
}

bool Game::RenderMainMenuState()
{
	if (mainMenuImage_.empty() || mainMenuWidth_ <= 0 || mainMenuHeight_ <= 0) {
		return true;
	}

	SDL_Renderer* renderer = video_.GetRenderer();
	if (renderer == nullptr) {
		return false;
	}

	double scaleX = static_cast<double>(windowWidth_) / mainMenuWidth_;
	double scaleY = static_cast<double>(windowHeight_) / mainMenuHeight_;
	double uniformScale = std::min(scaleX, scaleY);
	int renderX = 0;
	int renderY = 0;
	int renderWidth = windowWidth_;
	int renderHeight = windowHeight_;

	if (kPreserveTitleAspectRatio) {
		renderWidth = static_cast<int>(mainMenuWidth_ * uniformScale);
		renderHeight = static_cast<int>(mainMenuHeight_ * uniformScale);
		renderX = (windowWidth_ - renderWidth) / 2;
		renderY = (windowHeight_ - renderHeight) / 2;
	}

	if (!video_.RenderPCXImageAt(
		mainMenuImage_.data(), mainMenuWidth_, mainMenuHeight_,
		renderX, renderY,
		renderWidth, renderHeight)) {
		return false;
	}

	// Render scaled-down animated logo over the menu background
	const double logoScale = 0.7;
	int scaledLogoHeight = 0;
	if (!logoRenderer_.RenderTopCentered(
			video_,
			logoImage_,
			logoWidth_,
			logoHeight_,
			uniformScale,
			logoScale,
			renderX,
			renderY,
			renderWidth,
			&scaledLogoHeight)) {
		return false;
	}

	// Render menu options below the logo.
	if (menuButtonFontLoaded_) {
		const float textScale = static_cast<float>(uniformScale);
		const std::array<const char*, 5> menuEntries = {
			"Single Player",
			"Multi Player",
			"Replay Intro",
			"Show Savegame",
			"Exit Diablo"
		};

		const int lineHeight = std::max(1, menuButtonFont_.GetLineHeight(textScale));
		const int firstLineY = renderY + scaledLogoHeight + lineHeight + static_cast<int>(20 * uniformScale);
		const int selectedIndex = std::clamp(mainMenuSelectionIndex_, 0, static_cast<int>(menuEntries.size()) - 1);
		int selectedTextX = 0;
		int selectedTextY = 0;
		int selectedTextWidth = 0;

		for (std::size_t i = 0; i < menuEntries.size(); ++i) {
			const int textWidth = menuButtonFont_.MeasureTextWidth(menuEntries[i], textScale);
			const int textX = (windowWidth_ - textWidth) / 2;
			const int textY = firstLineY + static_cast<int>(i) * lineHeight;
			if (static_cast<int>(i) == selectedIndex) {
				selectedTextX = textX;
				selectedTextY = textY;
				selectedTextWidth = textWidth;
			}
			if (!menuButtonFont_.RenderText(renderer, menuEntries[i], textX, textY, textScale)) {
				return false;
			}
		}

		const FocusAtlas focusAtlas = FocusAtlas::Focus42;
		const std::vector<std::uint32_t>* focusImage = nullptr;
		int focusWidth = 0;
		int focusHeight = 0;
		int focusFrameCount = 0;
		if (GetFocusAtlas(focusAtlas, focusImage, focusWidth, focusHeight, focusFrameCount) &&
			focusImage != nullptr && !focusImage->empty() && focusWidth > 0 && focusHeight > 0 && focusFrameCount > 0) {
			const int markerPadding = std::max(1, menuButtonFont_.MeasureTextWidth("    ", textScale));
			if (!focusRenderer_.RenderPairAroundText(
					video_,
					*focusImage,
					focusWidth,
					focusHeight,
					focusFrameCount,
					uniformScale,
					selectedTextX,
					selectedTextY,
					selectedTextWidth,
					lineHeight,
					markerPadding)) {
				return false;
			}
		}
	}

	return SDL_RenderPresent(renderer);
}