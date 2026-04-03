#include "game.hpp"

#include <SDL3/SDL.h>

#include <algorithm>

namespace {
constexpr bool kPreserveTitleAspectRatio = true;
}

void Game::UpdateSelectHeroState(double dt)
{
	logoRenderer_.Update(dt);
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
	if (!logoRenderer_.RenderTopCentered(
			video_,
			logoImage_,
			logoWidth_,
			logoHeight_,
			uniformScale,
			logoScale,
			renderX,
			renderY,
			renderWidth)) {
		return false;
	}

	SDL_RenderPresent(renderer);

	return true;
}
