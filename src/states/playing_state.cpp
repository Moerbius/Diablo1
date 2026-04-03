#include "game.hpp"

void Game::UpdatePlayingState(double dt)
{
	if (state_ == State::Paused && stateTimeSeconds_ > 1.0) {
		EnterState(State::Playing);
		return;
	}

	if (state_ != State::Playing) {
		return;
	}

	rectX_ += rectVelocityX_ * dt;
	rectY_ += rectVelocityY_ * dt;

	const double maxX = static_cast<double>(windowWidth_ - rectSize_);
	const double maxY = static_cast<double>(windowHeight_ - rectSize_);

	if (rectX_ < 0.0) {
		rectX_ = 0.0;
		rectVelocityX_ = -rectVelocityX_;
	} else if (rectX_ > maxX) {
		rectX_ = maxX;
		rectVelocityX_ = -rectVelocityX_;
	}

	if (rectY_ < 0.0) {
		rectY_ = 0.0;
		rectVelocityY_ = -rectVelocityY_;
	} else if (rectY_ > maxY) {
		rectY_ = maxY;
		rectVelocityY_ = -rectVelocityY_;
	}

	if (stateTimeSeconds_ > 8.0) {
		EnterState(State::Paused);
	}
}