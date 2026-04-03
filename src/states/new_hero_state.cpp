#include "game.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <vector>

namespace {
constexpr bool kPreserveTitleAspectRatio = true;
}

void Game::UpdateNewHeroState(double dt)
{
	// Animate logo in new hero state (15 frames at ~20 FPS)
	logoAnimationTime_ += dt;
	const double frameTime = 0.05;
	currentLogoFrame_ = static_cast<int>(logoAnimationTime_ / frameTime) % 15;
}

bool Game::RenderNewHeroState()
{
	if (selheroImage_.empty() || selheroWidth_ <= 0 || selheroHeight_ <= 0) {
		return true;
	}

	SDL_Renderer* renderer = video_.GetRenderer();
	if (renderer == nullptr) {
		return false;
	}

	double scaleX = static_cast<double>(windowWidth_) / selheroWidth_;
	double scaleY = static_cast<double>(windowHeight_) / selheroHeight_;
	int renderX = 0;
	int renderY = 0;
	int renderWidth = windowWidth_;
	int renderHeight = windowHeight_;

	if (kPreserveTitleAspectRatio) {
		const double uniformScale = std::min(scaleX, scaleY);
		renderWidth = static_cast<int>(selheroWidth_ * uniformScale);
		renderHeight = static_cast<int>(selheroHeight_ * uniformScale);
		renderX = (windowWidth_ - renderWidth) / 2;
		renderY = (windowHeight_ - renderHeight) / 2;
	}

	if (!video_.RenderPCXImageAt(
		selheroImage_.data(), selheroWidth_, selheroHeight_,
		renderX, renderY,
		renderWidth, renderHeight)) {
		return false;
	}

	// Render scaled-down animated logo over the background
	double uniformScale = std::min(scaleX, scaleY);
	const double logoScale = 0.7;
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

		// Scale logo to roughly 0.6x the background size and position at top
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

	// Render "New Single Player Hero" text below logo
	if (heroCreationFontLoaded_) {
		const float textScale = static_cast<float>(uniformScale);
		const char* heroText = "New Single Player Hero";
		const int textWidth = heroCreationFont_.MeasureTextWidth(heroText, textScale);
		const int textX = (windowWidth_ - textWidth) / 2;
		const int logoFrameHeight = logoHeight_ / 15;
		const int scaledLogoHeight = static_cast<int>(logoFrameHeight * uniformScale * logoScale);
		const int textY = renderY + scaledLogoHeight + static_cast<int>(5 * uniformScale);

		if (!heroCreationFont_.RenderText(renderer, heroText, textX, textY, textScale)) {
			return false;
		}

		// Place class prompt inside the right panel of the 640x480 selhero layout.
		const char* chooseClassText = "Choose Class";
		const int chooseClassWidth = heroCreationFont_.MeasureTextWidth(chooseClassText, textScale);
		const int rightPanelCenterXNative = 425;
		const int rightPanelTitleYNative = 210;
		const int chooseClassX = renderX + static_cast<int>(rightPanelCenterXNative * uniformScale) - (chooseClassWidth / 2);
		const int chooseClassY = renderY + static_cast<int>(rightPanelTitleYNative * uniformScale);

		if (!heroCreationFont_.RenderText(renderer, chooseClassText, chooseClassX, chooseClassY, textScale)) {
			return false;
		}

		const int lineHeight = std::max(1, heroCreationFont_.GetLineHeight(textScale));
		const int classStartY = chooseClassY + (lineHeight * 2) + static_cast<int>(10 * uniformScale); // one blank line after heading + slight downward nudge
		const int classCenterX = renderX + static_cast<int>(rightPanelCenterXNative * uniformScale);
		const char* classNames[] = { "Warrior", "Rogue", "Sorcerer" };

		for (int i = 0; i < 3; ++i) {
			const int classTextWidth = heroClassFontLoaded_
				? heroClassFont_.MeasureTextWidth(classNames[i], textScale)
				: heroCreationFont_.MeasureTextWidth(classNames[i], textScale);
			const int classTextX = classCenterX - (classTextWidth / 2);
			const int classTextY = classStartY + (i * lineHeight);
			const bool classRenderOk = heroClassFontLoaded_
				? heroClassFont_.RenderText(renderer, classNames[i], classTextX, classTextY, textScale)
				: heroCreationFont_.RenderText(renderer, classNames[i], classTextX, classTextY, textScale);
			if (!classRenderOk) {
				return false;
			}
		}
	}

	SDL_RenderPresent(renderer);

	return true;
}
