#include "game.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <vector>

namespace {
constexpr bool kPreserveTitleAspectRatio = true;
}

void Game::UpdateSelectHeroState(double dt)
{
	// Animate logo in select hero state (15 frames at ~20 FPS)
	logoAnimationTime_ += dt;
	const double frameTime = 0.05;
	currentLogoFrame_ = static_cast<int>(logoAnimationTime_ / frameTime) % 15;
}

bool Game::RenderSelectHeroState()
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

	SDL_RenderPresent(renderer);

	return true;
}
