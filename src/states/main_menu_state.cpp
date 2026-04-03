#include "game.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <vector>

namespace {
constexpr bool kPreserveTitleAspectRatio = false;
}

void Game::UpdateMainMenuState(double dt)
{
	menuMusic_.Update();
	
	// Animate logo in main menu (15 frames at ~20 FPS)
	logoAnimationTime_ += dt;
	const double frameTime = 0.05;
	currentLogoFrame_ = static_cast<int>(logoAnimationTime_ / frameTime) % 15;
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
	int renderX = 0;
	int renderY = 0;
	int renderWidth = windowWidth_;
	int renderHeight = windowHeight_;

	if (kPreserveTitleAspectRatio) {
		const double uniformScale = std::min(scaleX, scaleY);
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
	double uniformScale = std::min(scaleX, scaleY);
	const double logoScale = 0.6;
	if (!logoImage_.empty() && logoWidth_ > 0 && logoHeight_ > 0) {
		const int logoFrameHeight = logoHeight_ / 15;
		const int logoFrameY = currentLogoFrame_ * logoFrameHeight;
		std::vector<std::uint32_t> logoFrame;
		logoFrame.reserve(logoWidth_ * logoFrameHeight);

		for (int y = 0; y < logoFrameHeight; ++y) {
			for (int x = 0; x < logoWidth_; ++x) {
				logoFrame.push_back(logoImage_[(logoFrameY + y) * logoWidth_ + x]);
			}
		}

		// Scale logo to roughly 0.6x the menu background size and position at top
		// Use uniform scale to maintain aspect ratio
		const int scaledLogoWidth = static_cast<int>(logoWidth_ * uniformScale * logoScale);
		const int scaledLogoHeight = static_cast<int>(logoFrameHeight * uniformScale * logoScale);
		const int logoX = renderX + ((renderWidth - scaledLogoWidth) / 2);
		const int logoY = renderY;  // Top of screen

		if (!video_.RenderLogoScaled(
			logoFrame.data(), logoWidth_, logoFrameHeight,
			scaledLogoWidth, scaledLogoHeight,
			logoX, logoY)) {
			return false;
		}
	}

	// Render menu options below the logo.
	if (menuButtonFontLoaded_) {
		const float textScale = 1.0f;
		const std::array<const char*, 5> menuEntries = {
			"Single Player",
			"Multi Player",
			"Replay Intro",
			"Show Credits",
			"Exit Diablo"
		};

		const int logoFrameHeight = logoHeight_ / 15;
		const int scaledLogoHeight = static_cast<int>(logoFrameHeight * uniformScale * logoScale);
		const int lineHeight = std::max(1, menuButtonFont_.GetLineHeight(textScale));
		const int firstLineY = renderY + scaledLogoHeight + lineHeight + 20;
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

		if (!focus42Image_.empty() && focus42Width_ > 0 && focus42Height_ > 0) {
			constexpr int kFocusFrameCount = 8;
			const int focusFrame = static_cast<int>(logoAnimationTime_ / 0.05) % kFocusFrameCount;

			int focusFrameWidth = 0;
			int focusFrameHeight = 0;
			int focusFrameX = 0;
			int focusFrameY = 0;

			if ((focus42Height_ % kFocusFrameCount) == 0) {
				focusFrameWidth = focus42Width_;
				focusFrameHeight = focus42Height_ / kFocusFrameCount;
				focusFrameX = 0;
				focusFrameY = focusFrame * focusFrameHeight;
			} else if ((focus42Width_ % kFocusFrameCount) == 0) {
				focusFrameWidth = focus42Width_ / kFocusFrameCount;
				focusFrameHeight = focus42Height_;
				focusFrameX = focusFrame * focusFrameWidth;
				focusFrameY = 0;
			}

			if (focusFrameWidth > 0 && focusFrameHeight > 0) {
				std::vector<std::uint32_t> focusFramePixels;
				focusFramePixels.reserve(focusFrameWidth * focusFrameHeight);
				for (int y = 0; y < focusFrameHeight; ++y) {
					for (int x = 0; x < focusFrameWidth; ++x) {
						const int srcX = focusFrameX + x;
						const int srcY = focusFrameY + y;
						focusFramePixels.push_back(focus42Image_[srcY * focus42Width_ + srcX]);
					}
				}

				const int markerY = selectedTextY + ((lineHeight - focusFrameHeight) / 2);
				const int markerPadding = std::max(1, menuButtonFont_.MeasureTextWidth("    ", textScale));
				const int leftMarkerX = selectedTextX - focusFrameWidth - markerPadding;
				const int rightMarkerX = selectedTextX + selectedTextWidth + markerPadding;

				if (!video_.RenderLogoScaled(
					focusFramePixels.data(), focusFrameWidth, focusFrameHeight,
					focusFrameWidth, focusFrameHeight,
					leftMarkerX, markerY)) {
					return false;
				}

				if (!video_.RenderLogoScaled(
					focusFramePixels.data(), focusFrameWidth, focusFrameHeight,
					focusFrameWidth, focusFrameHeight,
					rightMarkerX, markerY)) {
					return false;
				}
			}
		}
	}

	return SDL_RenderPresent(renderer);
}