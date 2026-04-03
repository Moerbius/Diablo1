#include "graphics/cel_animation.hpp"

#include <utility>

void CELAnimation::SetFrames(std::vector<std::vector<std::uint32_t>> frames,
	int width,
	int height,
	double frameDurationSeconds,
	bool loop)
{
	frames_ = std::move(frames);
	width_ = width;
	height_ = height;
	frameDurationSeconds_ = frameDurationSeconds;
	loop_ = loop;
	Reset();
}

void CELAnimation::Clear()
{
	frames_.clear();
	width_ = 0;
	height_ = 0;
	frameDurationSeconds_ = 0.0;
	frameTimeSeconds_ = 0.0;
	currentFrameIndex_ = 0;
	loop_ = true;
}

void CELAnimation::Reset()
{
	frameTimeSeconds_ = 0.0;
	currentFrameIndex_ = 0;
}

void CELAnimation::Update(double dt)
{
	if (frames_.size() <= 1 || frameDurationSeconds_ <= 0.0) {
		return;
	}

	frameTimeSeconds_ += dt;
	while (frameTimeSeconds_ >= frameDurationSeconds_) {
		frameTimeSeconds_ -= frameDurationSeconds_;
		if (currentFrameIndex_ + 1 < frames_.size()) {
			++currentFrameIndex_;
		} else if (loop_) {
			currentFrameIndex_ = 0;
		} else {
			currentFrameIndex_ = frames_.size() - 1;
			frameTimeSeconds_ = 0.0;
			break;
		}
	}
}

bool CELAnimation::HasFrame() const
{
	return !frames_.empty() && width_ > 0 && height_ > 0;
}

const std::vector<std::uint32_t>& CELAnimation::GetCurrentFrame() const
{
	static const std::vector<std::uint32_t> kEmptyFrame;
	if (frames_.empty()) {
		return kEmptyFrame;
	}

	return frames_[currentFrameIndex_];
}

int CELAnimation::GetWidth() const
{
	return width_;
}

int CELAnimation::GetHeight() const
{
	return height_;
}